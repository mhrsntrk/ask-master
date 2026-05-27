package main

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log/slog"
	"net"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/gorilla/websocket"

	"github.com/mhrsntrk/ask-master/internal/truncate"
)

var errBridgeDisconnected = errors.New("cardputer disconnected before reply")
var errDeviceOffline = errors.New("cardputer offline — not seen via UDP beacon recently")

const (
	udpBeaconPort     = 8766
	presenceTimeout   = 2 * time.Minute
	wakeTimeout       = 10 * time.Second
	wakePacket        = "ask-master-wake"
	beaconPacket      = "ask-master-ping"
)

type DevicePresence struct {
	mu       sync.RWMutex
	lastIP   string
	lastSeen time.Time
}

func (dp *DevicePresence) Update(ip string) {
	dp.mu.Lock()
	defer dp.mu.Unlock()
	dp.lastIP = ip
	dp.lastSeen = time.Now()
}

func (dp *DevicePresence) IsOnline() bool {
	dp.mu.RLock()
	defer dp.mu.RUnlock()
	return dp.lastIP != "" && time.Since(dp.lastSeen) < presenceTimeout
}

func (dp *DevicePresence) GetIP() string {
	dp.mu.RLock()
	defer dp.mu.RUnlock()
	return dp.lastIP
}

type pendingQuestion struct {
	payload      string
	questionType string
	options      []string
	replyCh      chan string
	errCh        chan error
	totalTimer   *time.Timer
	done         chan struct{}
	answered     bool
}

type Bridger interface {
	Connected() bool
	DeviceOnline() bool
	SendAndWait(payload string, questionType string, options []string, timeout time.Duration) (string, error)
}

type Bridge struct {
	mu             sync.Mutex
	writeMu        sync.Mutex
	stateMu        sync.Mutex
	conn           *websocket.Conn
	queue          []*pendingQuestion
	current        *pendingQuestion
	shutdownCtx    context.Context
	shutdownCancel context.CancelFunc
	logger         *slog.Logger
	server         *http.Server
	upgrader       websocket.Upgrader
	presence       *DevicePresence
	udpConn        *net.UDPConn
}

func NewBridge(logger *slog.Logger) *Bridge {
	if logger == nil {
		logger = slog.Default()
	}

	shutdownCtx, shutdownCancel := context.WithCancel(context.Background())

	b := &Bridge{
		shutdownCtx:    shutdownCtx,
		shutdownCancel: shutdownCancel,
		logger:         logger,
		presence:       &DevicePresence{},
		upgrader: websocket.Upgrader{
			CheckOrigin: func(r *http.Request) bool { return true },
			ReadBufferSize:  1024,
			WriteBufferSize: 1024,
		},
	}

	go b.queueProcessor()
	return b
}

func (b *Bridge) Connected() bool {
	b.stateMu.Lock()
	defer b.stateMu.Unlock()
	return b.conn != nil
}

func (b *Bridge) DeviceOnline() bool {
	return b.presence.IsOnline()
}

func (b *Bridge) SendAndWait(payload string, questionType string, options []string, timeout time.Duration) (string, error) {
	if !b.presence.IsOnline() {
		return "", errDeviceOffline
	}

	replyCh := make(chan string, 1)
	errCh := make(chan error, 1)

	pq := &pendingQuestion{
		payload:      payload,
		questionType: questionType,
		options:      append([]string(nil), options...),
		replyCh:      replyCh,
		errCh:        errCh,
		totalTimer:   time.NewTimer(timeout),
		done:         make(chan struct{}),
	}

	b.mu.Lock()
	b.queue = append(b.queue, pq)
	b.mu.Unlock()

	// Notify queue processor that a new item is available
	b.wakeDevice()
	defer pq.totalTimer.Stop()

	select {
	case reply, ok := <-replyCh:
		if !ok {
			return "", errBridgeDisconnected
		}
		return reply, nil
	case err := <-errCh:
		return "", err
	case <-pq.totalTimer.C:
		b.removeQuestion(pq)
		return "", context.DeadlineExceeded
	case <-b.shutdownCtx.Done():
		b.removeQuestion(pq)
		return "", b.shutdownCtx.Err()
	}
}

func (b *Bridge) queueProcessor() {
	for {
		select {
		case <-b.shutdownCtx.Done():
			return
		default:
		}

		b.mu.Lock()
		if len(b.queue) == 0 {
			b.mu.Unlock()
			time.Sleep(100 * time.Millisecond)
			continue
		}

		pq := b.queue[0]
		b.current = pq
		b.mu.Unlock()

		b.processQuestion(pq)

		b.mu.Lock()
		b.current = nil
		// Remove the processed question from queue
		if len(b.queue) > 0 && b.queue[0] == pq {
			b.queue = b.queue[1:]
		}
		b.mu.Unlock()
	}
}

func (b *Bridge) processQuestion(pq *pendingQuestion) {
	deadline := time.Now().Add(wakeTimeout)
	for time.Now().Before(deadline) {
		b.stateMu.Lock()
		conn := b.conn
		b.stateMu.Unlock()

		if conn != nil {
			break
		}

		b.wakeDevice()
		time.Sleep(500 * time.Millisecond)
	}

	b.stateMu.Lock()
	conn := b.conn
	b.stateMu.Unlock()

	if conn == nil {
		select {
		case pq.errCh <- errors.New("cardputer did not connect after wake"):
		default:
		}
		return
	}

	b.writeMu.Lock()
	err := conn.WriteMessage(websocket.TextMessage, []byte(pq.payload))
	b.writeMu.Unlock()

	if err != nil {
		b.disconnectConn(conn, fmt.Errorf("write to cardputer failed: %w", err))
		select {
		case pq.errCh <- err:
		default:
		}
		return
	}

	// Block until answered, errored, or timed out
	<-pq.done
}

func (b *Bridge) removeQuestion(pq *pendingQuestion) {
	b.mu.Lock()
	defer b.mu.Unlock()

	for i, q := range b.queue {
		if q == pq {
			b.queue = append(b.queue[:i], b.queue[i+1:]...)
			close(q.done)
			break
		}
	}
}

func (b *Bridge) wakeDevice() error {
	ip := b.presence.GetIP()
	if ip == "" {
		return errors.New("no device IP known")
	}

	addr, err := net.ResolveUDPAddr("udp", ip+":"+strconv.Itoa(udpBeaconPort))
	if err != nil {
		return err
	}

	conn, err := net.DialUDP("udp", nil, addr)
	if err != nil {
		return err
	}
	defer conn.Close()

	_, err = conn.Write([]byte(wakePacket))
	return err
}

func (b *Bridge) wsHandler(w http.ResponseWriter, r *http.Request) {
	conn, err := b.upgrader.Upgrade(w, r, nil)
	if err != nil {
		b.logger.Error("websocket upgrade failed", "error", err)
		return
	}

	b.replaceConn(conn)
	b.logger.Info("device connected via websocket")

	for {
		_, msg, err := conn.ReadMessage()
		if err != nil {
			b.disconnectConn(conn, errBridgeDisconnected)
			return
		}
		b.receive(string(msg))
	}
}

func (b *Bridge) receive(msg string) {
	b.mu.Lock()
	pq := b.current
	if pq != nil {
		pq.answered = true
	}
	b.mu.Unlock()

	if pq == nil {
		return
	}

	reply := strings.TrimSpace(msg)
	if pq.questionType == "choose" {
		mapped, err := mapChooseReply(reply, pq.options)
		if err != nil {
			select {
			case pq.errCh <- err:
			default:
			}
			return
		}
		reply = mapped
	}

	select {
	case pq.replyCh <- reply:
	default:
	}

	close(pq.done)
}

func (b *Bridge) StartWS(addr string) error {
	mux := http.NewServeMux()
	mux.HandleFunc("/notify", b.notifyHandler)
	mux.HandleFunc("/", b.wsHandler)

	server := &http.Server{
		Addr:    addr,
		Handler: mux,
	}

	b.stateMu.Lock()
	b.server = server
	b.stateMu.Unlock()

	err := server.ListenAndServe()
	if errors.Is(err, http.ErrServerClosed) && b.shutdownCtx.Err() != nil {
		return nil
	}
	return err
}

func (b *Bridge) StartUDP() error {
	addr, err := net.ResolveUDPAddr("udp", ":"+strconv.Itoa(udpBeaconPort))
	if err != nil {
		return err
	}

	conn, err := net.ListenUDP("udp", addr)
	if err != nil {
		return err
	}
	b.udpConn = conn

	go b.udpListener()
	return nil
}

func (b *Bridge) udpListener() {
	buf := make([]byte, 1024)
	for {
		select {
		case <-b.shutdownCtx.Done():
			return
		default:
		}

		n, clientAddr, err := b.udpConn.ReadFromUDP(buf)
		if err != nil {
			if b.shutdownCtx.Err() != nil {
				return
			}
			b.logger.Warn("udp read error", "error", err)
			continue
		}

		msg := strings.TrimSpace(string(buf[:n]))
		if msg == beaconPacket {
			ip := clientAddr.IP.String()
			b.presence.Update(ip)
			b.logger.Debug("device beacon received", "ip", ip)
		}
	}
}

func (b *Bridge) Shutdown() error {
	b.shutdownCancel()

	b.stateMu.Lock()
	server := b.server
	conn := b.conn
	udpConn := b.udpConn
	b.stateMu.Unlock()

	b.disconnectConn(conn, context.Canceled)

	if udpConn != nil {
		_ = udpConn.Close()
	}

	if server == nil {
		return nil
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	return server.Shutdown(ctx)
}

func (b *Bridge) replaceConn(conn *websocket.Conn) {
	b.stateMu.Lock()
	previous := b.conn
	b.conn = conn
	b.stateMu.Unlock()

	if previous != nil && previous != conn {
		b.disconnectConn(previous, errBridgeDisconnected)
	}
}

func (b *Bridge) disconnectConn(conn *websocket.Conn, cause error) {
	b.stateMu.Lock()
	if conn != nil && b.conn != conn {
		b.stateMu.Unlock()
		_ = conn.Close()
		return
	}

	currentConn := b.conn
	b.conn = nil
	b.stateMu.Unlock()

	if currentConn != nil {
		_ = currentConn.Close()
	}

	b.mu.Lock()
	pq := b.current
	b.mu.Unlock()

	if pq != nil {
		if !pq.answered {
			select {
			case pq.errCh <- cause:
			default:
			}
		}
		close(pq.done)
	}
}

// Notify dispatches a fire-and-forget escalate-style alert to the device.
// Returns immediately after a presence check; delivery happens in a goroutine
// so callers (HTTP hooks) don't block on the device-reply timeout.
func (b *Bridge) Notify(message, ctxMsg string) error {
	if !b.presence.IsOnline() {
		return errDeviceOffline
	}

	payload, err := json.Marshal(map[string]any{
		"type":      "escalate",
		"question":  truncate.String(message, 120),
		"context":   truncate.String(ctxMsg, 60),
		"escalated": true,
	})
	if err != nil {
		return err
	}

	go func() {
		_, dispatchErr := b.SendAndWait(string(payload), "escalate", nil, 60*time.Second)
		if dispatchErr != nil && !errors.Is(dispatchErr, context.DeadlineExceeded) {
			b.logger.Debug("notify dispatch ended", "error", dispatchErr)
		}
	}()
	return nil
}

func (b *Bridge) notifyHandler(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "POST required", http.StatusMethodNotAllowed)
		return
	}

	host, _, err := net.SplitHostPort(r.RemoteAddr)
	if err != nil || (host != "127.0.0.1" && host != "::1" && host != "localhost") {
		http.Error(w, "forbidden: localhost only", http.StatusForbidden)
		return
	}

	var body struct {
		Message string `json:"message"`
		Context string `json:"context"`
	}
	if r.Body != nil {
		_ = json.NewDecoder(io.LimitReader(r.Body, 4096)).Decode(&body)
	}
	if body.Message == "" {
		body.Message = "Claude Code needs your attention"
	}

	if err := b.Notify(body.Message, body.Context); err != nil {
		http.Error(w, err.Error(), http.StatusServiceUnavailable)
		return
	}
	w.WriteHeader(http.StatusAccepted)
	_, _ = w.Write([]byte("ok"))
}

func mapChooseReply(reply string, options []string) (string, error) {
	index, err := strconv.Atoi(reply)
	if err != nil {
		return "", fmt.Errorf("invalid choose reply %q: %w", reply, err)
	}
	if index < 1 || index > len(options) {
		return "", fmt.Errorf("choose reply %q out of range", reply)
	}
	return options[index-1], nil
}
