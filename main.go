package main

import (
	"context"
	"log/slog"
	"os"
	"os/signal"
	"syscall"

	"github.com/mark3labs/mcp-go/server"
)

var version = "dev"

func main() {
	cfg := ParseConfig()
	SetupLogger(cfg.LogLevel)
	logger := slog.Default()

	// If a daemon is already running, proxy stdio to it.
	if isDaemonRunning() {
		logger.Info("daemon detected, running in client mode")
		if err := runClientMode(logger); err != nil {
			logger.Error("client mode failed", "error", err)
			os.Exit(1)
		}
		return
	}

	// First instance: become the daemon.
	if err := writeLockFile(); err != nil {
		logger.Error("failed to write lock file", "error", err)
		os.Exit(1)
	}
	defer removeLockFile()

	bridge := NewBridge(logger)

	if err := bridge.StartUDP(); err != nil {
		logger.Error("UDP listener failed", "error", err)
		os.Exit(1)
	}
	logger.Info("UDP beacon listener started", "port", udpBeaconPort)

	go func() {
		if err := bridge.StartWS(cfg.WSAddr); err != nil {
			logger.Error("bridge server failed", "error", err)
		}
	}()

	daemon := NewDaemon(bridge, logger)
	if err := daemon.Start(); err != nil {
		logger.Error("daemon failed", "error", err)
		os.Exit(1)
	}
	logger.Info("daemon started", "socket", socketPath)

	s := server.NewMCPServer("ask-master", version,
		server.WithToolCapabilities(true),
		server.WithRecovery(),
	)
	RegisterTools(s, bridge, logger)

	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	done := make(chan struct{})
	go func() {
		defer close(done)

		stdioServer := server.NewStdioServer(s)
		ctx, cancel := contextWithCancel(logger)
		defer cancel()

		sigChanInternal := make(chan os.Signal, 1)
		signal.Notify(sigChanInternal, syscall.SIGTERM, syscall.SIGINT)
		go func() {
			<-sigChanInternal
			cancel()
		}()

		if err := stdioServer.Listen(ctx, os.Stdin, newAnnotationFilter(os.Stdout)); err != nil {
			logger.Error("MCP server failed", "error", err)
		}
	}()

	select {
	case <-sigChan:
		logger.Info("shutting down due to signal")
	case <-done:
		logger.Info("shutting down due to stdin closure")
	}

	if err := bridge.Shutdown(); err != nil {
		logger.Error("bridge shutdown failed", "error", err)
	}

	logger.Info("shutdown complete")
}

func contextWithCancel(logger *slog.Logger) (context.Context, context.CancelFunc) {
	return context.WithCancel(context.Background())
}
