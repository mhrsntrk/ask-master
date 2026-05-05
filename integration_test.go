package main

import (
	"context"
	"io"
	"log/slog"
	"strings"
	"testing"

	"github.com/mark3labs/mcp-go/mcp"
	"github.com/mark3labs/mcp-go/server"
)

func setupIntegration(t *testing.T) (*Bridge, *server.MCPServer, func()) {
	t.Helper()

	bridge := NewBridge(testLogger())

	s := server.NewMCPServer("ask-master-integration", "test",
		server.WithToolCapabilities(true),
		server.WithRecovery(),
	)
	RegisterTools(s, bridge, slog.New(slog.NewTextHandler(io.Discard, nil)))

	cleanup := func() {
		_ = bridge.Shutdown()
	}

	return bridge, s, cleanup
}

func TestIntegration_AskHuman_Success(t *testing.T) {
	bridge, s, cleanup := setupIntegration(t)
	defer cleanup()

	client := connectTestClient(t, bridge)
	defer client.Close()

	go func() {
		_, msg, err := client.ReadMessage()
		if err != nil {
			t.Errorf("read message: %v", err)
			return
		}
		if !strings.Contains(string(msg), `"type":"ask"`) {
			t.Errorf("expected ask payload, got %q", string(msg))
			return
		}
		if err := client.WriteMessage(1, []byte("PostgreSQL")); err != nil {
			t.Errorf("write reply: %v", err)
		}
	}()

	tool := s.GetTool("ask-human")
	if tool == nil {
		t.Fatal("ask-human tool not registered")
	}

	result, err := tool.Handler(context.Background(), toolRequest("ask-human", map[string]any{
		"question": "Which DB?",
		"context":  "3 options",
	}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}

	assertToolText(t, result, "PostgreSQL")
}

func TestIntegration_Confirm_Success(t *testing.T) {
	bridge, s, cleanup := setupIntegration(t)
	defer cleanup()

	client := connectTestClient(t, bridge)
	defer client.Close()

	go func() {
		_, msg, err := client.ReadMessage()
		if err != nil {
			t.Errorf("read message: %v", err)
			return
		}
		if !strings.Contains(string(msg), `"type":"confirm"`) {
			t.Errorf("expected confirm payload, got %q", string(msg))
			return
		}
		if err := client.WriteMessage(1, []byte("y")); err != nil {
			t.Errorf("write reply: %v", err)
		}
	}()

	tool := s.GetTool("confirm")
	if tool == nil {
		t.Fatal("confirm tool not registered")
	}

	result, err := tool.Handler(context.Background(), toolRequest("confirm", map[string]any{
		"statement":   "Delete all data?",
		"consequence": "Irreversible",
	}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}

	assertToolText(t, result, "true")
}

func TestIntegration_Choose_Success(t *testing.T) {
	bridge, s, cleanup := setupIntegration(t)
	defer cleanup()

	client := connectTestClient(t, bridge)
	defer client.Close()

	go func() {
		_, msg, err := client.ReadMessage()
		if err != nil {
			t.Errorf("read message: %v", err)
			return
		}
		if !strings.Contains(string(msg), `"type":"choose"`) {
			t.Errorf("expected choose payload, got %q", string(msg))
			return
		}
		if err := client.WriteMessage(1, []byte("2")); err != nil {
			t.Errorf("write reply: %v", err)
		}
	}()

	tool := s.GetTool("choose")
	if tool == nil {
		t.Fatal("choose tool not registered")
	}

	result, err := tool.Handler(context.Background(), toolRequest("choose", map[string]any{
		"question": "Which region?",
		"options":  []any{"us-east", "eu-west", "ap-south"},
	}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}

	assertToolText(t, result, "eu-west")
}

func TestIntegration_OfflineDegradation(t *testing.T) {
	_, s, cleanup := setupIntegration(t)
	defer cleanup()

	askTool := s.GetTool("ask-human")
	if askTool == nil {
		t.Fatal("ask-human tool not registered")
	}
	result, err := askTool.Handler(context.Background(), toolRequest("ask-human", map[string]any{
		"question": "Need approval?",
	}))
	if err != nil {
		t.Fatalf("ask-human handler returned error: %v", err)
	}
	assertToolText(t, result, "[CARDPUTER OFFLINE] Please answer manually: Need approval?")

	confirmTool := s.GetTool("confirm")
	if confirmTool == nil {
		t.Fatal("confirm tool not registered")
	}
	result, err = confirmTool.Handler(context.Background(), toolRequest("confirm", map[string]any{
		"statement": "Delete all data?",
	}))
	if err != nil {
		t.Fatalf("confirm handler returned error: %v", err)
	}
	assertToolText(t, result, "false")

	chooseTool := s.GetTool("choose")
	if chooseTool == nil {
		t.Fatal("choose tool not registered")
	}
	result, err = chooseTool.Handler(context.Background(), toolRequest("choose", map[string]any{
		"question": "Which region?",
		"options":  []any{"us-east", "eu-west", "ap-south"},
	}))
	if err != nil {
		t.Fatalf("choose handler returned error: %v", err)
	}
	assertToolText(t, result, "us-east")
}

func TestIntegration_Timeout(t *testing.T) {
	bridge, s, cleanup := setupIntegration(t)
	defer cleanup()

	client := connectTestClient(t, bridge)
	defer client.Close()

	go func() {
		_, _, err := client.ReadMessage()
		if err != nil {
			return
		}
	}()

	tool := s.GetTool("ask-human")
	if tool == nil {
		t.Fatal("ask-human tool not registered")
	}

	result, err := tool.Handler(context.Background(), toolRequest("ask-human", map[string]any{
		"question": "Will you reply?",
		"timeout":  100,
	}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}

	if !result.IsError {
		t.Fatal("expected error result for timeout, got non-error")
	}
	if len(result.Content) != 1 {
		t.Fatalf("expected 1 content item, got %d", len(result.Content))
	}
	text, ok := result.Content[0].(mcp.TextContent)
	if !ok {
		t.Fatalf("expected text content, got %T", result.Content[0])
	}
	if !strings.Contains(text.Text, "context deadline exceeded") && !strings.Contains(text.Text, "deadline exceeded") {
		t.Fatalf("expected timeout error text, got %q", text.Text)
	}
}
