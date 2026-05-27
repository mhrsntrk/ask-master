package main

import (
	"bufio"
	"fmt"
	"io"
	"log/slog"
	"net"
	"os"
	"path/filepath"
	"sync"

	"github.com/mark3labs/mcp-go/server"
)

var (
	socketPath = resolveStateFile("ask-master.sock")
	lockFile   = resolveStateFile("ask-master.lock")
)

// resolveStateFile returns a per-user path for runtime state files, falling
// back to /tmp only if a per-user directory is not available. This avoids
// the historical /tmp/ask-master.sock collision on multi-user machines.
func resolveStateFile(name string) string {
	if dir := os.Getenv("XDG_RUNTIME_DIR"); dir != "" {
		base := filepath.Join(dir, "ask-master")
		_ = os.MkdirAll(base, 0700)
		return filepath.Join(base, name)
	}
	if cache, err := os.UserCacheDir(); err == nil && cache != "" {
		base := filepath.Join(cache, "ask-master")
		_ = os.MkdirAll(base, 0700)
		return filepath.Join(base, name)
	}
	return filepath.Join("/tmp", name)
}

// Daemon runs the persistent WebSocket + UDP server and accepts
// MCP client connections over a Unix domain socket.
type Daemon struct {
	bridge  *Bridge
	logger  *slog.Logger
	mu      sync.Mutex
	active  bool
	clients map[net.Conn]struct{}
}

func NewDaemon(bridge *Bridge, logger *slog.Logger) *Daemon {
	return &Daemon{
		bridge:  bridge,
		logger:  logger,
		clients: make(map[net.Conn]struct{}),
	}
}

func (d *Daemon) Start() error {
	// Clean up stale socket
	_ = os.Remove(socketPath)

	listener, err := net.Listen("unix", socketPath)
	if err != nil {
		return fmt.Errorf("listen unix socket: %w", err)
	}

	d.mu.Lock()
	d.active = true
	d.mu.Unlock()

	go d.acceptLoop(listener)
	return nil
}

func (d *Daemon) acceptLoop(listener net.Listener) {
	for {
		conn, err := listener.Accept()
		if err != nil {
			d.mu.Lock()
			active := d.active
			d.mu.Unlock()
			if !active {
				return
			}
			d.logger.Warn("daemon accept error", "error", err)
			continue
		}

		d.mu.Lock()
		d.clients[conn] = struct{}{}
		d.mu.Unlock()

		go d.handleClient(conn)
	}
}

func (d *Daemon) handleClient(conn net.Conn) {
	defer func() {
		_ = conn.Close()
		d.mu.Lock()
		delete(d.clients, conn)
		d.mu.Unlock()
	}()

	d.logger.Info("MCP client connected", "remote", conn.RemoteAddr())

	// Create an MCP server for this client connection
	s := server.NewMCPServer("ask-master", version,
		server.WithToolCapabilities(true),
		server.WithRecovery(),
	)
	RegisterTools(s, d.bridge, d.logger)

	// Run JSON-RPC loop over the Unix socket
	stdioServer := server.NewStdioServer(s)
	
	// Wrap the connection as stdin/stdout
	ctx, cancel := contextWithCancel(d.logger)
	defer cancel()

	// Use a pipe approach: socket read -> stdin, stdout -> socket write
	pr, pw := io.Pipe()
	defer pr.Close()
	defer pw.Close()

	// Goroutine: read from socket, write to pipe (acts as stdin).
	// When the socket reaches EOF (client disconnected), cancel the context
	// so handleClient unblocks and returns instead of leaking the goroutines.
	go func() {
		defer pw.Close()
		defer cancel()
		scanner := bufio.NewScanner(conn)
		scanner.Buffer(make([]byte, 0, 4096), wsReadLimitBytes)
		for scanner.Scan() {
			line := scanner.Bytes()
			if len(line) == 0 {
				continue
			}
			_, _ = pw.Write(append(line, '\n'))
		}
	}()

	// Goroutine: read MCP responses from our stdout interceptor, write to socket
	go func() {
		// Create a custom writer that captures stdout
		// For simplicity, we'll use the annotation filter approach
		writer := &socketWriter{conn: conn}
		_ = stdioServer.Listen(ctx, pr, writer)
	}()

	// Wait for connection to close or context cancellation
	<-ctx.Done()
	d.logger.Info("MCP client disconnected", "remote", conn.RemoteAddr())
}

// socketWriter implements io.Writer to send data back to the Unix socket
type socketWriter struct {
	conn net.Conn
	mu   sync.Mutex
}

func (w *socketWriter) Write(p []byte) (n int, err error) {
	w.mu.Lock()
	defer w.mu.Unlock()
	return w.conn.Write(p)
}

// runClientMode connects to the daemon via Unix socket and proxies
// stdin/stdout bidirectionally.
func runClientMode(logger *slog.Logger) error {
	conn, err := net.Dial("unix", socketPath)
	if err != nil {
		return fmt.Errorf("connect to daemon: %w", err)
	}
	defer conn.Close()

	logger.Info("connected to ask-master daemon")

	var wg sync.WaitGroup
	wg.Add(2)

	// stdin -> socket
	go func() {
		defer wg.Done()
		_, _ = io.Copy(conn, os.Stdin)
	}()

	// socket -> stdout
	go func() {
		defer wg.Done()
		_, _ = io.Copy(os.Stdout, conn)
	}()

	wg.Wait()
	return nil
}

// isDaemonRunning checks if the daemon lock file exists and the process is alive.
func isDaemonRunning() bool {
	data, err := os.ReadFile(lockFile)
	if err != nil {
		return false
	}

	var pid int
	if _, err := fmt.Sscanf(string(data), "%d", &pid); err != nil {
		_ = os.Remove(lockFile)
		return false
	}

	// Check if process exists (Unix-specific)
	_, err = os.FindProcess(pid)
	if err != nil {
		_ = os.Remove(lockFile)
		return false
	}

	// Try to connect to the socket as verification
	conn, err := net.Dial("unix", socketPath)
	if err != nil {
		// Stale lock
		_ = os.Remove(lockFile)
		return false
	}
	_ = conn.Close()
	return true
}

// writeLockFile writes the current PID to the lock file.
func writeLockFile() error {
	return os.WriteFile(lockFile, []byte(fmt.Sprintf("%d", os.Getpid())), 0644)
}

// removeLockFile removes the lock file (call on shutdown).
func removeLockFile() {
	_ = os.Remove(lockFile)
	_ = os.Remove(socketPath)
}
