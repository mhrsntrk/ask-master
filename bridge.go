package main

import (
	"context"
	"errors"
	"fmt"
	"log/slog"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/gorilla/websocket"
)

var errBridgeDisconnected = errors.New("cardputer disconnected before reply")

type Bridger interface {
	Connected() bool
	SendAndWait(payload string, questionType string, options []string, timeout time.Duration) (string, error)
}

type Bridge struct {
	mu             sync.Mutex
	writeMu        sync.Mutex
	stateMu        sync.Mutex
	conn           *websocket.Conn
	pending        chan string
	pendingErr     chan error
	currentType    string
	currentOptions []string
	shutdownCtx    context.Context
	shutdownCancel context.CancelFunc
	logger         *slog.Logger
	server         *http.Server
	upgrader       websocket.Upgrader
}

func NewBridge(logger *slog.Logger) *Bridge {
	if logger == nil {
		logger = slog.Default()
	}

	shutdownCtx, shutdownCancel := context.WithCancel(context.Background())

	return &Bridge{
		shutdownCtx:    shutdownCtx,
		shutdownCancel: shutdownCancel,
		logger:         logger,
		upgrader: websocket.Upgrader{
			CheckOrigin: func(r *http.Request) bool { return true },
			ReadBufferSize:  1024,
			WriteBufferSize: 1024,
		},
	}
}

func (b *Bridge) Connected() bool {
	b.stateMu.Lock()
	defer b.stateMu.Unlock()

	return b.conn != nil
}

func (b *Bridge) SendAndWait(payload string, questionType string, options []string, timeout time.Duration) (string, error) {
	b.mu.Lock()
	defer b.mu.Unlock()

	replyCh := make(chan string, 1)
	errCh := make(chan error, 1)

	b.stateMu.Lock()
	conn := b.conn
	if conn == nil {
		b.stateMu.Unlock()
		return "", errors.New("cardputer not connected")
	}
	b.pending = replyCh
	b.pendingErr = errCh
	b.currentType = questionType
	b.currentOptions = append([]string(nil), options...)
	shutdownCtx := b.shutdownCtx
	b.stateMu.Unlock()

	defer b.clearPending(replyCh)

	b.writeMu.Lock()
	err := conn.WriteMessage(websocket.TextMessage, []byte(payload))
	b.writeMu.Unlock()
	if err != nil {
		wrapped := fmt.Errorf("write to cardputer failed: %w", err)
		b.disconnectConn(conn, wrapped)
		return "", wrapped
	}

	timer := time.NewTimer(timeout)
	defer timer.Stop()

	select {
	case reply, ok := <-replyCh:
		if !ok {
			return "", errBridgeDisconnected
		}
		return reply, nil
	case err := <-errCh:
		return "", err
	case <-timer.C:
		return "", context.DeadlineExceeded
	case <-shutdownCtx.Done():
		return "", shutdownCtx.Err()
	}
}

func (b *Bridge) wsHandler(w http.ResponseWriter, r *http.Request) {
	conn, err := b.upgrader.Upgrade(w, r, nil)
	if err != nil {
		b.logger.Error("websocket upgrade failed", "error", err)
		return
	}

	b.replaceConn(conn)

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
	b.stateMu.Lock()
	replyCh := b.pending
	errCh := b.pendingErr
	questionType := b.currentType
	options := append([]string(nil), b.currentOptions...)
	b.stateMu.Unlock()

	if replyCh == nil {
		return
	}

	reply := strings.TrimSpace(msg)
	if questionType == "choose" {
		mapped, err := mapChooseReply(reply, options)
		if err != nil {
			select {
			case errCh <- err:
			default:
			}
			return
		}
		reply = mapped
	}

	select {
	case replyCh <- reply:
	default:
	}
}

func (b *Bridge) Start(addr string) error {
	mux := http.NewServeMux()
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

func (b *Bridge) Shutdown() error {
	b.shutdownCancel()

	b.stateMu.Lock()
	server := b.server
	conn := b.conn
	b.stateMu.Unlock()

	b.disconnectConn(conn, context.Canceled)
	if server == nil {
		return nil
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	return server.Shutdown(ctx)
}

func (b *Bridge) clearPending(replyCh chan string) {
	b.stateMu.Lock()
	defer b.stateMu.Unlock()

	if b.pending != replyCh {
		return
	}

	b.pending = nil
	b.pendingErr = nil
	b.currentType = ""
	b.currentOptions = nil
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
	replyCh := b.pending
	errCh := b.pendingErr
	b.conn = nil
	b.pending = nil
	b.pendingErr = nil
	b.currentType = ""
	b.currentOptions = nil
	b.stateMu.Unlock()

	if currentConn != nil {
		_ = currentConn.Close()
	}
	if replyCh != nil {
		close(replyCh)
	}
	if errCh != nil {
		select {
		case errCh <- cause:
		default:
		}
	}
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
