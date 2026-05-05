package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/gorilla/websocket"
)

func TestBridgeOfflineAskHuman(t *testing.T) {
	bridge := NewBridge(testLogger())
	got := askHumanForTest(bridge, "Need approval?", 25*time.Millisecond)
	want := "[CARDPUTER OFFLINE] Please answer manually: Need approval?"
	if got != want {
		t.Fatalf("expected %q, got %q", want, got)
	}
}

func TestBridgeOfflineConfirm(t *testing.T) {
	bridge := NewBridge(testLogger())
	if got := confirmForTest(bridge, "Delete production?", 25*time.Millisecond); got != "false" {
		t.Fatalf("expected false, got %q", got)
	}
}

func TestBridgeOfflineChoose(t *testing.T) {
	bridge := NewBridge(testLogger())
	options := []string{"safe", "fast"}
	if got := chooseForTest(bridge, "Mode?", options, 25*time.Millisecond); got != options[0] {
		t.Fatalf("expected %q, got %q", options[0], got)
	}
}

func TestBridgeSendAndWaitSuccess(t *testing.T) {
	bridge := NewBridge(testLogger())
	client := connectTestClient(t, bridge)
	defer client.Close()

	payload := `{"type":"ask","question":"Proceed?"}`
	readDone := make(chan struct{})
	go func() {
		defer close(readDone)
		_, msg, err := client.ReadMessage()
		if err != nil {
			t.Errorf("read message: %v", err)
			return
		}
		if string(msg) != payload {
			t.Errorf("expected payload %q, got %q", payload, string(msg))
			return
		}
		if err := client.WriteMessage(websocket.TextMessage, []byte("approved")); err != nil {
			t.Errorf("write reply: %v", err)
		}
	}()

	got, err := bridge.SendAndWait(payload, "ask", nil, time.Second)
	if err != nil {
		t.Fatalf("SendAndWait returned error: %v", err)
	}
	if got != "approved" {
		t.Fatalf("expected approved, got %q", got)
	}
	<-readDone
}

func TestBridgeSendAndWaitTimeout(t *testing.T) {
	bridge := NewBridge(testLogger())
	client := connectTestClient(t, bridge)
	defer client.Close()

	go func() {
		_, _, _ = client.ReadMessage()
	}()

	_, err := bridge.SendAndWait(`{"type":"ask","question":"Wait?"}`, "ask", nil, 50*time.Millisecond)
	if !strings.Contains(fmt.Sprint(err), context.DeadlineExceeded.Error()) {
		t.Fatalf("expected deadline exceeded error, got %v", err)
	}
}

func TestBridgeDisconnectMidQuestion(t *testing.T) {
	bridge := NewBridge(testLogger())
	client := connectTestClient(t, bridge)

	go func() {
		_, _, _ = client.ReadMessage()
		_ = client.Close()
	}()

	_, err := bridge.SendAndWait(`{"type":"ask","question":"Still there?"}`, "ask", nil, time.Second)
	if err == nil {
		t.Fatal("expected disconnect error, got nil")
	}
	if strings.Contains(err.Error(), context.DeadlineExceeded.Error()) {
		t.Fatalf("expected disconnect error, got timeout: %v", err)
	}
}

func TestBridgeChooseDigitMapping(t *testing.T) {
	bridge := NewBridge(testLogger())
	client := connectTestClient(t, bridge)
	defer client.Close()

	go func() {
		_, _, _ = client.ReadMessage()
		_ = client.WriteMessage(websocket.TextMessage, []byte("2"))
	}()

	got, err := bridge.SendAndWait(`{"type":"choose","question":"Pick","options":["red","blue","green"]}`, "choose", []string{"red", "blue", "green"}, time.Second)
	if err != nil {
		t.Fatalf("SendAndWait returned error: %v", err)
	}
	if got != "blue" {
		t.Fatalf("expected blue, got %q", got)
	}
}

func TestBridgeChooseOutOfRangeDigitZero(t *testing.T) {
	bridge := NewBridge(testLogger())
	client := connectTestClient(t, bridge)
	defer client.Close()

	go func() {
		_, _, _ = client.ReadMessage()
		_ = client.WriteMessage(websocket.TextMessage, []byte("0"))
	}()

	_, err := bridge.SendAndWait(`{"type":"choose","question":"Pick","options":["red","blue"]}`, "choose", []string{"red", "blue"}, time.Second)
	if err == nil || !strings.Contains(err.Error(), "out of range") {
		t.Fatalf("expected out of range error, got %v", err)
	}
}

func TestBridgeChooseOutOfRangeDigitSeven(t *testing.T) {
	bridge := NewBridge(testLogger())
	client := connectTestClient(t, bridge)
	defer client.Close()

	go func() {
		_, _, _ = client.ReadMessage()
		_ = client.WriteMessage(websocket.TextMessage, []byte("7"))
	}()

	_, err := bridge.SendAndWait(`{"type":"choose","question":"Pick","options":["red","blue"]}`, "choose", []string{"red", "blue"}, time.Second)
	if err == nil || !strings.Contains(err.Error(), "out of range") {
		t.Fatalf("expected out of range error, got %v", err)
	}
}

func TestBridgeConcurrentToolCalls(t *testing.T) {
	bridge := NewBridge(testLogger())
	client := connectTestClient(t, bridge)
	defer client.Close()

	firstArrived := make(chan string, 1)
	allowFirstReply := make(chan struct{})
	secondArrived := make(chan string, 1)

	go func() {
		_, first, err := client.ReadMessage()
		if err != nil {
			return
		}
		firstArrived <- string(first)
		<-allowFirstReply

		_ = client.WriteMessage(websocket.TextMessage, []byte("first"))

		_, second, err := client.ReadMessage()
		if err != nil {
			return
		}
		secondArrived <- string(second)
		_ = client.WriteMessage(websocket.TextMessage, []byte("second"))
	}()

	results := make(chan string, 2)
	errs := make(chan error, 2)
	go func() {
		resp, err := bridge.SendAndWait(`{"id":1}`, "ask", nil, time.Second)
		if err != nil {
			errs <- err
			return
		}
		results <- resp
	}()

	<-firstArrived

	go func() {
		resp, err := bridge.SendAndWait(`{"id":2}`, "ask", nil, time.Second)
		if err != nil {
			errs <- err
			return
		}
		results <- resp
	}()

	select {
	case got := <-secondArrived:
		t.Fatalf("second call should be blocked, but payload arrived early: %q", got)
	case <-time.After(200 * time.Millisecond):
	}
	close(allowFirstReply)

	for range 2 {
		select {
		case err := <-errs:
			t.Fatalf("unexpected error: %v", err)
		case <-results:
		}
	}

	select {
	case got := <-secondArrived:
		if got != `{"id":2}` {
			t.Fatalf("expected second payload after first reply, got %q", got)
		}
	case <-time.After(time.Second):
		t.Fatal("timed out waiting for second payload")
	}
}

func TestBridgeWebSocketWriteConcurrency(t *testing.T) {
	bridge := NewBridge(testLogger())
	client := connectTestClient(t, bridge)
	defer client.Close()

	const calls = 8
	go func() {
		for i := range calls {
			_, _, err := client.ReadMessage()
			if err != nil {
				return
			}
			_ = client.WriteMessage(websocket.TextMessage, []byte("reply-"+fmt.Sprint(i)))
		}
	}()

	var wg sync.WaitGroup
	errCh := make(chan error, calls)
	for i := range calls {
		wg.Add(1)
		go func(i int) {
			defer wg.Done()
			_, err := bridge.SendAndWait(fmt.Sprintf(`{"id":%d}`, i), "ask", nil, 2*time.Second)
			errCh <- err
		}(i)
	}
	wg.Wait()
	close(errCh)

	for err := range errCh {
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}
	}
}

func askHumanForTest(bridge Bridger, question string, timeout time.Duration) string {
	payload := mustJSON(map[string]any{"type": "ask", "question": question})
	resp, err := bridge.SendAndWait(payload, "ask", nil, timeout)
	if err != nil {
		return fmt.Sprintf("[CARDPUTER OFFLINE] Please answer manually: %s", question)
	}
	return resp
}

func confirmForTest(bridge Bridger, statement string, timeout time.Duration) string {
	payload := mustJSON(map[string]any{"type": "confirm", "question": statement})
	resp, err := bridge.SendAndWait(payload, "confirm", nil, timeout)
	if err != nil {
		return "false"
	}
	return resp
}

func chooseForTest(bridge Bridger, question string, options []string, timeout time.Duration) string {
	payload := mustJSON(map[string]any{"type": "choose", "question": question, "options": options})
	resp, err := bridge.SendAndWait(payload, "choose", options, timeout)
	if err != nil {
		return options[0]
	}
	return resp
}

func mustJSON(v any) string {
	b, err := json.Marshal(v)
	if err != nil {
		panic(err)
	}
	return string(b)
}

func connectTestClient(t *testing.T, bridge *Bridge) *websocket.Conn {
	t.Helper()

	server := httptest.NewServer(http.HandlerFunc(bridge.wsHandler))
	t.Cleanup(server.Close)

	wsURL := "ws" + strings.TrimPrefix(server.URL, "http")
	client, _, err := websocket.DefaultDialer.Dial(wsURL, nil)
	if err != nil {
		t.Fatalf("dial websocket: %v", err)
	}

	deadline := time.Now().Add(time.Second)
	for !bridge.Connected() {
		if time.Now().After(deadline) {
			client.Close()
			t.Fatal("bridge did not observe websocket connection")
		}
		time.Sleep(10 * time.Millisecond)
	}

	t.Cleanup(func() {
		_ = client.Close()
	})

	return client
}

func testLogger() *slog.Logger {
	return slog.New(slog.NewTextHandler(io.Discard, nil))
}
