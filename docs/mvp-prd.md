## Physical Human-in-the-Loop MCP Server

**Version:** 1.0
**Author:** mhrsntrk
**Last Updated:** May 2026

---

## 1. Overview

### 1.1 Summary

`ask-master` is a standalone MCP (Model Context Protocol) server written in Go that enables any AI coding agent (Claude Code, OpenCode, Cursor, Windsurf, etc.) to pause mid-task and ask a human a question via a physical M5Stack Cardputer ADV device. The human reads the question on the Cardputer's screen and responds using its physical QWERTY keyboard. The agent receives the reply and continues execution.

### 1.2 Problem Statement

Current AI coding agents run autonomously and either proceed with assumptions when information is ambiguous, or interrupt the developer inside the terminal — breaking focus. There is no mechanism to delegate a question to a physical, always-visible device that doesn't require the developer to switch windows or break their flow on the primary screen.

### 1.3 Solution

A two-component system:

1. **Go MCP server** — speaks stdio JSON-RPC (standard MCP transport), exposes three human-in-the-loop tools (`ask-human`, `confirm`, `choose`), and maintains a WebSocket bridge that the Cardputer connects to.
2. **Cardputer firmware** (Arduino/C++) — connects to the local WiFi network, opens a persistent WebSocket connection to the Go server, renders incoming questions on its 1.14" TFT screen with appropriate UI per tool type, and sends the typed/pressed answer back.

### 1.4 Goals

- Work with **any MCP-compatible coding agent** without client-specific code
- **Single static Go binary** — no runtime, no interpreter, no dependencies to install
- **Three interaction modes**: free-text answer, yes/no confirmation, numbered choice
- **Graceful degradation** when Cardputer is offline — safe defaults, no agent crash
- **Persistent WebSocket reconnect** on the Cardputer side — survives WiFi blips
- Production-quality code: proper error handling, structured logging, clean shutdown

---

## 2. Architecture

### 2.1 System Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│  MCP Client (Claude Code / OpenCode / Cursor / Windsurf)         │
│                                                                  │
│   agent calls: ask-human() / confirm() / choose()                │
│         │                                                        │
│         ▼  stdio (JSON-RPC 2.0 over stdin/stdout)                │
├──────────────────────────────────────────────────────────────────┤
│  ask-master  (Go binary)                                     │
│  ├── main.go      — MCP server init + ServeStdio                 │
│  ├── bridge.go    — WebSocket server + state (mutex + channel)   │
│  └── tools.go     — ask-human, confirm, choose handlers          │
│         │                                                        │
│         ▼  WebSocket  ws://0.0.0.0:8765                          │
├──────────────────────────────────────────────────────────────────┤
│  Cardputer ADV firmware (Arduino / C++)                          │
│  ├── WiFi client                                                 │
│  ├── WebSocket client (auto-reconnect every 3s)                  │
│  ├── JSON parser (ArduinoJson)                                   │
│  ├── TFT renderer (per tool type)                                │
│  └── Keyboard input handler                                      │
└──────────────────────────────────────────────────────────────────┘
```

### 2.2 Data Flow (Happy Path)

1. Agent calls `ask-human(question="Which DB migration strategy?", context="3 options available")`
2. MCP server receives call over stdio, acquires bridge mutex lock
3. Server serializes JSON payload and sends to Cardputer over WebSocket
4. Server blocks on a Go channel waiting for reply (timeout: 300s default)
5. Cardputer beeps, renders question on TFT screen
6. Human types answer + presses Enter
7. Cardputer sends raw answer string over WebSocket
8. Server receives reply, resolves channel, returns `CallToolResult` to agent
9. Agent continues execution with the answer

### 2.3 Concurrency Model

- The WebSocket bridge runs in a dedicated goroutine
- `Bridge.SendAndWait()` uses a `sync.Mutex` to serialize concurrent tool calls (only one question active at a time — agents must queue)
- The reply channel is a `chan string` with buffer size 1
- On disconnect mid-question, the channel is closed and an error is returned to the agent

---

## 3. Repository Structure

```
ask-master/
├── main.go                  ← Entry point: starts bridge goroutine, inits MCP server, ServeStdio
├── bridge.go                ← WebSocket server, connection state, SendAndWait()
├── tools.go                 ← MCP tool definitions and handlers
├── go.mod                   ← Module definition and dependencies
├── go.sum
├── config.go                ← CLI flags and config struct (port, timeout, log level)
├── Makefile                 ← build, install, test, lint targets
├── README.md                ← Setup guide + client config snippets
├── firmware/
│   ├── ask_master/
│   │   ├── ask_master.ino   ← Main Arduino sketch
│   │   ├── ui.h / ui.cpp        ← TFT rendering functions
│   │   ├── ws_client.h          ← WebSocket wrapper
│   │   └── config.h             ← WiFi credentials, server IP/port
└── docs/
    ├── architecture.md
    └── client-setup.md
```

---

## 4. Go Server — Detailed Specification

### 4.1 Dependencies

```
github.com/mark3labs/mcp-go  v0.32.0+   ← MCP server, stdio transport, tool DSL
github.com/gorilla/websocket v1.5.3+    ← WebSocket server
```

No other dependencies. `go mod tidy` pulls both transitively.

### 4.2 `config.go`

Parsed from CLI flags via standard `flag` package:

| Flag | Default | Description |
|------|---------|-------------|
| `--ws-addr` | `0.0.0.0:8765` | WebSocket bridge listen address |
| `--timeout` | `300` | Default tool answer timeout in seconds |
| `--log-level` | `info` | `debug` / `info` / `warn` / `error` |
| `--version` | — | Print version and exit |

### 4.3 `bridge.go` — `Bridge` struct

```
type Bridge struct {
    mu      sync.Mutex
    conn    *websocket.Conn   // nil when Cardputer is disconnected
    pending chan string        // buffered(1): current outstanding reply channel
}
```

**Methods:**

| Method | Signature | Behavior |
|--------|-----------|----------|
| `Connected()` | `() bool` | Thread-safe check if a Cardputer is connected |
| `SendAndWait()` | `(payload string, timeout time.Duration) (string, error)` | Serialize payload, block for reply, return answer or error |
| `wsHandler()` | `http.HandlerFunc` | Upgrades HTTP → WS, sets `conn`, reads messages in loop, calls `receive()` |
| `receive()` | `(msg string)` | Resolves pending channel with message |
| `Start()` | `(addr string) error` | Starts `http.ListenAndServe` with `wsHandler` at `/` |

**Error cases:**
- `Cardputer not connected` → returned to tool handler → tool returns offline message to agent
- `write to cardputer failed` → WebSocket error → close connection, return error
- `timeout exceeded` → `context.DeadlineExceeded` equivalent → tool returns timeout message
- `disconnect before reply` → channel closed → tool returns disconnect error

### 4.4 `tools.go` — Tool Definitions

#### Tool 1: `ask-human`

```
Name:        ask-human
Description: Display a free-form question on the Cardputer screen and wait
             for the human to type an answer on the physical keyboard.
             Use ONLY when the answer cannot be inferred from code or context.

Parameters:
  question  (string, required)  — max 120 chars rendered on screen
  context   (string, optional)  — subtitle, max 60 chars
  timeout   (integer, optional) — override default timeout in seconds

Returns: string — the human's typed answer

Offline behavior: returns "[CARDPUTER OFFLINE] Please answer manually: {question}"
```

#### Tool 2: `confirm`

```
Name:        confirm
Description: Ask the human to confirm or deny an action on the Cardputer.
             Human presses Y (yes) or N (no).
             Use for destructive/irreversible/security-sensitive operations.

Parameters:
  statement    (string, required)  — action description, max 120 chars
  consequence  (string, optional)  — what happens if confirmed, max 60 chars

Returns: string — "true" or "false"

Offline behavior: returns "false" (safe deny)
```

#### Tool 3: `choose`

```
Name:        choose
Description: Present a numbered menu (2–6 options) on the Cardputer.
             Human presses a digit key to select.
             Use for discrete named choices.

Parameters:
  question  (string, required)        — the prompt, max 100 chars
  options   (array of string, required) — 2–6 items, each max 40 chars
  context   (string, optional)        — subtitle, max 60 chars

Returns: string — the text of the selected option (not the digit)

Offline behavior: returns options[0] (first option as default)
Validation: returns MCP error if options count < 2 or > 6
```

### 4.5 JSON Payload Format (Server → Cardputer)

All payloads are JSON objects:

```json
{ "type": "ask",     "question": "...", "context": "..." }
{ "type": "confirm", "question": "...", "context": "..." }
{ "type": "choose",  "question": "...", "context": "...", "options": ["a","b","c"] }
```

**Cardputer → Server reply:** plain UTF-8 string (the answer text or digit).

### 4.6 `main.go`

```
1. Parse config flags
2. Initialize logger
3. Initialize Bridge
4. go bridge.Start(config.WSAddr)  — non-blocking goroutine
5. s := server.NewMCPServer("ask-master", version, WithToolCapabilities, WithRecovery)
6. RegisterTools(s)
7. server.ServeStdio(s)  — blocks until stdin closes (agent exits)
```

### 4.7 Build

```makefile
build:
    go build -ldflags="-s -w -X main.version=$(VERSION)" -o ask-master .

install:
    go install -ldflags="-s -w" .

lint:
    golangci-lint run ./...

test:
    go test ./... -race -v
```

Binary size target: < 12MB stripped (`-s -w`). Zero CGO.

---

## 5. Cardputer Firmware — Detailed Specification

### 5.1 Hardware Target

- **Device:** M5Stack Cardputer ADV
- **MCU:** ESP32-S3 (240MHz, 512KB SRAM, 8MB PSRAM, 8MB Flash)
- **Display:** ST7789 1.14" TFT, 135×240px
- **Input:** 56-key QWERTY keyboard (M5Stack Keyboard unit)
- **Audio:** ES8311 codec + built-in speaker
- **Connectivity:** WiFi 802.11 b/g/n

### 5.2 Arduino Libraries

| Library | Version | Source |
|---------|---------|--------|
| M5Cardputer | latest | M5Stack official |
| WebSockets (Markus Sattler) | 2.4.x+ | Arduino Library Manager |
| ArduinoJson | 7.x | Arduino Library Manager |

### 5.3 `config.h`

```cpp
#define WIFI_SSID     "YourSSID"
#define WIFI_PASSWORD "YourPassword"
#define WS_HOST       "192.168.1.X"   // Mac's local IP
#define WS_PORT       8765
#define WS_RECONNECT_MS 3000
#define BEEP_FREQ_ASK     1000
#define BEEP_FREQ_CONFIRM 1300
#define BEEP_FREQ_CHOOSE  900
#define BEEP_DURATION_MS  150
```

### 5.4 State Machine

The firmware runs a simple state machine:

```
IDLE ──── receives WS message ──→ RENDERING
RENDERING ─── drawUI() done ───→ WAITING_INPUT
WAITING_INPUT ── Enter/Y/N/digit → SENDING
SENDING ─── ws.sendTXT() done ──→ IDLE
```

Additionally: `CONNECTING` state on boot and reconnect.

### 5.5 UI Rendering per Tool Type

**`ask` — Navy header**
```
┌─[AGENT QUESTION]──────────────────┐
│ Question line 1 (yellow, 38 chars)│
│ Question line 2 (yellow, overflow)│
│ Question line 3 (yellow, overflow)│
│ Context subtitle (dark grey)      │
│────────────────────────────────── │
│ > typed_input_here_               │
└───────────────────────────────────┘
```

**`confirm` — Maroon header**
```
┌─[CONFIRM REQUIRED]────────────────┐
│ Statement line 1 (orange)         │
│ Statement line 2 (orange)         │
│ Consequence subtitle (dark grey)  │
│────────────────────────────────── │
│ [Y] Confirm        [N] Cancel     │
└───────────────────────────────────┘
```

**`choose` — Dark Cyan header**
```
┌─[CHOOSE AN OPTION]────────────────┐
│ Question (cyan)                   │
│ Context (dark grey)               │
│ 1. Option A                       │
│ 2. Option B                       │
│ 3. Option C                       │
└───────────────────────────────────┘
```

**`idle`**
```
┌───────────────────────────────────┐
│                                   │
│ ask-master v0.1               │
│ Waiting for agent...              │
│ 192.168.1.X                       │
│                                   │
└───────────────────────────────────┘
```

### 5.6 Input Handling Rules

| Tool type | Valid input | Action |
|-----------|-------------|--------|
| `ask` | Any printable char | Append to buffer (max 80 chars) |
| `ask` | Backspace/Del | Remove last char |
| `ask` | Enter | Send buffer, go IDLE |
| `confirm` | `y` or `Y` | Send `"y"`, go IDLE |
| `confirm` | `n` or `N` | Send `"n"`, go IDLE |
| `confirm` | any other | Ignore |
| `choose` | digit `1`–`6` (≤ optionCount) | Send digit string, go IDLE |
| `choose` | any other | Ignore |

### 5.7 WebSocket Reconnect

- `ws.setReconnectInterval(WS_RECONNECT_MS)` — automatic reconnect built into the Sattler library
- On reconnect: re-render idle screen, do NOT re-send any previous payload
- If a question was pending when connection dropped: firmware goes IDLE (server already returned error to agent)

### 5.8 Audio Feedback

| Event | Frequency | Duration |
|-------|-----------|----------|
| Question received (`ask`) | 1000 Hz | 150ms |
| Question received (`confirm`) | 1300 Hz | 150ms |
| Question received (`choose`) | 900 Hz | 150ms |
| Answer sent | 1400 Hz | 80ms (short positive beep) |

---

## 6. Client Configuration Reference

All clients use the same binary via stdio transport. No client-specific code needed.

### Claude Code (`~/.claude/settings.json`)
```json
{
  "mcpServers": {
    "ask-master": {
      "command": "/Users/mahir/go/bin/ask-master",
      "args": ["--ws-addr", "0.0.0.0:8765"]
    }
  }
}
```

### OpenCode (`opencode.jsonc`)
```jsonc
{
  "mcp": {
    "ask-master": {
      "type": "local",
      "command": ["/Users/mahir/go/bin/ask-master", "--ws-addr", "0.0.0.0:8765"],
      "enabled": true,
      "timeout": 310000
    }
  }
}
```

### Cursor (`~/.cursor/mcp.json`)
```json
{
  "mcpServers": {
    "ask-master": {
      "command": "/Users/mahir/go/bin/ask-master"
    }
  }
}
```

### Windsurf (`~/.codeium/windsurf/mcp_config.json`)
```json
{
  "mcpServers": {
    "ask-master": {
      "command": "/Users/mahir/go/bin/ask-master"
    }
  }
}
```

---

## 7. Error Handling & Edge Cases

| Scenario | Server Behavior | Cardputer Behavior |
|----------|----------------|-------------------|
| Cardputer not connected | Return offline message to agent, do not error | N/A |
| Cardputer disconnects mid-question | Close pending channel, return error string to agent | Auto-reconnect loop |
| Answer timeout (300s default) | Return timeout message to agent, clear pending state | Show idle screen |
| Two agents call tools simultaneously | Second call blocks on `sync.Mutex` until first resolves | Not affected |
| JSON parse error from Cardputer | N/A (Cardputer sends plain strings, not JSON) | N/A |
| JSON parse error from server (malformed payload) | Firmware logs error, shows idle screen, does NOT send reply | Handled in wsEvent |
| Cardputer options count out of range | Server returns MCP error before sending to device | N/A |
| WiFi unavailable on boot | Cardputer retries WiFi connect in loop, shows "Connecting..." | Retry loop |
| Binary executed without Cardputer connected | Works normally, all tools return offline messages | N/A |

---

## 8. Non-Functional Requirements

| Requirement | Target |
|-------------|--------|
| Binary size | < 12MB (stripped) |
| Server startup time | < 10ms |
| Idle memory usage | < 15MB RSS |
| Round-trip latency (tool call → screen display) | < 200ms on local WiFi |
| WebSocket message size limit | 4096 bytes (more than sufficient) |
| Concurrent tool calls | Serialized via mutex (by design — one question at a time) |
| Go version | 1.22+ |
| Arduino core | ESP32 Arduino Core 2.x+ |
| Zero CGO | Required — must cross-compile cleanly |

---

## 9. Testing Requirements

### 9.1 Go Unit Tests (`*_test.go`)

- `TestBridgeOffline` — tool handlers return correct offline messages when `conn == nil`
- `TestBridgeSendAndWait` — mock WebSocket, verify payload sent and reply received
- `TestBridgeTimeout` — verify timeout returns error after configured duration
- `TestBridgeDisconnectMidQuestion` — close channel mid-wait, verify error propagation
- `TestTruncate` — edge cases: empty string, exact boundary, multibyte runes
- `TestChooseDigitParsing` — verify digit and text matching logic
- `TestConfirmParsing` — verify all truthy/falsy variants (`y`, `yes`, `Y`, `n`, `no`)

### 9.2 Integration Test

A `cmd/test-client/main.go` tool that:
1. Starts the bridge
2. Simulates a Cardputer WebSocket connection
3. Calls all three tools via a mock MCP client
4. Asserts correct payloads sent and correct results returned

### 9.3 Manual Firmware Verification Checklist

- [ ] WiFi connects and IP shown on idle screen
- [ ] WebSocket connects to Go server (server logs "Cardputer connected")
- [ ] `ask` payload: question renders correctly, multi-line wraps at col 38, backspace works, Enter sends
- [ ] `confirm` payload: Y/N renders, Y sends `"y"`, N sends `"n"`, other keys ignored
- [ ] `choose` payload: all options render, digit selects correct option, out-of-range ignored
- [ ] Beep plays on question receive and on answer send
- [ ] WebSocket disconnect → idle screen → auto-reconnect within 3s
- [ ] Long question (>114 chars) truncated gracefully, no screen overflow

---

## 10. Deliverables Checklist for Coding Agent

The implementing agent must produce all of the following:

### Go Server
- [ ] `go.mod` with correct module path and deps
- [ ] `config.go` — flag parsing, `Config` struct
- [ ] `bridge.go` — `Bridge` struct, all methods, `Start()`
- [ ] `tools.go` — all three tools registered via `mark3labs/mcp-go` DSL
- [ ] `main.go` — wires config + bridge + MCP server
- [ ] `*_test.go` — unit tests covering all cases in §9.1
- [ ] `Makefile` — `build`, `install`, `test`, `lint` targets
- [ ] `README.md` — setup guide, client config snippets for all four clients

### Firmware
- [ ] `firmware/ask_master/ask_master.ino`
- [ ] `firmware/ask_master/config.h` — credentials as `#define`
- [ ] `firmware/ask_master/ui.h` + `ui.cpp` — all four draw functions
- [ ] `firmware/ask_master/ws_client.h` — WebSocket wrapper

### Documentation
- [ ] `docs/architecture.md` — system diagram + data flow description
- [ ] `docs/client-setup.md` — per-client config with copy-paste snippets

---

## 11. Out of Scope (v1.0)

The following are explicitly excluded from v1.0 and tracked for future versions:

- **Answer history log to MicroSD** — deferred to v1.1
- **Function key preset answers** (Fn+1 = "yes", Fn+2 = "no") — v1.1
- **Multi-Cardputer support** (routing to multiple connected devices) — v2.0
- **HTTPS/WSS** (TLS on the WebSocket bridge) — v1.1 (local network, low risk for now)
- **Over-the-air firmware update** — v2.0
- **Web config UI** (for setting WiFi credentials without recompiling) — v1.1
- **BLE transport** (alternative to WiFi) — v2.0
- **Priority levels** (red screen for destructive ops) — v1.1

---

## 12. Version History

| Version | Date | Notes |
|---------|------|-------|
| 1.0 | May 2026 | Initial release — three tools, Go server, Cardputer ADV firmware |

