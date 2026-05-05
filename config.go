package main

import (
	"flag"
	"fmt"
	"log/slog"
	"os"
	"strings"
	"time"
)

type Config struct {
	WSAddr   string
	Timeout  time.Duration
	LogLevel slog.Level
	Version  string
}

func ParseConfig() *Config {
	var (
		wsAddr      = flag.String("ws-addr", "0.0.0.0:8765", "WebSocket server address")
		timeout     = flag.Duration("timeout", 300*time.Second, "Connection timeout")
		logLevelStr = flag.String("log-level", "info", "Log level (debug, info, warn, error)")
		versionFlag = flag.Bool("version", false, "Print version and exit")
	)

	flag.Parse()

	if versionFlag != nil && *versionFlag {
		fmt.Printf("ask-master version %s\n", version)
		os.Exit(0)
	}

	var level slog.Level
	switch strings.ToLower(*logLevelStr) {
	case "debug":
		level = slog.LevelDebug
	case "info":
		level = slog.LevelInfo
	case "warn":
		level = slog.LevelWarn
	case "error":
		level = slog.LevelError
	default:
		level = slog.LevelInfo
	}

	return &Config{
		WSAddr:   *wsAddr,
		Timeout:  *timeout,
		LogLevel: level,
		Version:  version,
	}
}

func SetupLogger(level slog.Level) {
	handler := slog.NewJSONHandler(os.Stderr, &slog.HandlerOptions{Level: level})
	logger := slog.New(handler)
	slog.SetDefault(logger)
}
