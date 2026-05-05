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
	// 2. Parse config
	cfg := ParseConfig()

	// 4. Setup logger
	SetupLogger(cfg.LogLevel)
	logger := slog.Default()

	// 5. Create bridge
	bridge := NewBridge(logger)

	// 6. Start bridge goroutine
	go func() {
		if err := bridge.Start(cfg.WSAddr); err != nil {
			logger.Error("bridge server failed", "error", err)
		}
	}()

	// 7. Create MCP server
	s := server.NewMCPServer("ask-master", version,
		server.WithToolCapabilities(true),
		server.WithRecovery(),
	)

	// 8. Register tools
	RegisterTools(s, bridge, logger)

	// 9. Handle OS signals for graceful shutdown
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	// 10. Start serving in a goroutine so we can wait for signals or stdin closure
	done := make(chan struct{})
	go func() {
		defer close(done)

		stdioServer := server.NewStdioServer(s)

		ctx, cancel := context.WithCancel(context.Background())
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

	// Wait for signal or MCP server to exit (e.g. stdin closed)
	select {
	case <-sigChan:
		logger.Info("shutting down due to signal")
	case <-done:
		logger.Info("shutting down due to stdin closure")
	}

	// 11. Call bridge.Shutdown()
	if err := bridge.Shutdown(); err != nil {
		logger.Error("bridge shutdown failed", "error", err)
	}

	logger.Info("shutdown complete")
}
