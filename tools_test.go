package main

import (
	"context"
	"encoding/json"
	"io"
	"log/slog"
	"strings"
	"testing"
	"time"

	"github.com/mark3labs/mcp-go/mcp"
	"github.com/mark3labs/mcp-go/server"
)

func TestAskHuman_Offline(t *testing.T) {
	bridge := &stubBridge{err: errBridgeDisconnected}
	handler := registeredHandler(t, bridge, "ask-human")

	result, err := handler(context.Background(), toolRequest("ask-human", map[string]any{"question": "Need approval?"}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}
	assertToolText(t, result, "[CARDPUTER OFFLINE] Please answer manually: Need approval?")
	if result.IsError {
		t.Fatal("expected non-error tool result")
	}
}

func TestAskHuman_Success(t *testing.T) {
	bridge := &stubBridge{reply: "approved"}
	handler := registeredHandler(t, bridge, "ask-human")

	result, err := handler(context.Background(), toolRequest("ask-human", map[string]any{
		"question": "Proceed?",
		"context":  "deployment",
		"timeout":  1500,
	}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}

	assertToolText(t, result, "approved")
	assertJSONPayload(t, bridge.payload, map[string]any{
		"type":     "ask",
		"question": "Proceed?",
		"context":  "deployment",
	})
	if bridge.questionType != "ask" {
		t.Fatalf("expected question type ask, got %q", bridge.questionType)
	}
	if bridge.options != nil {
		t.Fatalf("expected nil options, got %#v", bridge.options)
	}
	if bridge.timeout != 1500*time.Millisecond {
		t.Fatalf("expected timeout 1500ms, got %s", bridge.timeout)
	}
}

func TestAskHuman_Truncation(t *testing.T) {
	bridge := &stubBridge{reply: "ok"}
	handler := registeredHandler(t, bridge, "ask-human")
	question := strings.Repeat("q", 121)

	result, err := handler(context.Background(), toolRequest("ask-human", map[string]any{"question": question}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}

	assertToolText(t, result, "ok")
	assertJSONPayload(t, bridge.payload, map[string]any{
		"type":     "ask",
		"question": strings.Repeat("q", 120),
		"context":  "",
	})
}

func TestConfirm_Offline(t *testing.T) {
	bridge := &stubBridge{err: errBridgeDisconnected}
	handler := registeredHandler(t, bridge, "confirm")

	result, err := handler(context.Background(), toolRequest("confirm", map[string]any{"statement": "Delete production?"}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}
	assertToolText(t, result, "false")
	if result.IsError {
		t.Fatal("expected non-error tool result")
	}
}

func TestConfirm_Success_Yes(t *testing.T) {
	bridge := &stubBridge{reply: "Y"}
	handler := registeredHandler(t, bridge, "confirm")

	result, err := handler(context.Background(), toolRequest("confirm", map[string]any{
		"statement":   "Ship release?",
		"consequence": "Production deploy",
		"timeout":     800,
	}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}

	assertToolText(t, result, "true")
	assertJSONPayload(t, bridge.payload, map[string]any{
		"type":     "confirm",
		"question": "Ship release?",
		"context":  "Production deploy",
	})
	if bridge.questionType != "confirm" {
		t.Fatalf("expected question type confirm, got %q", bridge.questionType)
	}
	if bridge.timeout != 800*time.Millisecond {
		t.Fatalf("expected timeout 800ms, got %s", bridge.timeout)
	}
}

func TestConfirm_Success_No(t *testing.T) {
	bridge := &stubBridge{reply: "n"}
	handler := registeredHandler(t, bridge, "confirm")

	result, err := handler(context.Background(), toolRequest("confirm", map[string]any{"statement": "Roll back?"}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}

	assertToolText(t, result, "false")
}

func TestChoose_Offline(t *testing.T) {
	bridge := &stubBridge{err: errBridgeDisconnected}
	handler := registeredHandler(t, bridge, "choose")
	options := []string{"safe", "fast"}

	result, err := handler(context.Background(), toolRequest("choose", map[string]any{
		"question": "Mode?",
		"options":  []any{"safe", "fast"},
	}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}

	assertToolText(t, result, options[0])
	if result.IsError {
		t.Fatal("expected non-error tool result")
	}
}

func TestChoose_Success(t *testing.T) {
	bridge := &stubBridge{reply: "fast"}
	handler := registeredHandler(t, bridge, "choose")
	options := []any{"safe", "fast", "debug"}

	result, err := handler(context.Background(), toolRequest("choose", map[string]any{
		"question": "Mode?",
		"options":  options,
		"context":  "Pick one",
		"timeout":  900,
	}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}

	assertToolText(t, result, "fast")
	assertJSONPayload(t, bridge.payload, map[string]any{
		"type":     "choose",
		"question": "Mode?",
		"context":  "Pick one",
		"options":  []any{"safe", "fast", "debug"},
	})
	assertStringSliceEqual(t, bridge.options, []string{"safe", "fast", "debug"})
	if bridge.questionType != "choose" {
		t.Fatalf("expected question type choose, got %q", bridge.questionType)
	}
	if bridge.timeout != 900*time.Millisecond {
		t.Fatalf("expected timeout 900ms, got %s", bridge.timeout)
	}
}

func TestChoose_TooFewOptions(t *testing.T) {
	bridge := &stubBridge{}
	handler := registeredHandler(t, bridge, "choose")

	result, err := handler(context.Background(), toolRequest("choose", map[string]any{
		"question": "Mode?",
		"options":  []any{"safe"},
	}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}

	assertToolErrorText(t, result, "options must have 2-6 items")
	if bridge.payload != "" {
		t.Fatalf("expected bridge not called, got payload %q", bridge.payload)
	}
}

func TestChoose_TooManyOptions(t *testing.T) {
	bridge := &stubBridge{}
	handler := registeredHandler(t, bridge, "choose")

	result, err := handler(context.Background(), toolRequest("choose", map[string]any{
		"question": "Mode?",
		"options":  []any{"1", "2", "3", "4", "5", "6", "7"},
	}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}

	assertToolErrorText(t, result, "options must have 2-6 items")
	if bridge.payload != "" {
		t.Fatalf("expected bridge not called, got payload %q", bridge.payload)
	}
}

func TestChoose_Truncation(t *testing.T) {
	bridge := &stubBridge{reply: "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"}
	handler := registeredHandler(t, bridge, "choose")

	result, err := handler(context.Background(), toolRequest("choose", map[string]any{
		"question": strings.Repeat("q", 101),
		"options": []any{
			strings.Repeat("a", 41),
			strings.Repeat("b", 41),
		},
	}))
	if err != nil {
		t.Fatalf("handler returned error: %v", err)
	}

	assertToolText(t, result, strings.Repeat("b", 40))
	assertJSONPayload(t, bridge.payload, map[string]any{
		"type":     "choose",
		"question": strings.Repeat("q", 100),
		"context":  "",
		"options": []any{
			strings.Repeat("a", 40),
			strings.Repeat("b", 40),
		},
	})
	assertStringSliceEqual(t, bridge.options, []string{strings.Repeat("a", 40), strings.Repeat("b", 40)})
}

type stubBridge struct {
	reply        string
	err          error
	payload      string
	questionType string
	options      []string
	timeout      time.Duration
}

func (s *stubBridge) Connected() bool {
	return s.err == nil
}

func (s *stubBridge) DeviceOnline() bool {
	return s.err == nil
}

func (s *stubBridge) SendAndWait(payload string, questionType string, options []string, timeout time.Duration) (string, error) {
	s.payload = payload
	s.questionType = questionType
	s.options = append([]string(nil), options...)
	s.timeout = timeout
	if s.err != nil {
		return "", s.err
	}
	return s.reply, nil
}

func registeredHandler(t *testing.T, bridge Bridger, toolName string) server.ToolHandlerFunc {
	t.Helper()

	s := server.NewMCPServer("test", "0.0.1")
	RegisterTools(s, bridge, slog.New(slog.NewTextHandler(io.Discard, nil)))

	tools := s.ListTools()
	if len(tools) != 4 {
		t.Fatalf("expected 4 registered tools, got %d", len(tools))
	}

	tool := s.GetTool(toolName)
	if tool == nil {
		t.Fatalf("tool %q not registered", toolName)
	}

	return tool.Handler
}

func toolRequest(name string, args map[string]any) mcp.CallToolRequest {
	return mcp.CallToolRequest{
		Params: mcp.CallToolParams{
			Name:      name,
			Arguments: args,
		},
	}
}

func assertToolText(t *testing.T, result *mcp.CallToolResult, want string) {
	t.Helper()
	if result == nil {
		t.Fatal("expected non-nil result")
	}
	if len(result.Content) != 1 {
		t.Fatalf("expected 1 content item, got %d", len(result.Content))
	}
	text, ok := result.Content[0].(mcp.TextContent)
	if !ok {
		t.Fatalf("expected text content, got %T", result.Content[0])
	}
	if text.Text != want {
		t.Fatalf("expected text %q, got %q", want, text.Text)
	}
}

func assertToolErrorText(t *testing.T, result *mcp.CallToolResult, want string) {
	t.Helper()
	assertToolText(t, result, want)
	if !result.IsError {
		t.Fatal("expected error tool result")
	}
}

func assertJSONPayload(t *testing.T, got string, want map[string]any) {
	t.Helper()

	var gotMap map[string]any
	if err := json.Unmarshal([]byte(got), &gotMap); err != nil {
		t.Fatalf("unmarshal payload: %v", err)
	}

	if len(gotMap) != len(want) {
		t.Fatalf("expected payload with %d fields, got %d: %#v", len(want), len(gotMap), gotMap)
	}

	for key, wantValue := range want {
		gotValue, ok := gotMap[key]
		if !ok {
			t.Fatalf("expected payload key %q missing in %#v", key, gotMap)
		}
		assertJSONValueEqual(t, key, gotValue, wantValue)
	}
}

func assertJSONValueEqual(t *testing.T, key string, got any, want any) {
	t.Helper()

	switch wantValue := want.(type) {
	case []any:
		gotSlice, ok := got.([]any)
		if !ok {
			t.Fatalf("expected key %q to be []any, got %T", key, got)
		}
		if len(gotSlice) != len(wantValue) {
			t.Fatalf("expected key %q length %d, got %d", key, len(wantValue), len(gotSlice))
		}
		for i := range wantValue {
			if gotSlice[i] != wantValue[i] {
				t.Fatalf("expected key %q index %d to be %v, got %v", key, i, wantValue[i], gotSlice[i])
			}
		}
	default:
		if got != want {
			t.Fatalf("expected key %q to be %v, got %v", key, want, got)
		}
	}
}

func assertStringSliceEqual(t *testing.T, got []string, want []string) {
	t.Helper()
	if len(got) != len(want) {
		t.Fatalf("expected %d options, got %d", len(want), len(got))
	}
	for i := range want {
		if got[i] != want[i] {
			t.Fatalf("expected option %d to be %q, got %q", i, want[i], got[i])
		}
	}
}
