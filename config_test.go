package main

import (
	"flag"
	"log/slog"
	"os"
	"testing"
	"time"
)

func TestParseConfig_Defaults(t *testing.T) {
	// Reset flag.CommandLine to avoid interference between tests
	flag.CommandLine = flag.NewFlagSet(os.Args[0], flag.PanicOnError)
	os.Args = []string{"cmd"}

	cfg := ParseConfig()

	if cfg.WSAddr != "0.0.0.0:8765" {
		t.Errorf("expected WSAddr 0.0.0.0:8765, got %s", cfg.WSAddr)
	}
	if cfg.Timeout != 300*time.Second {
		t.Errorf("expected Timeout 300s, got %v", cfg.Timeout)
	}
	if cfg.LogLevel != slog.LevelInfo {
		t.Errorf("expected LogLevel info, got %v", cfg.LogLevel)
	}
	if cfg.Version != "dev" {
		t.Errorf("expected Version dev, got %s", cfg.Version)
	}
}

func TestParseConfig_CustomFlags(t *testing.T) {
	flag.CommandLine = flag.NewFlagSet(os.Args[0], flag.PanicOnError)
	os.Args = []string{"cmd", "--ws-addr", "127.0.0.1:9999", "--timeout", "100s", "--log-level", "debug"}

	cfg := ParseConfig()

	if cfg.WSAddr != "127.0.0.1:9999" {
		t.Errorf("expected WSAddr 127.0.0.1:9999, got %s", cfg.WSAddr)
	}
	if cfg.Timeout != 100*time.Second {
		t.Errorf("expected Timeout 100s, got %v", cfg.Timeout)
	}
	if cfg.LogLevel != slog.LevelDebug {
		t.Errorf("expected LogLevel debug, got %v", cfg.LogLevel)
	}
}

func TestSetupLogger_Levels(t *testing.T) {
	levels := []slog.Level{slog.LevelDebug, slog.LevelInfo, slog.LevelWarn, slog.LevelError}
	for _, l := range levels {
		SetupLogger(l)
		// SetupLogger doesn't return anything, but we ensure it doesn't panic
	}
}

func TestVersionFlag(t *testing.T) {
	// This test is tricky because --version should exit.
	// In a real TDD scenario, we might refactor ParseConfig to take an io.Writer 
	// and return an error or a special type to indicate exit.
	// For now, we'll implement it and verify manually or via a separate test pattern if needed.
}
