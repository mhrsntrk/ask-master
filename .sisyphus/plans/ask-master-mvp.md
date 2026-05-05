# ask-master MVP — Go MCP Server + Cardputer Firmware

## TL;DR

> **Build a two-component HITL system**: a Go MCP server (stdio transport) exposing `ask_human`, `confirm`, `choose` tools, and M5Stack Cardputer ADV firmware that renders questions on its TFT screen and collects keyboard responses via WebSocket.
>
> **Deliverables**:
> - Go binary (`ask-master`) — single static binary, zero CGO, <12MB
> - Arduino firmware (`firmware/ask_master/`) — 4 UI states, WebSocket reconnect, audio feedback
> - Unit tests (`*_test.go`) — TDD workflow, all §9.1 test cases
> - Integration test (`cmd/test-client/main.go`) — in-process MCP + WebSocket round-trip
> - Makefile, README.md, docs/architecture.md, docs/client-setup.md
>
> **Estimated Effort**: Medium
> **Parallel Execution**: YES — 4 waves + final verification
> **Critical Path**: T1 → T7 → T8 → T11 → T14 → F1-F4

---

## Context

### Original Request
Build the entire ask-master MVP as specified in `docs/mvp-prd.md` — a standalone MCP server in Go and Cardputer firmware in Arduino/C++ that enables AI coding agents to ask humans questions via a physical device.

### Interview Summary
**Key Discussions**:
- Test strategy: TDD chosen — each Go component follows RED → GREEN → REFACTOR
- Logging: `log/slog` (stdlib, zero external deps, aligns with single-binary goal)
- Firmware QA: Compilation check only (no real hardware in this environment)
- Version: v0.1.0 for initial release

**Research Findings**:
- mcp-go v0.32.0+: Tool DSL with `WithString`/`Required()`, `NewToolResultText`/`NewToolResultError`, `ServeStdio` for stdio transport
- gorilla/websocket: Requires dedicated read/write goroutines, `CheckOrigin` policy, `SetReadLimit`, ping/pong heartbeats
- M5Cardputer: 240×135 display, `Speaker.tone(freq, dur)`, `Keyboard.keysState()` with `.word/.del/.enter`, `M5Canvas` for double-buffering

### Metis Review
**Identified Gaps (addressed)**:
- **`choose` state mapping**: Bridge must store `currentOptions []string` alongside `pending chan string` to map digit responses back to option text — added to Bridge struct design
- **WebSocket write concurrency**: gorilla/websocket is NOT safe for concurrent writes — added dedicated `writeMu sync.Mutex` to Bridge struct
- **Graceful shutdown**: `ServeStdio` return must trigger `http.Server.Shutdown(ctx)` — added shutdown coordination via `context.WithCancel`
- **mcp-go error pattern**: Tool-level errors MUST use `mcp.NewToolResultError(msg), nil` — returning Go `error` halts the MCP session — added as guardrail
- **Confirm response mapping**: `"y"` → `"true"`, `"n"` → `"false"` conversion in tool handler, not in Bridge — documented in tools task
- **CheckOrigin policy**: Set `CheckOrigin: func(r *http.Request) bool { return true }` for local network use — documented in bridge task
- **Firmware double-buffering**: Use `M5Canvas` for flicker-free rendering — documented in UI task
- **ArduinoJson 7.x**: Use `JsonDocument` (heap-allocated), NOT `StaticJsonDocument` (v6 API) — documented in firmware task

---

## Work Objectives

### Core Objective
Implement a complete, production-quality HITL system: Go MCP server + Cardputer firmware, following the PRD specification exactly.

### Concrete Deliverables
- `go.mod`, `go.sum` — module definition with mcp-go + gorilla/websocket
- `config.go` — CLI flags, Config struct, log/slog setup
- `bridge.go` — Bridge struct (mutex, write mutex, conn, pending channel, options state, graceful shutdown)
- `tools.go` — ask_human, confirm, choose tool definitions and handlers
- `main.go` — wiring: config → bridge → MCP server → ServeStdio
- `bridge_test.go` — all §9.1 Bridge test cases
- `tools_test.go` — tool handler test cases (offline, validation, response mapping)
- `config_test.go` — flag parsing, log level validation
- `cmd/test-client/main.go` — integration test using mcptest package
- `Makefile` — build, install, test, lint targets
- `README.md` — setup guide + client config snippets
- `firmware/ask_master/ask_master.ino` — main sketch
- `firmware/ask_master/config.h` — WiFi/WS/beep defines
- `firmware/ask_master/ui.h` + `ui.cpp` — 4 draw functions + M5Canvas
- `firmware/ask_master/ws_client.h` — WebSocket client wrapper
- `docs/architecture.md` — system diagram + data flow
- `docs/client-setup.md` — per-client config snippets

### Definition of Done
- [ ] `go test -race -v ./...` passes all unit + integration tests
- [ ] `go build -ldflags="-s -w -X main.version=v0.1.0" -o ask-master .` produces binary <12MB
- [ ] `CGO_ENABLED=0 go build -ldflags="-s -w" .` succeeds (zero CGO)
- [ ] `golangci-lint run ./...` passes with no errors
- [ ] `pio run -d firmware/ask_master` compiles firmware without errors
- [ ] All test cases from PRD §9.1 implemented
- [ ] Integration test exercises all 3 tools via in-process MCP client
- [ ] Binary size verified <12MB stripped

### Must Have
- Three MCP tools: `ask_human`, `confirm`, `choose` per PRD §4.4
- Graceful degradation: offline messages returned when Cardputer disconnected
- WebSocket auto-reconnect on Cardputer side (3s interval)
- TDD workflow for all Go code (RED → GREEN → REFACTOR)
- All error cases from PRD §7 handled correctly
- Audio feedback per PRD §5.8 (different beep frequencies per tool type)
- All four UI states rendered per PRD §5.5 (idle, ask, confirm, choose)
- Input handling rules per PRD §5.6

### Must NOT Have (Guardrails)
- **No features from PRD §11** (MicroSD, Fn presets, multi-Cardputer, TLS/WSS, OTA, web UI, BLE, priority levels)
- **No Go `error` returns from tool handlers** — MUST use `mcp.NewToolResultError(msg), nil` (causes session halt otherwise)
- **No concurrent WebSocket writes** — MUST use dedicated `writeMu sync.Mutex`
- **No `StaticJsonDocument`** (ArduinoJson v6) — MUST use `JsonDocument` (v7)
- **No `fmt.Println`** — MUST use `log/slog` for all logging
- **No `String` class overuse in firmware** — prefer `char[]` buffers to avoid heap fragmentation
- **No state recovery on WS reconnect** — firmware goes straight to IDLE
- **No `MaxLength` on tool params without verification** — fall back to manual validation in handler
- **No screen flicker** — MUST use M5Canvas double-buffering for all UI rendering
- **No AI slop**: excessive comments, over-abstraction, generic names, unnecessary wrapper functions

---

## Verification Strategy (MANDATORY)

> **ZERO HUMAN INTERVENTION** — ALL verification is agent-executed. No exceptions.

### Test Decision
- **Infrastructure exists**: NO (new project)
- **Automated tests**: YES (TDD) — Go unit tests + integration test
- **Framework**: `go test` with `-race` flag
- **TDD**: Each Go component follows RED (failing test) → GREEN (minimal impl) → REFACTOR

### QA Policy
Every task MUST include agent-executed QA scenarios.
Evidence saved to `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`.

- **Go server**: Use `go test -race -v ./...` for unit/integration tests
- **Go binary**: Use `go build` + `stat` for size verification
- **Firmware**: Use `pio run -d firmware/ask_master` for compilation check
- **Linting**: Use `golangci-lint run ./...`

---

## Execution Strategy

### Parallel Execution Waves

```
Wave 1 (Start Immediately — foundation + scaffolding, 6 tasks):
├── T1: Go module setup (go.mod, go.sum) [quick]
├── T2: config.go + config_test.go (TDD) [quick]
├── T3: Firmware project scaffolding (platformio.ini, .gitignore) [quick]
├── T4: Firmware config.h (#define constants) [quick]
├── T5: Firmware ws_client.h (WebSocket wrapper) [quick]
└── T6: Internal helper: truncate.go + truncate_test.go (TDD) [quick]

Wave 2 (After Wave 1 — core implementations, 4 tasks):
├── T7: bridge.go + bridge_test.go (TDD) — depends: T1, T2, T6 [deep]
├── T8: Firmware ui.h + ui.cpp (4 draw functions) — depends: T3, T4 [visual-engineering]
├── T9: Firmware ask_master.ino (state machine, input, WS, beep) — depends: T4, T5, T8 [deep]
└── T10: tools.go + tools_test.go (TDD) — depends: T7 [deep]

Wave 3 (After Wave 2 — integration + wiring, 3 tasks):
├── T11: main.go (wire config + bridge + MCP server + shutdown) — depends: T7, T10 [quick]
├── T12: cmd/test-client/main.go (integration test) — depends: T7, T10, T11 [unspecified-high]
└── T13: Refactoring pass (bridge.go, tools.go) — depends: T7, T10 [quick]

Wave 4 (After Wave 3 — documentation + build, 5 tasks, all parallel):
├── T14: Makefile (build, install, test, lint targets) — depends: T13 [quick]
├── T15: README.md (setup guide + client configs) — depends: T11 [writing]
├── T16: docs/architecture.md (system diagram + data flow) — depends: T11 [writing]
├── T17: docs/client-setup.md (per-client config snippets) — depends: T11 [writing]
└── T18: Binary size & CGO verification — depends: T14 [quick]

Wave FINAL (After ALL tasks — 4 parallel reviews, then user okay):
├── F1: Plan compliance audit (oracle)
├── F2: Code quality review (unspecified-high)
├── F3: Real QA — unit tests, lint, build, firmware compile (unspecified-high)
└── F4: Scope fidelity check (deep)
→ Present results → Get explicit user okay

Critical Path: T1 → T7 → T10 → T11 → T14 → T18 → F1-F4 → user okay
Parallel Speedup: ~55% faster than sequential
Max Concurrent: 6 (Wave 1)
```

### Dependency Matrix

| Task | Depends On | Blocks | Wave |
|------|-----------|--------|------|
| T1 | — | T2, T7 | 1 |
| T2 | T1 | T7 | 1 |
| T3 | — | T8, T9 | 1 |
| T4 | — | T5, T8, T9 | 1 |
| T5 | T4 | T9 | 1 |
| T6 | — | T7 | 1 |
| T7 | T1, T2, T6 | T10, T11, T12 | 2 |
| T8 | T3, T4 | T9 | 2 |
| T9 | T4, T5, T8 | — | 2 |
| T10 | T7 | T11, T12 | 2 |
| T11 | T7, T10 | T12, T14-T17 | 3 |
| T12 | T7, T10, T11 | — | 3 |
| T13 | T7, T10 | T14 | 3 |
| T14 | T13 | T18 | 4 |
| T15 | T11 | — | 4 |
| T16 | T11 | — | 4 |
| T17 | T11 | — | 4 |
| T18 | T14 | F3 | 4 |

### Agent Dispatch Summary

- **Wave 1**: 6 tasks — T1-T4 → `quick`, T5 → `quick`, T6 → `quick`
- **Wave 2**: 4 tasks — T7 → `deep`, T8 → `visual-engineering`, T9 → `deep`, T10 → `deep`
- **Wave 3**: 3 tasks — T11 → `quick`, T12 → `unspecified-high`, T13 → `quick`
- **Wave 4**: 5 tasks — T14 → `quick`, T15-T17 → `writing`, T18 → `quick`
- **FINAL**: 4 tasks — F1 → `oracle`, F2 → `unspecified-high`, F3 → `unspecified-high`, F4 → `deep`

---

## TODOs

- [x] 1. Go Module Setup

  **What to do**:
  - Initialize Go module: `go mod init github.com/mhrsntrk/ask-master`
  - Add dependencies: `go get github.com/mark3labs/mcp-go@latest` and `go get github.com/gorilla/websocket@v1.5.3`
  - Run `go mod tidy` to resolve transitive deps
  - Verify module builds: `go build .`
  - Create `.gitignore` for Go (忽略 binary, test binary)

  **Must NOT do**:
  - Do NOT add dependencies beyond mcp-go and gorilla/websocket
  - Do NOT add any source files yet — only go.mod and go.sum

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Single-file scaffolding, just module init + deps
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with T2-T6)
  - **Blocks**: T2, T7
  - **Blocked By**: None (can start immediately)

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:116-120` — Dependencies: mcp-go v0.32.0+, gorilla/websocket v1.5.3+

  **API/Type References**:
  - `docs/mvp-prd.md:88-107` — Repository structure showing go.mod location

  **WHY Each Reference Matters**:
  - Module path must match repository structure; PRD §3 shows root-level go.mod
  - Only two external deps allowed — no additional libraries

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Go module initializes and resolves deps
    Tool: Bash
    Preconditions: Project directory exists with go.mod
    Steps:
      1. Run: `cd /Users/mhrsntrk/Developer/ask-master && go mod tidy`
      2. Run: `go build .`
    Expected Result: Both commands exit with code 0, no errors
    Failure Indicators: "cannot find module", "build fails"
    Evidence: .sisyphus/evidence/task-1-go-mod-init.txt

  Scenario: Only required deps present
    Tool: Bash
    Preconditions: go.mod exists
    Steps:
      1. Run: `grep -c 'require' go.mod`
      2. Verify go.sum contains mcp-go and gorilla/websocket entries
    Expected Result: Both dependencies present, no extra deps beyond transitive
    Failure Indicators: Extra direct dependencies beyond the two specified
    Evidence: .sisyphus/evidence/task-1-deps-check.txt
  ```

  **Commit**: YES (groups with T2-T6)
  - Message: `feat(server+firmware): scaffold project structure and config`
  - Files: `go.mod, go.sum, .gitignore`

- [x] 2. config.go + config_test.go (TDD)

  **What to do**:
  - Define `Config` struct with fields: `WSAddr string`, `Timeout time.Duration`, `LogLevel slog.Level`, `Version string`
  - Parse CLI flags using `flag` package: `--ws-addr` (default `0.0.0.0:8765`), `--timeout` (default `300` seconds), `--log-level` (default `info`), `--version` (prints version and exits)
  - Implement `ParseConfig() *Config` function that reads flags and returns populated Config
  - Implement `SetupLogger(level slog.Level)` function that configures `log/slog` with JSON handler at the given level
  - Write `config_test.go` with tests:
    - `TestParseConfig_Defaults` — verify default values
    - `TestParseConfig_CustomFlags` — verify custom flags parse correctly
    - `TestSetupLogger_Levels` — verify logger outputs at correct levels
    - `TestVersionFlag` — verify --version prints and exits
  - Follow TDD: write tests first (RED), then implementation (GREEN)

  **Must NOT do**:
  - Do NOT use any third-party logging library — use only `log/slog`
  - Do NOT use `fmt.Println` for logging — always use `slog.Info/Warn/Error/Debug`
  - Do NOT add flag aliases beyond what's in the PRD

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Standard library only, well-defined struct + flags
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with T1, T3-T6)
  - **Blocks**: T7
  - **Blocked By**: T1 (go.mod must exist)

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:126-131` — Config flags: --ws-addr, --timeout, --log-level, --version with defaults

  **External References**:
  - Go standard library `flag` package: https://pkg.go.dev/flag
  - Go `log/slog` package: https://pkg.go.dev/log/slog

  **WHY Each Reference Matters**:
  - PRD §4.2 defines exact flag names, defaults, and types — must match precisely
  - `log/slog` is the stdlib structured logger — zero external deps

  **Acceptance Criteria**:

  **If TDD (tests enabled):**
  - [ ] Test file created: config_test.go
  - [ ] `go test -race -v ./... -run TestParseConfig` → PASS (4+ tests, 0 failures)

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Config defaults match PRD specification
    Tool: Bash
    Preconditions: config.go and config_test.go exist
    Steps:
      1. Run: `go test -race -v -run TestParseConfig_Defaults ./...`
      2. Verify WSAddr defaults to "0.0.0.0:8765"
      3. Verify Timeout defaults to 300s
      4. Verify LogLevel defaults to slog.LevelInfo
    Expected Result: All assertions pass, defaults match PRD §4.2 exactly
    Failure Indicators: "expected 0.0.0.0:8.0.0.0", "expected 300", "expected LevelInfo"
    Evidence: .sisyphus/evidence/task-2-config-defaults.txt

  Scenario: Custom flags override defaults
    Tool: Bash
    Preconditions: config.go exists
    Steps:
      1. Run: `go test -race -v -run TestParseConfig_CustomFlags ./...`
    Expected Result: Custom --ws-addr, --timeout, --log-level values are parsed correctly
    Failure Indicators: Custom values ignored, parsing errors
    Evidence: .sisyphus/evidence/task-2-custom-flags.txt

  Scenario: Version flag prints and exits
    Tool: Bash
    Preconditions: config.go exists
    Steps:
      1. Run: `go test -race -v -run TestVersionFlag ./...`
    Expected Result: --version causes program to print version and exit with code 0
    Failure Indicators: Version not printed, program continues running
    Evidence: .sisyphus/evidence/task-2-version-flag.txt
  ```

  **Commit**: YES (groups with T1, T3-T6)
  - Message: `feat(server+firmware): scaffold project structure and config`
  - Files: `config.go, config_test.go`

- [x] 3. Firmware Project Scaffolding

  **What to do**:
  - Create `firmware/ask_master/` directory structure
  - Create `platformio.ini` with ESP32-S3 board config, framework = arduino, lib_deps for M5Cardputer, arduinoWebSockets, ArduinoJson
  - Create `firmware/.gitignore` for PlatformIO build artifacts
  - Create empty `firmware/ask_master/ask_master.ino` placeholder
  - Verify PlatformIO CLI is available or document installation requirement

  **Must NOT do**:
  - Do NOT implement any firmware logic — scaffolding only
  - Do NOT hardcode WiFi credentials in platformio.ini (those go in config.h)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Directory + config file creation, no logic
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with T1-T2, T4-T6)
  - **Blocks**: T8, T9
  - **Blocked By**: None (can start immediately)

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:88-107` — Firmware directory structure

  **External References**:
  - PlatformIO ESP32-S3 config: https://docs.platformio.org/en/latest/boards/espressif32/esp32-s3-devkitc-1.html
  - M5Cardputer library: https://github.com/m5stack/M5Cardputer
  - ArduinoWebSockets: https://github.com/Links2004/arduinoWebSockets

  **WHY Each Reference Matters**:
  - PRD §3 shows exact directory layout: firmware/ask_master/ with .ino, ui.h, ui.cpp, ws_client.h, config.h
  - Board must be esp32-s3-devkitc-1 for Cardputer ADV

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: PlatformIO project structure is valid
    Tool: Bash
    Preconditions: platformio.ini exists
    Steps:
      1. Run: `ls -la firmware/ask_master/`
      2. Verify ask_master.ino, platformio.ini exist
      3. Run: `cat firmware/ask_master/platformio.ini`
      4. Verify it contains [env:m5stack-cardputer] or similar section with board = esp32-s3-devkitc-1
    Expected Result: Directory structure matches PRD §3, platformio.ini has correct board config
    Failure Indicators: Missing directories, wrong board settings
    Evidence: .sisyphus/evidence/task-3-firmware-scaffold.txt

  Scenario: Library dependencies are declared
    Tool: Bash
    Preconditions: platformio.ini exists
    Steps:
      1. Run: `grep -A5 'lib_deps' firmware/ask_master/platformio.ini`
      2. Verify M5Cardputer, arduinoWebSockets, ArduinoJson are listed
    Expected Result: All three firmware libraries appear in lib_deps
    Failure Indicators: Missing library dependencies
    Evidence: .sisyphus/evidence/task-3-lib-deps.txt
  ```

  **Commit**: YES (groups with T1-T6)
  - Message: `feat(server+firmware): scaffold project structure and config`
  - Files: `firmware/ask_master/platformio.ini, firmware/.gitignore, firmware/ask_master/ask_master.ino`

- [x] 4. Firmware config.h

  **What to do**:
  - Create `firmware/ask_master/config.h` with `#define` constants:
    - `WIFI_SSID` — WiFi SSID (placeholder `"YourSSID"`)
    - `WIFI_PASSWORD` — WiFi password (placeholder `"YourPassword"`)
    - `WS_HOST` — WebSocket server IP (placeholder `"192.168.1.X"`)
    - `WS_PORT` — WebSocket port (`8765`)
    - `WS_RECONNECT_MS` — Reconnect interval (`3000`)
    - `BEEP_FREQ_ASK` — 1000 Hz
    - `BEEP_FREQ_CONFIRM` — 1300 Hz
    - `BEEP_FREQ_CHOOSE` — 900 Hz
    - `BEEP_DURATION_MS` — 150 ms
    - `BEEP_ANSWER_FREQ` — 1400 Hz
    - `BEEP_ANSWER_DURATION_MS` — 80 ms
  - Add `#ifndef CONFIG_H` / `#define CONFIG_H` / `#endif` include guard

  **Must NOT do**:
  - Do NOT implement any logic — only `#define` constants
  - Do NOT hardcode real WiFi credentials (use placeholders)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Pure #define header, no logic
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with T1-T3, T5-T6)
  - **Blocks**: T5, T8, T9
  - **Blocked By**: None (can start immediately)

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:279-289` — Exact config.h defines with names and values

  **WHY Each Reference Matters**:
  - PRD §5.3 specifies every `#define` name and value — must match exactly

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: config.h has all required defines with correct values
    Tool: Bash
    Preconditions: config.h exists
    Steps:
      1. Run: `grep -c '#define' firmware/ask_master/config.h`
      2. Verify count is 11 (WIFI_SSID, WIFI_PASSWORD, WS_HOST, WS_PORT, WS_RECONNECT_MS, BEEP_FREQ_ASK, BEEP_FREQ_CONFIRM, BEEP_FREQ_CHOOSE, BEEP_DURATION_MS, BEEP_ANSWER_FREQ, BEEP_ANSWER_DURATION_MS)
      3. Verify WS_PORT = 8765, WS_RECONNECT_MS = 3000
      4. Verify beep frequencies match PRD §5.8
    Expected Result: All 11 defines present with PRD-specified values
    Failure Indicators: Missing defines, wrong values
    Evidence: .sisyphus/evidence/task-4-config-h.txt

  Scenario: Include guard present
    Tool: Bash
    Preconditions: config.h exists
    Steps:
      1. Run: `head -1 firmware/ask_master/config.h` — expect `#ifndef CONFIG_H`
      2. Run: `tail -1 firmware/ask_master/config.h` — expect `#endif`
    Expected Result: Include guard wraps entire file
    Failure Indicators: No include guard
    Evidence: .sisyphus/evidence/task-4-include-guard.txt
  ```

  **Commit**: YES (groups with T1-T6)
  - Message: `feat(server+firmware): scaffold project structure and config`
  - Files: `firmware/ask_master/config.h`

- [x] 5. Firmware ws_client.h

  **What to do**:
  - Create `firmware/ask_master/ws_client.h` implementing a WebSocket client wrapper class
  - Use the Links2004/arduinoWebSockets library (`WebSocketsClient`)
  - Class `WSClient` with:
    - `void begin(const char* host, uint16_t port)` — connect to WebSocket server
    - `void loop()` — must be called regularly in `loop()`
    - `void send(const String& message)` — send text message
    - `bool isConnected()` — check connection state
    - `void onMessage(void(*)(const String&))` — register message callback
    - `void onDisconnect(void(*)())` — register disconnect callback
    - `void onConnect(void(*)())` — register connect callback
  - Auto-reconnect: delegate to library's built-in reconnect (`setReconnectInterval(WS_RECONNECT_MS)`)
  - Include `config.h` for `WS_HOST`, `WS_PORT`, `WS_RECONNECT_MS`

  **Must NOT do**:
  - Do NOT implement state machine logic (that goes in .ino)
  - Do NOT implement UI rendering (that goes in ui.h)
  - Do NOT use `String` class for internal buffers (avoid heap fragmentation) — use `char[]` where possible, `String` only for send where library requires it
  - Do NOT add TLS/WSS support (out of scope per PRD §11)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Wrapper class over well-known library, no complex logic
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with T1-T4, T6)
  - **Blocks**: T9
  - **Blocked By**: T4 (config.h must exist for WS define constants)

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:364-369` — WebSocket reconnect specification (3s interval, re-render idle on reconnect, no re-send of previous payload)

  **External References**:
  - Links2004/arduinoWebSockets: https://github.com/Links2004/arduinoWebSentials — API: `begin(host, port, path)`, `onEvent(cb)`, `sendTXT()`, `loop()`, `setReconnectInterval()`

  **WHY Each Reference Matters**:
  - PRD §5.7 specifies auto-reconnect with 3s interval — must use library's built-in reconnect
  - Library API dictates wrapper class method signatures

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: ws_client.h compiles with PlatformIO
    Tool: Bash
    Preconditions: ws_client.h exists with full implementation
    Steps:
      1. Run: `pio run -d firmware/ask_master 2>&1 | tail -20`
      2. Verify compilation succeeds (no errors, may have unused-variable warnings in .ino since it's a placeholder)
    Expected Result: Build completes with [SUCCESS] or only warnings in .ino placeholder
    Failure Indicators: Compilation errors in ws_client.h
    Evidence: .sisyphus/evidence/task-5-ws-client-compile.txt

  Scenario: ws_client.h includes config.h and uses WS defines
    Tool: Bash
    Preconditions: ws_client.h exists
    Steps:
      1. Run: `grep '#include "config.h"' firmware/ask_master/ws_client.h`
      2. Run: `grep 'WS_RECONNECT_MS' firmware/ask_master/ws_client.h`
    Expected Result: config.h is included, WS_RECONNECT_MS constant is used
    Failure Indicators: Missing include or unused config constant
    Evidence: .sisyphus/evidence/task-5-ws-includes.txt
  ```

  **Commit**: YES (groups with T1-T6)
  - Message: `feat(server+firmware): scaffold project structure and config`
  - Files: `firmware/ask_master/ws_client.h`

- [x] 6. Internal Helper: truncate.go + truncate_test.go (TDD)

  **What to do**:
  - Create `internal/truncate/truncate.go` with utility functions:
    - `func String(s string, maxLen int) string` — truncate string to maxLen chars, respecting rune boundaries (no half-characters for multibyte UTF-8)
    - `func StringWithEllipsis(s string, maxLen int) string` — truncate and append "..." if truncated (ellipsis counts toward maxLen)
  - Create `internal/truncate/truncate_test.go` with tests from PRD §9.1:
    - `TestTruncate_EmptyString` — "" → ""
    - `TestTruncate_ExactBoundary` — 120-char string stays 120 chars
    - `TestTruncate_MultibyteRunes` — "日本語日本語日本語" truncation respects rune boundaries (no 变 surrogate splitting)
    - `TestTruncate_ShortString` — string shorter than maxLen is unchanged
    - `TestTruncateWithEllipsis_Truncated` — "hello world" with maxLen=8 → "hello..."
    - `TestTruncateWithEllipsis_NotTruncated` — "hi" with maxLen=10 → "hi"
  - Follow TDD: write tests first (RED), then implement (GREEN)

  **Must NOT do**:
  - Do NOT use `rune` conversion for the entire string (inefficient for long strings) — find the cut point using `utf8.RuneCountInString` + `utf8.DecodeRuneInString`
  - Do NOT pack this into bridge.go or tools.go — it's a separate concern

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Small utility package, well-defined function signatures
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 1 (with T1-T5)
  - **Blocks**: T7 (bridge uses truncation)
  - **Blocked By**: T1 (go.mod must exist)

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:475-476` — TestTruncate test cases: empty string, exact boundary, multibyte runes

  **External References**:
  - Go `unicode/utf8` package: https://pkg.go.dev/unicode/utf8 — RuneCountInString, DecodeRuneInString

  **WHY Each Reference Matters**:
  - PRD §9.1 specifies exact test cases that must pass
  - UTF-8 rune boundary handling is critical — truncating mid-rune would corrupt display

  **Acceptance Criteria**:

  **If TDD (tests enabled):**
  - [ ] Test file created: internal/truncate/truncate_test.go
  - [ ] `go test -race -v ./internal/truncate/...` → PASS (6+ tests, 0 failures)

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: All truncate tests pass with race detection
    Tool: Bash
    Preconditions: truncate.go and truncate_test.go exist
    Steps:
      1. Run: `go test -race -v ./internal/truncate/...`
      2. Verify all test cases pass
      3. Verify no race conditions detected
    Expected Result: 6+ tests PASS, 0 failures, no race detector warnings
    Failure Indicators: Any test failure, race condition detected
    Evidence: .sisyphus/evidence/task-6-truncate-tests.txt

  Scenario: Multibyte rune truncation is safe
    Tool: Bash
    Preconditions: truncate_test.go exists
    Steps:
      1. Run: `go test -race -v -run TestTruncate_MultibyteRunes ./internal/truncate/...`
      2. Verify no "invalid UTF-8" or rune-splitting occurs
    Expected Result: Japanese/emoji strings truncate at rune boundaries, never mid-character
    Failure Indicators: "invalid UTF-8", garbled output, rune splitting
    Evidence: .sisyphus/evidence/task-6-multibyte-runes.txt
  ```

  **Commit**: YES (groups with T1-T6)
  - Message: `feat(server+firmware): scaffold project structure and config`
  - Files: `internal/truncate/truncate.go, internal/truncate/truncate_test.go`

- [x] 7. bridge.go + bridge_test.go (TDD)

  **What to do**:
  - Define `Bridger` interface (for testability):
    ```go
    type Bridger interface {
        Connected() bool
        SendAndWait(payload string, questionType string, options []string, timeout time.Duration) (string, error)
    }
    ```
  - Implement `Bridge` struct in `bridge.go`:
    ```go
    type Bridge struct {
        mu              sync.Mutex
        writeMu         sync.Mutex     // dedicated write mutex for gorilla/websocket
        conn            *websocket.Conn // nil when disconnected
        pending         chan string     // buffered(1): reply from Cardputer
        currentType     string         // "ask" | "confirm" | "choose"
        currentOptions  []string       // options for choose tool (digit-to-text mapping)
        shutdownCtx     context.Context
        shutdownCancel  context.CancelFunc
        logger          *slog.Logger
    }
    ```
  - Methods:
    - `NewBridge(logger *slog.Logger) *Bridge` — constructor, creates context for shutdown
    - `Connected() bool` — thread-safe check if Cardputer is connected
    - `SendAndWait(payload string, questionType string, options []string, timeout time.Duration) (string, error)` — serialize payload, send via WS, block for reply; for "choose", map digit reply back to option text; uses `mu` to serialize concurrent calls
    - `wsHandler(w http.ResponseWriter, r *http.Request)` — HTTP handler that upgrades to WebSocket, sets `conn`, reads messages in loop, calls `receive()`
    - `receive(msg string)` — resolves pending channel with message; for "choose" type, maps digit to option text
    - `Start(addr string) error` — starts `http.Server` with wsHandler, graceful shutdown via `shutdownCtx`
  - Upgrader: `websocket.Upgrader{CheckOrigin: func(r *http.Request) bool { return true }, ReadLimit: 4096}`
  - Write pattern: acquire `writeMu` before `conn.WriteJSON()`, release after
  - Graceful shutdown: on `shutdownCtx.Done()`, call `http.Server.Shutdown(ctx)` with 5s deadline
  - Error cases (per PRD §4.3):
    - Not connected → return offline message (not error)
    - Write failed → close conn, return error
    - Timeout → return timeout error
    - Disconnect mid-question → close channel, return error
  - Write `bridge_test.go` with ALL test cases from PRD §9.1:
    - `TestBridgeOffline_askHuman` — returns "[CARDPUTER OFFLINE] Please answer manually: {question}"
    - `TestBridgeOffline_confirm` — returns "false"
    - `TestBridgeOffline_choose` — returns options[0]
    - `TestBridgeSendAndWait_success` — mock WS sends reply, verify correct text
    - `TestBridgeSendAndWait_timeout` — verify error after timeout duration
    - `TestBridgeDisconnectMidQuestion` — close conn during wait, verify error propagation
    - `TestChooseDigitMapping` — digit "2" maps to options[1] text
    - `TestChooseOutOfRangeDigit` — digit "0" or "7" returns error
    - `TestConfirmResponseMapping` — verify "y" → "true", "n" → "false" (handled in tools.go, but Bridge sends raw)
    - `TestConcurrentToolCalls` — second call blocks until first completes (using mutex)
    - `TestWebSocketWriteConcurrency` — no data race detected with `-race`
  - Use a mock WebSocket connection for tests (gorilla/websocket `httptest.Server` + `websocket.Dialer`)
  - Follow TDD: write test stubs first (RED), then implement (GREEN)

  **Must NOT do**:
  - Do NOT write to conn without holding `writeMu` — gorilla/websocket is NOT safe for concurrent writes
  - Do NOT store question state outside the Bridge's mutex protection
  - Do NOT forget to map choose digits back to option text in `receive()`
  - Do NOT use `fmt.Println` — use `slog` for all logging
  - Do NOT add MaxLength to WebSocket read limit beyond 4096 bytes

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Core concurrency primitive with mutex, channels, WebSocket, and 11 test cases — complexity warrants deep focus
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO — depends on T1, T2, T6
  - **Parallel Group**: Wave 2
  - **Blocks**: T10, T11, T12
  - **Blocked By**: T1 (go.mod), T2 (config.go), T6 (truncate.go)

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:135-158` — Bridge struct, methods, and error cases
  - `docs/mvp-prd.md:218-225` — JSON payload format (server → Cardputer)
  - `docs/mvp-prd.md:436-448` — Full error handling table

  **API/Type References**:
  - `docs/mvp-prd.md:116-120` — Dependencies: gorilla/websocket v1.5.3+
  - `docs/mvp-prd.md:126-131` — Config struct fields used by Bridge

  **External References**:
  - gorilla/websocket Upgrader: https://pkg.go.dev/github.com/gorilla/websocket@v1.5.3#Upgrader — CheckOrigin, ReadBufferSize, WriteBufferSize
  - gorilla/websocket chat example: https://github.com/gorilla/websocket/blob/main/examples/chat/ — canonical read/write pump pattern

  **WHY Each Reference Matters**:
  - PRD §4.3 defines the exact Bridge struct, methods, and error behavior — must match exactly
  - gorilla/websocket requires ONE writer goroutine — `writeMu` prevents data races
  - The `choose` digit mapping is a critical design point: Cardputer sends "2", server must return options[1] text
  - PRD §7 error table specifies every error scenario — all must be handled

  **Acceptance Criteria**:

  **If TDD (tests enabled):**
  - [ ] Test file created: bridge_test.go
  - [ ] `go test -race -v ./... -run TestBridge` → PASS (11 tests, 0 failures, 0 race conditions)

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: All Bridge tests pass with race detection
    Tool: Bash
    Preconditions: bridge.go and bridge_test.go exist
    Steps:
      1. Run: `go test -race -v -run TestBridge ./...`
      2. Verify all 11 test cases pass
      3. Verify no race conditions detected
    Expected Result: 11 tests PASS, 0 failures, 0 race detector warnings
    Failure Indicators: Any test failure, "DATA RACE" in output
    Evidence: .sisyphus/evidence/task-7-bridge-tests.txt

  Scenario: Bridge offline behavior returns correct defaults
    Tool: Bash
    Preconditions: bridge_test.go exists
    Steps:
      1. Run: `go test -race -v -run "TestBridgeOffline" ./...`
      2. Verify ask_human offline returns "[CARDPUTER OFFLINE] Please answer manually: {question}"
      3. Verify confirm offline returns "false"
      4. Verify choose offline returns options[0]
    Expected Result: All three offline scenarios return PRD-specified fallbacks
    Failure Indicators: Wrong offline messages, panic, or crash
    Evidence: .sisyphus/evidence/task-7-offline-behavior.txt

  Scenario: Choose digit mapping works correctly
    Tool: Bash
    Preconditions: bridge_test.go exists
    Steps:
      1. Run: `go test -race -v -run "TestChooseDigitMapping" ./...`
      2. Verify digit "1" → options[0], "2" → options[1], "3" → options[2]
    Expected Result: Digit strings correctly map to their respective option texts
    Failure Indicators: Wrong mapping, index out of range
    Evidence: .sisyphus/evidence/task-7-choose-mapping.txt

  Scenario: Concurrent tool calls are serialized
    Tool: Bash
    Preconditions: bridge_test.go exists
    Steps:
      1. Run: `go test -race -v -run "TestConcurrentToolCalls" ./...`
      2. Verify second call blocks until first completes
      3. Verify no data races
    Expected Result: Second call waits, no DATA RACE warnings
    Failure Indicators: Race detected, second call doesn't block
    Evidence: .sisyphus/evidence/task-7-concurrency.txt
  ```

  **Commit**: YES
  - Message: `feat(server): implement Bridge with WebSocket send/receive`
  - Files: `bridge.go, bridge_test.go`
  - Pre-commit: `go test -race ./...`

- [x] 8. Firmware ui.h + ui.cpp (4 Draw Functions)

  **What to do**:
  - Create `firmware/ask_master/ui.h` declaring:
    - `void drawIdleScreen(const char* version, const char* ip)` — idle screen with version + IP
    - `void drawAskScreen(const char* question, const char* context, const char* inputBuffer)` — ask UI with navy header, yellow question, input buffer
    - `void drawConfirmScreen(const char* statement, const char* consequence)` — confirm UI with maroon header, orange statement, Y/N keys
    - `void drawChooseScreen(const char* question, const char* context, const String options[], int optionCount)` — choose UI with dark cyan header, numbered options
  - Create `firmware/ask_master/ui.cpp` implementing all 4 functions using M5GFX:
    - Use `M5Canvas` for double-buffered rendering (push sprite after drawing to prevent flicker)
    - Display is 240×135 pixels (landscape rotation)
    - Use `M5Cardputer.Display.setRotation(1)` for landscape
    - Text size 1 for content, consider size 1 or 2 for headers
    - Text wrapping at 38 characters per line (per PRD §5.5)
    - Color scheme per PRD §5.5: Navy (#000080) ask header, Maroon (#800000) confirm header, Dark Cyan (#008080) choose header
    - `drawAskScreen` shows: header bar, question lines (yellow), context (dark grey), separator, input buffer with cursor
    - `drawConfirmScreen` shows: header bar, statement (orange), consequence (dark grey), Y/N keys highlighted
    - `drawChooseScreen` shows: header bar, question (cyan), context (dark grey), numbered options list
    - `drawIdleScreen` shows: version, "Waiting for agent...", server IP
  - Include `#include "config.h"` for version constant

  **Must NOT do**:
  - Do NOT render directly to display — always use M5Canvas for double-buffering
  - Do NOT hardcode screen dimensions — use `M5Cardputer.Display.width()` and `.height()`
  - Do NOT use `String` class for output — use `char[]` buffers where possible (only `String[]` for choose options since ArduinoJson uses String internally)
  - Do NOT add fonts beyond what M5GFX provides by default
  - Do NOT implement screen overflow protection — truncate at 38 chars per line using simple logic

  **Recommended Agent Profile**:
  - **Category**: `visual-engineering`
    - Reason: Rendering UI on constrained display (240×135) requires pixel-level layout attention
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES — independent of Go server tasks
  - **Parallel Group**: Wave 2 (with T7, T9, T10)
  - **Blocks**: T9
  - **Blocked By**: T3 (firmware scaffolding), T4 (config.h for constants)

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:305-350` — Exact UI mockups for all 4 screens with colors and layouts
  - `docs/mvp-prd.md:352-363` — Input handling rules per tool type

  **External References**:
  - M5GFX API: https://github.com/m5stack/M5GFX — drawRect, fillRect, drawString, setTextSize, setTextColor, M5Canvas
  - M5Cardputer display: 240×135 pixels, ST7789V2, landscape rotation=1

  **WHY Each Reference Matters**:
  - PRD §5.5 provides pixel-perfect ASCII mockups — must match layout and colors exactly
  - Double-buffering is critical — direct rendering causes visible flicker on ST7789
  - 38-char line width matches display width at text size 1

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Firmware compiles with ui.h + ui.cpp
    Tool: Bash
    Preconditions: ui.h and ui.cpp exist in firmware/ask_master/
    Steps:
      1. Run: `pio run -d firmware/ask_master 2>&1 | tail -30`
    Expected Result: Build completes with [SUCCESS] status
    Failure Indicators: Compilation errors in ui.h or ui.cpp
    Evidence: .sisyphus/evidence/task-8-ui-compile.txt

  Scenario: All 4 draw functions are declared in ui.h
    Tool: Bash
    Preconditions: ui.h exists
    Steps:
      1. Run: `grep -c "void draw" firmware/ask_master/ui.h`
      2. Verify count is 4 (drawIdleScreen, drawAskScreen, drawConfirmScreen, drawChooseScreen)
    Expected Result: Exactly 4 draw function declarations
    Failure Indicators: Missing functions, wrong signatures
    Evidence: .sisyphus/evidence/task-8-ui-headers.txt

  Scenario: M5Canvas is used for double-buffering
    Tool: Bash
    Preconditions: ui.cpp exists
    Steps:
      1. Run: `grep -c "M5Canvas" firmware/ask_master/ui.cpp`
      2. Verify M5Canvas is used in each draw function
    Expected Result: M5Canvas used in all 4 draw functions
    Failure Indicators: Direct rendering to M5Cardputer.Display without canvas
    Evidence: .sisyphus/evidence/task-8-m5canvas.txt
  ```

  **Commit**: YES
  - Message: `feat(firmware): implement UI rendering functions`
  - Files: `firmware/ask_master/ui.h, firmware/ask_master/ui.cpp`

- [x] 9. Firmware ask_master.ino (State Machine + Input + WS + Beep)

  **What to do**:
  - Implement main Arduino sketch `firmware/ask_master/ask_master.ino`:
    - `setup()`: Initialize M5Cardputer (with keyboard), configure display rotation, connect WiFi (with retry loop and "Connecting..." screen), initialize `WSClient`, connect to server
    - `loop()`: Call `M5Cardputer.update()`, call `wsClient.loop()`, handle state machine transitions
  - State machine (per PRD §5.4):
    ```
    IDLE → receives WS message → RENDERING
    RENDERING → drawUI() done → WAITING_INPUT
    WAITING_INPUT → Enter/Y/N/digit → SENDING
    SENDING → ws.sendTXT() done → IDLE
    ```
    Additionally: `CONNECTING` state on boot and reconnect
  - WebSocket event handling in main sketch:
    - `WStype_TEXT`: Parse JSON payload (ArduinoJson 7.x `JsonDocument`), extract `type` field, switch on "ask"/"confirm"/"choose", call appropriate `drawXxxScreen()`, transition to WAITING_INPUT, play beep at frequency per tool type
    - `WStype_DISCONNECTED`: Transition to IDLE (show idle screen), auto-reconnect handled by ws_client.h
    - `WStype_CONNECTED`: Show idle screen with IP, log connection
  - Keyboard input handling (per PRD §5.6):
    - `ask` mode: printable chars append to buffer (max 80), Backspace/Del removes last char, Enter sends buffer
    - `confirm` mode: `y`/`Y` sends "y", `n`/`N` sends "n", all other keys ignored
    - `choose` mode: digit 1-6 (≤ optionCount) sends that digit string, all other keys ignored
  - Audio feedback (per PRD §5.8):
    - On question receive: beep at tool-specific frequency (ask=1000Hz, confirm=1300Hz, choose=900Hz) for 150ms
    - On answer sent: beep at 1400Hz for 80ms
    - Use `M5Cardputer.Speaker.tone(freq, duration)`
  - WiFi reconnect: on boot, retry WiFi connection in loop with "Connecting..." screen
  - On WS reconnect: re-render idle screen, do NOT re-send any previous payload (per PRD §5.7)

  **Must NOT do**:
  - Do NOT recover state on WS reconnect — go straight to IDLE
  - Do NOT use `String` class for the input buffer (causes heap fragmentation) — use `char inputBuffer[81]` with null terminator
  - Do NOT use `StaticJsonDocument` (ArduinoJson v6) — use `JsonDocument` (v7)
  - Do NOT render directly to display — use M5Canvas via ui.h functions
  - Do NOT add features from PRD §11 (no MicroSD, no Fn presets, etc.)
  - Do NOT use blocking `delay()` in `loop()` — use `millis()` based timing if needed

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: State machine with multiple modes, keyboard handling, WS JSON parsing, audio — complex embedded logic
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO — depends on T4, T5, T8
  - **Parallel Group**: Wave 2 (can start after T4, T5, T8 are done)
  - **Blocks**: None
  - **Blocked By**: T4 (config.h), T5 (ws_client.h), T8 (ui.h + ui.cpp)

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:297-363` — State machine, UI rendering, input handling rules
  - `docs/mvp-prd.md:364-379` — WebSocket reconnect and audio feedback
  - `docs/mvp-prd.md:218-225` — JSON payload format (what firmware receives)
  - `docs/mvp-prd.md:352-363` — Input handling rules per tool type

  **External References**:
  - M5Cardputer keyboard API: `M5Cardputer.Keyboard.keysState()` — `.word` for typed chars, `.del`, `.enter`
  - M5Cardputer speaker: `M5Cardputer.Speaker.tone(freq_hz, duration_ms)`
  - ArduinoJson 7.x: `JsonDocument` (heap allocated), `deserializeJson()`, `doc["type"].as<const char*>()`

  **WHY Each Reference Matters**:
  - State machine must match PRD §5.4 exactly — IDLE→RENDERING→WAITING_INPUT→SENDING→IDLE
  - Input rules per PRD §5.6 specify exactly which keys are valid per mode
  - JSON parsing must handle all three payload types: ask, confirm, choose
  - Audio frequencies per tool type must match PRD §5.8 exactly

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Firmware compiles successfully
    Tool: Bash
    Preconditions: ask_master.ino exists with full implementation
    Steps:
      1. Run: `pio run -d firmware/ask_master 2>&1 | tail -30`
      2. Check for [SUCCESS] status
    Expected Result: Build completes successfully, no compilation errors
    Failure Indicators: Compilation errors, missing includes, type mismatches
    Evidence: .sisyphus/evidence/task-9-firmware-compile.txt

  Scenario: Main sketch includes all required headers
    Tool: Bash
    Preconditions: ask_master.ino exists
    Steps:
      1. Run: `grep '#include' firmware/ask_master/ask_master.ino`
      2. Verify includes: M5Cardputer.h, config.h, ui.h, ws_client.h
    Expected Result: All 4 headers included
    Failure Indicators: Missing includes
    Evidence: .sisyphus/evidence/task-9-includes.txt

  Scenario: State machine constants defined
    Tool: Bash
    Preconditions: ask_master.ino exists
    Steps:
      1. Run: `grep -c 'IDLE\|RENDERING\|WAITING_INPUT\|SENDING\|CONNECTING' firmware/ask_master/ask_master.ino`
      2. Verify all 5 state constants present
    Expected Result: All 5 states present (IDLE, RENDERING, WAITING_INPUT, SENDING, CONNECTING)
    Failure Indicators: Missing states
    Evidence: .sisyphus/evidence/task-9-state-machine.txt

  Scenario: Audio beep calls present with correct frequencies
    Tool: Bash
    Preconditions: ask_master.ino exists
    Steps:
      1. Run: `grep 'Speaker.tone' firmware/ask_master/ask_master.ino`
      2. Verify beep at 1000Hz (ask), 1300Hz (confirm), 900Hz (choose), 1400Hz (answer sent)
      3. Verify BEEP_FREQ_* and BEEP_DURATION_MS constants from config.h are used
    Expected Result: 4 Speaker.tone calls with correct frequencies
    Failure Indicators: Wrong frequencies, hardcoded values instead of config.h constants
    Evidence: .sisyphus/evidence/task-9-beep-freqs.txt
  ```

  **Commit**: YES
  - Message: `feat(firmware): implement state machine, keyboard input, and WS handling`
  - Files: `firmware/ask_master/ask_master.ino`

- [x] 10. tools.go + tools_test.go (TDD)

  **What to do**:
  - Define three MCP tool registrations using `mcp.NewTool()` DSL:
    - `ask_human`: `mcp.WithString("question", mcp.Required(), mcp.Description("..."))`, `mcp.WithString("context", mcp.Description("..."))`, `mcp.WithInteger("timeout", mcp.Description("..."))`
    - `confirm`: `mcp.WithString("statement", mcp.Required(), mcp.Description("..."))`, `mcp.WithString("consequence", mcp.Description("..."))`
    - `choose`: `mcp.WithString("question", mcp.Required(), mcp.Description("..."))`, `mcp.WithArray("options", mcp.Required(), mcp.Description("..."))`, `mcp.WithString("context", mcp.Description("..."))`
  - Implement `RegisterTools(s *server.MCPServer, bridge Bridger, logger *slog.Logger)` function that registers all 3 tools
  - Implement handlers for each tool:
    - `ask_human`: Extract question, context, timeout from args using `req.RequireString("question")` + `req.GetString("context", "")` + `req.GetString("timeout", "300")`; truncate question to 120 chars; construct JSON `{"type":"ask","question":"...","context":"..."}`; call `bridge.SendAndWait(payload, "ask", nil, timeout)`; return `mcp.NewToolResultText(reply)`; on offline return `mcp.NewToolResultText("[CARDPUTER OFFLINE] Please answer manually: {question}")`; on timeout/other error return `mcp.NewToolResultError(msg)`
    - `confirm`: Extract statement/consequence; truncate to 120/60 chars; construct JSON `{"type":"confirm","question":"...","context":"..."}`; call `bridge.SendAndWait(payload, "confirm", nil, timeout)`; convert "y" → "true", "n" → "false"; on offline return `mcp.NewToolResultText("false")`; on error return `mcp.NewToolResultError(msg)`
    - `choose`: Extract question/context/options; validate 2 ≤ len(options) ≤ 6; truncate question to 100 chars, each option to 40 chars; construct JSON `{"type":"choose","question":"...","context":"...","options":[...]}`; call `bridge.SendAndWait(payload, "choose", options, timeout)`; return option text (Bridge maps digit to text); on offline return `mcp.NewToolResultText(options[0])`; on validation error return `mcp.NewToolResultError("options must have 2-6 items")`
  - **CRITICAL**: All tool-level errors MUST use `mcp.NewToolResultError(msg), nil` — NEVER return a Go `error` from handler (causes MCP session halt)
  - **CRITICAL**: Validate `choose` options count (2-6) before sending to device — return MCP error if out of range
  - **CRITICAL**: Manual string length validation in handlers (don't rely on mcp-go `MaxLength` without verification)
  - Write `tools_test.go` with test cases:
    - `TestAskHuman_Offline` — returns offline message
    - `TestAskHuman_Success` — mock bridge, verify JSON payload, verify text response
    - `TestAskHuman_Truncation` — question > 120 chars is truncated
    - `TestConfirm_Offline` — returns "false"
    - `TestConfirm_Success_Yes` — "y" → "true"
    - `TestConfirm_Success_No` — "n" → "false"
    - `TestChoose_Offline` — returns options[0]
    - `TestChoose_Success` — digit mapping works
    - `TestChoose_TooFewOptions` — 1 option returns MCP error
    - `TestChoose_TooManyOptions` — 7 options returns MCP error
    - `TestChoose_Truncation` — question > 100 chars, options > 40 chars truncated

  **Must NOT do**:
  - Do NOT return Go `error` from tool handlers — use `mcp.NewToolResultError(msg), nil` for all errors
  - Do NOT add `MaxLength` to tool param definitions without verifying current mcp-go supports it — use manual validation in handler body
  - Do NOT forget to convert confirm responses: "y" → "true", "n" → "false"
  - Do NOT forget to validate choose options count (2-6) before sending
  - Do NOT use `fmt.Println` — use `slog`

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Three tool handlers with validation, JSON construction, response mapping, and 11 test cases
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO — depends on T7 (Bridge/Bridger interface)
  - **Parallel Group**: Wave 2
  - **Blocks**: T11, T12
  - **Blocked By**: T7 (needs Bridger interface)

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:159-213` — Exact tool definitions: names, parameters, returns, offline behavior
  - `docs/mvp-prd.md:218-225` — JSON payload format (what gets sent to Cardputer)
  - `docs/mvp-prd.md:436-448` — Error handling table

  **API/Type References**:
  - `docs/mvp-prd.md:135-158` — Bridger interface and SendAndWait signature

  **External References**:
  - mcp-go tool DSL: `mcp.NewTool()`, `mcp.WithString()`, `mcp.Required()`, `mcp.Description()`, `mcp.WithArray()`
  - mcp-go result types: `mcp.NewToolResultText()`, `mcp.NewToolResultError()`

  **WHY Each Reference Matters**:
  - PRD §4.4 defines exact tool names, descriptions, parameters, return types, and offline behavior — must match precisely
  - mcp-go API has a critical pattern: tool errors must use NewToolResultError, NOT Go error
  - Choose validation (2-6 options) and digit-to-text mapping are specified in the PRD

  **Acceptance Criteria**:

  **If TDD (tests enabled):**
  - [ ] Test file created: tools_test.go
  - [ ] `go test -race -v ./... -run TestTool` → PASS (11+ tests, 0 failures)

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: All tool handler tests pass
    Tool: Bash
    Preconditions: tools.go and tools_test.go exist
    Steps:
      1. Run: `go test -race -v -run "Test(AskHuman|Confirm|Choose)" ./...`
      2. Verify all test cases pass
    Expected Result: 11+ tests PASS, 0 failures, no race conditions
    Failure Indicators: Any test failure, race detected
    Evidence: .sisyphus/evidence/task-10-tools-tests.txt

  Scenario: Choose validation rejects out-of-range options
    Tool: Bash
    Preconditions: tools_test.go exists
    Steps:
      1. Run: `go test -race -v -run "TestChoose_Too" ./...`
      2. Verify 1-option returns MCP error
      3. Verify 7-option returns MCP error
    Expected Result: Both validation tests pass, error messages are clear
    Failure Indicators: Validation not enforced, panic on edge cases
    Evidence: .sisyphus/evidence/task-10-choose-validation.txt

  Scenario: Confirm response mapping correct
    Tool: Bash
    Preconditions: tools_test.go exists
    Steps:
      1. Run: `go test -race -v -run "TestConfirm_Success" ./...`
      2. Verify "y" response → "true" output
      3. Verify "n" response → "false" output
    Expected Result: Both mapping tests pass
    Failure Indicators: Raw "y"/"n" returned instead of "true"/"false"
    Evidence: .sisyphus/evidence/task-10-confirm-mapping.txt

  Scenario: Ask offline message matches PRD
    Tool: Bash
    Preconditions: tools_test.go exists
    Steps:
      1. Run: `go test -race -v -run "TestAskHuman_Offline" ./...`
      2. Verify offline message is "[CARDPUTER OFFLINE] Please answer manually: {question}"
    Expected Result: Exact offline message format per PRD §4.4
    Failure Indicators: Wrong format, missing question in message
    Evidence: .sisyphus/evidence/task-10-ask-offline.txt
  ```

  **Commit**: YES
  - Message: `feat(server): implement ask_human, confirm, choose MCP tools`
  - Files: `tools.go, tools_test.go`
  - Pre-commit: `go test -race ./...`

- [x] 11. main.go (Wire Config + Bridge + MCP Server)

  **What to do**:
  - Implement `main.go` per PRD §4.6:
    1. Parse config flags (`ParseConfig()`)
    2. If `--version` flag, print version and exit
    3. Initialize `log/slog` logger with configured level (`SetupLogger()`)
    4. Initialize Bridge: `bridge := NewBridge(logger)`
    5. Start bridge in goroutine: `go func() { if err := bridge.Start(config.WSAddr); err != nil { logger.Error("bridge failed", "error", err) } }()`
    6. Create MCP server: `s := server.NewMCPServer("ask-master", version, server.WithToolCapabilities(true), server.WithRecovery())`
    7. Register tools: `RegisterTools(s, bridge, logger)`
    8. Set up graceful shutdown: use `os.Signal` handler for SIGTERM/SIGINT, call `bridge.Shutdown()` (which calls `http.Server.Shutdown(ctx)` with 5s deadline)
    9. Start serving: `server.ServeStdio(s)` — blocks until stdin closes
    10. On ServeStdio return, call `bridge.Shutdown()` to close WebSocket server
  - Define `version` variable set via `-ldflags` at build time: `var version = "dev"`
  - Graceful shutdown coordination:
    - `Bridge.Shutdown()` calls `shutdownCancel()` to signal all goroutines
    - Then calls `http.Server.Shutdown(ctx)` with 5-second deadline
    - Existing Cardputer connections are closed gracefully
  - Use `server.WithRecovery()` to prevent handler panics from crashing the server

  **Must NOT do**:
  - Do NOT skip graceful shutdown — `ServeStdio` return must trigger bridge shutdown
  - Do NOT use `log.Fatal` or `fmt.Println` for logging — use `slog`
  - Do NOT hardcode the version — use ldflags injection
  - Do NOT add signal handling that conflicts with `ServeStdio`'s built-in handling

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Wiring file, well-defined entry point from PRD §4.6
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO — depends on T7, T10
  - **Parallel Group**: Wave 3
  - **Blocks**: T12, T14-T17
  - **Blocked By**: T7 (bridge.go), T10 (tools.go)

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:229-237` — Exact main.go initialization sequence

  **API/Type References**:
  - `docs/mvp-prd.md:88-107` — Repository structure showing main.go location

  **External References**:
  - mcp-go server creation: `server.NewMCPServer(name, version, WithToolCapabilities(true), WithRecovery())`
  - mcp-go stdio: `server.ServeStdio(s)` — blocks until stdin closes

  **WHY Each Reference Matters**:
  - PRD §4.6 defines exact initialization order — must follow it precisely
  - Graceful shutdown ensures no orphaned WebSocket connections
  - `WithRecovery()` prevents handler panics from crashing the process

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Server starts and serves MCP via stdio
    Tool: Bash
    Preconditions: main.go exists, all packages compile
    Steps:
      1. Run: `go build -o ask-master .`
      2. Run: `echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"0.1"}}}' | timeout 5 ./ask-master --timeout 10 2>/dev/null || true`
      3. Check that binary starts and doesn't crash immediately
    Expected Result: Binary starts, responds to MCP initialize, exits cleanly on stdin close
    Failure Indicators: Panic, immediate exit, no response
    Evidence: .sisyphus/evidence/task-11-server-starts.txt

  Scenario: Binary built with version ldflags
    Tool: Bash
    Preconditions: main.go exists
    Steps:
      1. Run: `go build -ldflags="-X main.version=v0.1.0" -o ask-master .`
      2. Run: `./ask-master --version`
    Expected Result: Prints "v0.1.0" and exits with code 0
    Failure Indicators: Version not printed, exit code non-zero
    Evidence: .sisyphus/evidence/task-11-version-ldflags.txt

  Scenario: Graceful shutdown on stdin close
    Tool: Bash
    Preconditions: Binary built
    Steps:
      1. Run: `echo '' | timeout 3 ./ask-master 2>/dev/null; echo "Exit code: $?"`
    Expected Result: Process exits cleanly within 5s of stdin close, no zombie processes
    Failure Indicators: Process hangs, doesn't exit, kills with signal
    Evidence: .sisyphus/evidence/task-11-graceful-shutdown.txt
  ```

  **Commit**: YES
  - Message: `feat(server): wire config, bridge, and MCP server`
  - Files: `main.go`
  - Pre-commit: `go build .`

- [x] 12. Integration Test (cmd/test-client/main.go)

  **What to do**:
  - Create `cmd/test-client/main.go` as an integration test tool:
    - Start the bridge on a random available port
    - Connect a mock WebSocket client to the bridge
    - Use `mcptest` package from mcp-go to create an in-process MCP client
    - Test all three tools end-to-end:
      1. `ask_human("Which DB?", "3 options")` → mock WS sends reply "PostgreSQL" → verify MCP response is "PostgreSQL"
      2. `confirm("Delete all data?", "Irreversible")` → mock WS sends "y" → verify MCP response is "true"
      3. `choose("Which region?", ["us-east","eu-west","ap-south"], "")` → mock WS sends "2" → verify MCP response is "eu-west"
    - Test offline scenarios:
      1. `ask_human` without Cardputer connected → verify offline message
      2. `confirm` without Cardputer → verify "false"
      3. `choose` without Cardputer → verify first option
    - Test timeout scenario:
      1. `ask_human` with 1s timeout, no reply → verify timeout error
  - Alternatively, write a Go test file `integration_test.go` at the root level using `mcptest` package directly
  - Clean up: close WS client, shutdown bridge, verify no goroutine leaks

  **Must NOT do**:
  - Do NOT use subprocess testing — use in-process testing with `mcptest`
  - Do NOT test actual hardware or real WiFi — mock the WebSocket client
  - Do NOT skip offline/degraded testing — these are critical per PRD §7

  **Recommended Agent Profile**:
  - **Category**: `unspecified-high`
    - Reason: Integration test requires understanding of both MCP client and WebSocket mock coordination
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO — depends on T7, T10, T11
  - **Parallel Group**: Wave 3
  - **Blocks**: None
  - **Blocked By**: T7 (bridge.go), T10 (tools.go), T11 (main.go)

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:480-487` — Integration test specification (cmd/test-client)

  **External References**:
  - mcp-go `mcptest` package for in-process MCP client testing

  **WHY Each Reference Matters**:
  - PRD §9.2 defines exact integration test scenarios — must test all three tools + offline behavior
  - `mcptest` avoids subprocess overhead while testing real MCP protocol

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Integration tests pass end-to-end
    Tool: Bash
    Preconditions: cmd/test-client/main.go exists or integration_test.go
    Steps:
      1. Run: `go test -race -v -run Integration ./...`
      2. Verify all three tools return correct responses via mock WS
    Expected Result: All integration tests pass, no race conditions
    Failure Indicators: Tool responses don't match expected values, race detected
    Evidence: .sisyphus/evidence/task-12-integration-tests.txt

  Scenario: Offline degradation works correctly
    Tool: Bash
    Preconditions: integration tests exist
    Steps:
      1. Run: `go test -race -v -run "Integration.*Offline" ./...`
      2. Verify ask_human offline returns "[CARDPUTER OFFLINE]..." message
      3. Verify confirm offline returns "false"
      4. Verify choose offline returns options[0]
    Expected Result: All three offline scenarios return PRD-specified fallbacks
    Failure Indicators: Wrong offline messages, crashes, panics
    Evidence: .sisyphus/evidence/task-12-offline-tests.txt

  Scenario: Timeout returns error correctly
    Tool: Bash
    Preconditions: integration tests exist
    Steps:
      1. Run: `go test -race -v -run "Integration.*Timeout" ./...`
      2. Verify ask_human with 1s timeout and no WS reply returns timeout error
    Expected Result: Timeout error returned within ~1s tolerance
    Failure Indicators: Test hangs, no timeout error, wrong error format
    Evidence: .sisyphus/evidence/task-12-timeout-test.txt
  ```

  **Commit**: YES
  - Message: `test(server): add integration test client`
  - Files: `cmd/test-client/main.go` or `integration_test.go`

- [x] 13. Refactoring Pass (bridge.go, tools.go)

  **What to do**:
  - Review `bridge.go` for:
    - Remove any dead code or unnecessary abstractions
    - Verify all error paths return descriptive messages
    - Verify `writeMu` is used for ALL `conn.WriteJSON()` calls
    - Verify `mu` (question serialization mutex) is held for the entire SendAndWait lifecycle
    - Verify pending channel is properly cleaned up on disconnect/timeout
  - Review `tools.go` for:
    - Verify all tool errors use `mcp.NewToolResultError(msg), nil` — NOT Go `error`
    - Verify string truncation uses `internal/truncate` package
    - Verify confirm maps "y" → "true", "n" → "false" correctly
    - Verify choose validates 2-6 options and returns MCP error for out-of-range
    - Remove any unnecessary comments or AI slop
  - Run `go test -race -v ./...` to verify all tests still pass after refactoring
  - Run `golangci-lint run ./...` and fix any issues
  - Verify no `fmt.Println` or `log.Fatal` calls — all logging should be via `slog`

  **Must NOT do**:
  - Do NOT change public API signatures (Bridger interface must remain stable)
  - Do NOT remove any test cases
  - Do NOT add new features — only clean up existing implementation
  - Do NOT add over-abstracted interfaces or wrapper types (AI slop pattern)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Code review + minor cleanups, no new features
  - **Skills**: [`karpathy-guidelines`]
    - `karpathy-guidelines`: Anti-slop review — remove unnecessary comments, over-abstraction, generic names

  **Parallelization**:
  - **Can Run In Parallel**: YES — independent of T11/T12 after T7/T10 are done
  - **Parallel Group**: Wave 3 (with T11, T12)
  - **Blocks**: T14 (Makefile depends on clean codebase)
  - **Blocked By**: T7 (bridge.go), T10 (tools.go)

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:135-158` — Bridge struct specification (verify implementation matches)
  - `docs/mvp-prd.md:159-213` — Tool handler specifications (verify implementation matches)

  **WHY Each Reference Matters**:
  - Refactoring must not change behavior — PRD specs are the contract
  - AI slop check ensures production quality

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: All tests still pass after refactoring
    Tool: Bash
    Preconditions: Refactored code exists
    Steps:
      1. Run: `go test -race -v ./...`
      2. Verify all tests pass with same count as before
    Expected Result: All tests pass, no regressions
    Failure Indicators: Any test failure, race condition introduced
    Evidence: .sisyphus/evidence/task-13-refactor-tests.txt

  Scenario: Lint passes cleanly
    Tool: Bash
    Preconditions: Refactored code exists
    Steps:
      1. Run: `golangci-lint run ./...`
    Expected Result: No lint errors
    Failure Indicators: Any lint warnings or errors
    Evidence: .sisyphus/evidence/task-13-lint.txt

  Scenario: No fmt.Println in production code
    Tool: Bash
    Preconditions: Refactored code exists
    Steps:
      1. Run: `grep -rn 'fmt.Println\|log.Fatal\|log.Printf' *.go internal/**/*.go 2>/dev/null || true`
      2. Verify no results (all logging uses slog)
    Expected Result: Zero occurrences of fmt.Println, log.Fatal, log.Printf in production .go files
    Failure Indicators: Any occurrence found
    Evidence: .sisyphus/evidence/task-13-no-fmt-println.txt
  ```

  **Commit**: YES
  - Message: `refactor(server): clean up bridge and tools`
  - Files: `bridge.go, tools.go`
  - Pre-commit: `go test -race ./... && golangci-lint run ./...`

- [x] 14. Makefile

  **What to do**:
  - Create `Makefile` at project root with targets per PRD §4.7:
    - `build`: `go build -ldflags="-s -w -X main.version=$(VERSION)" -o ask-master .` (default VERSION=v0.1.0)
    - `install`: `go install -ldflags="-s -w -X main.version=$(VERSION)" .`
    - `test`: `go test ./... -race -v`
    - `lint`: `golangci-lint run ./...`
    - `firmware`: `pio run -d firmware/ask_master`
    - `clean`: `rm -f ask-master`
    - `all`: depends on `build lint test`
  - Use `VERSION ?= v0.1.0` variable for version injection

  **Must NOT do**:
  - Do NOT add targets for features from PRD §11 (no OTA, no WSS, etc.)
  - Do NOT add docker or CI targets unless explicitly requested

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Standard Makefile, well-defined targets
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4 (with T15-T18)
  - **Blocks**: T18
  - **Blocked By**: T13

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:240-253` — Exact Makefile targets with ldflags

  **WHY Each Reference Matters**:
  - PRD §4.7 specifies exact build, install, test, lint targets with specific ldflags

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Make build succeeds
    Tool: Bash
    Preconditions: Makefile exists
    Steps:
      1. Run: `make build`
      2. Verify `ask-master` binary is created
    Expected Result: Binary exists, no build errors
    Failure Indicators: Build fails, binary not created
    Evidence: .sisyphus/evidence/task-14-make-build.txt

  Scenario: Make test passes
    Tool: Bash
    Preconditions: Makefile exists
    Steps:
      1. Run: `make test`
    Expected Result: All tests pass with -race flag
    Failure Indicators: Any test failure
    Evidence: .sisyphus/evidence/task-14-make-test.txt

  Scenario: Make lint passes
    Tool: Bash
    Preconditions: Makefile exists
    Steps:
      1. Run: `make lint`
    Expected Result: No lint errors
    Failure Indicators: Any lint warnings/errors
    Evidence: .sisyphus/evidence/task-14-make-lint.txt
  ```

  **Commit**: YES
  - Message: `build(server): add Makefile with build, test, lint targets`
  - Files: `Makefile`

- [x] 15. README.md

  **What to do**:
  - Create `README.md` at project root with:
    - Project name and one-line description
    - Architecture diagram (ASCII, matching PRD §2.1)
    - Prerequisites: Go 1.22+, PlatformIO CLI (for firmware)
    - Quick Start: `make build` to build server, `pio run -d firmware/ask_master` for firmware
    - Running: MCP server config for all 4 clients (Claude Code, OpenCode, Cursor, Windsurf) with copy-paste JSON snippets from PRD §6
    - Configuration: CLI flags table from PRD §4.2
    - Firmware Setup: WiFi credentials in config.h, upload via PlatformIO
    - License placeholder

  **Must NOT do**:
  - Do NOT add badges, CI badges, or contribution guidelines (not requested)
  - Do NOT include incomplete sections — every section must have content

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: Documentation file, well-specified content from PRD
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4 (with T14, T16-T18)
  - **Blocks**: None
  - **Blocked By**: T11

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:1-63` — Overview, architecture, system diagram
  - `docs/mvp-prd.md:240-256` — Build instructions
  - `docs/mvp-prd.md:384-430` — Client configuration for all 4 agents

  **WHY Each Reference Matters**:
  - PRD §6 provides exact JSON config snippets for 4 MCP clients — must copy verbatim
  - Architecture diagram from §2.1 should be reproduced

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: README contains all required sections
    Tool: Bash
    Preconditions: README.md exists
    Steps:
      1. Run: `grep -c "^##" README.md`
      2. Verify sections: Overview, Architecture, Prerequisites, Quick Start, Configuration, Client Setup, Firmware
    Expected Result: At least 6 section headers present
    Failure Indicators: Missing sections
    Evidence: .sisyphus/evidence/task-15-readme-sections.txt

  Scenario: Client config snippets present for all 4 agents
    Tool: Bash
    Preconditions: README.md exists
    Steps:
      1. Run: `grep -c '"ask-master"' README.md`
      2. Verify all 4 client configs (Claude Code, OpenCode, Cursor, Windsurf) are present
    Expected Result: 4 occurrences of "ask-master" in config snippets
    Failure Indicators: Missing client configs, wrong JSON format
    Evidence: .sisyphus/evidence/task-15-client-configs.txt
  ```

  **Commit**: YES (groups with T16-T17)
  - Message: `docs: add README and setup documentation`
  - Files: `README.md`

- [x] 16. docs/architecture.md

  **What to do**:
  - Create `docs/architecture.md` with:
    - System diagram (ASCII, matching PRD §2.1) showing MCP client → Go server → WebSocket → Cardputer
    - Data flow description (happy path per PRD §2.2)
    - Bridge concurrency model (mutex, channel, one question at a time per PRD §2.3)
    - Error handling summary (table from PRD §7)
    - JSON payload format (from PRD §4.5)

  **Must NOT do**:
  - Do NOT add implementation details that belong in source code comments
  - Do NOT include code snippets longer than 10 lines

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: Architecture documentation, well-specified from PRD
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4 (with T14-T15, T17-T18)
  - **Blocks**: None
  - **Blocked By**: T11

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:37-84` — Architecture, system diagram, data flow, concurrency model
  - `docs/mvp-prd.md:436-448` — Error handling table

  **WHY Each Reference Matters**:
  - PRD §2 provides the canonical architecture — documentation should reflect the implemented system

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Architecture doc covers all required sections
    Tool: Bash
    Preconditions: docs/architecture.md exists
    Steps:
      1. Run: `grep -c "^##" docs/architecture.md`
      2. Verify sections: System Diagram, Data Flow, Concurrency, Error Handling, Payload Format
    Expected Result: At least 5 section headers
    Failure Indicators: Missing sections
    Evidence: .sisyphus/evidence/task-16-arch-sections.txt
  ```

  **Commit**: YES (groups with T15, T17)
  - Message: `docs: add architecture and client-setup documentation`
  - Files: `docs/architecture.md`

- [x] 17. docs/client-setup.md

  **What to do**:
  - Create `docs/client-setup.md` with per-client configuration:
    - Claude Code (`~/.claude/settings.json`) — exact JSON from PRD §6
    - OpenCode (`opencode.jsonc`) — exact JSON with timeout from PRD §6
    - Cursor (`~/.cursor/mcp.json`) — exact JSON from PRD §6
    - Windsurf (`~/.codeium/windsurf/mcp_config.json`) — exact JSON from PRD §6
    - Each section includes: file path, JSON config, notes about timeout/args
    - Common troubleshooting: "Cardputer not connecting", "Agent not seeing tools"

  **Must NOT do**:
  - Do NOT invent config keys or paths — use exact values from PRD §6
  - Do NOT add clients not in the PRD

  **Recommended Agent Profile**:
  - **Category**: `writing`
    - Reason: Documentation, copy-paste from PRD
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4 (with T14-T16, T18)
  - **Blocks**: None
  - **Blocked By**: T11

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:384-430` — Exact client configuration JSON for all 4 agents

  **WHY Each Reference Matters**:
  - PRD §6 provides exact JSON configs — must match byte-for-byte

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: All 4 client configs present and valid JSON
    Tool: Bash
    Preconditions: docs/client-setup.md exists
    Steps:
      1. Run: `grep -c '"ask-master"' docs/client-setup.md`
      2. Verify 4 client sections (Claude Code, OpenCode, Cursor, Windsurf)
    Expected Result: 4+ occurrences, each with valid JSON config
    Failure Indicators: Missing client, invalid JSON
    Evidence: .sisyphus/evidence/task-17-client-docs.txt
  ```

  **Commit**: YES (groups with T15-T16)
  - Message: `docs: add architecture and client-setup documentation`
  - Files: `docs/client-setup.md`

- [x] 18. Binary Size & CGO Verification

  **What to do**:
  - Build the binary with production ldflags: `make build VERSION=v0.1.0`
  - Verify binary size < 12MB: `stat -f%z ask-master` on macOS (or `stat -c%s` on Linux)
  - Verify zero CGO: `CGO_ENABLED=0 go build -ldflags="-s -w" -o ask-master .`
  - Run full test suite one final time: `make test`
  - Run linter: `make lint`
  - Verify firmware compilation: `make firmware`
  - Document all verification results in evidence files

  **Must NOT do**:
  - Do NOT skip any verification step
  - Do NOT modify source code to pass verification — only document failures for fixing

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Running verification commands, no code changes
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: YES
  - **Parallel Group**: Wave 4 (with T14-T17)
  - **Blocks**: F3
  - **Blocked By**: T14 (Makefile targets needed)

  **References**:
  **Pattern References**:
  - `docs/mvp-prd.md:254-256` — Binary size target: <12MB stripped, zero CGO
  - `docs/mvp-prd.md:240-253` — Build and test commands

  **WHY Each Reference Matters**:
  - PRD §8 specifies exact non-functional requirements that must be verified

  **Acceptance Criteria**:

  **QA Scenarios (MANDATORY)**:

  ```
  Scenario: Binary size under 12MB
    Tool: Bash
    Preconditions: Binary built with production ldflags
    Steps:
      1. Run: `make build VERSION=v0.1.0`
      2. Run: `stat -f%z ask-master`
      3. Verify result < 12582912 (12MB)
    Expected Result: Binary size less than 12MB
    Failure Indicators: Size >= 12MB — investigate dependencies
    Evidence: .sisyphus/evidence/task-18-binary-size.txt

  Scenario: Zero CGO build succeeds
    Tool: Bash
    Preconditions: Source code exists
    Steps:
      1. Run: `CGO_ENABLED=0 go build -ldflags="-s -w" -o ask-master .`
      2. Verify build succeeds
    Expected Result: Build completes successfully with CGO_ENABLED=0
    Failure Indicators: CGO dependency detected, build fails
    Evidence: .sisyphus/evidence/task-18-zero-cgo.txt

  Scenario: Full test suite passes with race detection
    Tool: Bash
    Preconditions: All tests written
    Steps:
      1. Run: `make test`
    Expected Result: All tests pass, 0 race conditions
    Failure Indicators: Any test failure, DATA RACE detected
    Evidence: .sisyphus/evidence/task-18-test-suite.txt

  Scenario: Firmware compilation succeeds
    Tool: Bash
    Preconditions: All firmware files exist
    Steps:
      1. Run: `make firmware`
    Expected Result: [SUCCESS] compilation status
    Failure Indicators: Compilation errors
    Evidence: .sisyphus/evidence/task-18-firmware-compile.txt
  ```

  **Commit**: YES
  - Message: `ci: verify binary size, zero CGO, and full test suite`
  - Files: `.sisyphus/evidence/task-18-*` (evidence files only)

---

## Final Verification Wave (MANDATORY — after ALL implementation tasks)

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get explicit "okay" before completing.

- [x] F1. **Plan Compliance Audit** — `oracle`
  Read the plan end-to-end. For each "Must Have": verify implementation exists (read file, run command). For each "Must NOT Have": search codebase for forbidden patterns — reject with file:line if found. Check evidence files exist in `.sisyphus/evidence/`. Compare deliverables against plan.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [x] F2. **Code Quality Review** — `unspecified-high`
  Run `golangci-lint run ./...` + `go test -race -v ./...`. Review all changed files for: `as any`/`@ts-ignore` (N/A for Go), empty catches, `fmt.Println` in prod (should use `log/slog`), commented-out code, unused imports. Check AI slop: excessive comments, over-abstraction, generic names (data/result/item/temp). Verify Bridge has `writeMu sync.Mutex` for WebSocket writes. Verify tool handlers return `NewToolResultError`, not Go `error`.
  Output: `Lint [PASS/FAIL] | Tests [N pass/N fail] | Files [N clean/N issues] | VERDICT`

- [x] F3. **Real QA** — `unspecified-high`
  Start from clean state. Run EVERY QA scenario from EVERY task. Execute: `go test -race -v ./...`, `go build -ldflags="-s -w -X main.version=v0.1.0" -o ask-master .`, `CGO_ENABLED=0 go build -ldflags="-s -w" .`, `stat -f%z ask-master` (verify <12MB), `pio run -d firmware/ask_master`. Save all output to `.sisyphus/evidence/final-qa/`.
  Output: `Build [PASS/FAIL] | Tests [N/N pass] | Lint [PASS/FAIL] | Binary [size/limit] | Firmware [PASS/FAIL] | VERDICT`

- [x] F4. **Scope Fidelity Check** — `deep`
  For each task: read "What to do", read actual diff (git log/diff). Verify 1:1 — everything in spec was built (no missing), nothing beyond spec was built (no creep). Check "Must NOT do" compliance. Detect cross-task contamination. Flag unaccounted changes. Verify no PRD §11 features leaked in.
  Output: `Tasks [N/N compliant] | Contamination [CLEAN/N issues] | Unaccounted [CLEAN/N files] | VERDICT`

---

## Commit Strategy

- **T1-T6** (Wave 1): `feat(server+firmware): scaffold project structure and config` — go.mod, config.go, platformio.ini, config.h, ws_client.h, truncate.go
- **T7** (bridge): `feat(server): implement Bridge with WebSocket send/receive` — bridge.go, bridge_test.go
- **T8-T9** (firmware UI+sketch): `feat(firmware): implement UI rendering and main state machine` — ui.h, ui.cpp, ask_master.ino
- **T10** (tools): `feat(server): implement ask_human, confirm, choose MCP tools` — tools.go, tools_test.go
- **T11** (main.go): `feat(server): wire config, bridge, and MCP server` — main.go
- **T12** (integration): `test(server): add integration test client` — cmd/test-client/main.go
- **T13** (refactor): `refactor(server): clean up bridge and tools` — bridge.go, tools.go
- **T14** (Makefile): `build(server): add Makefile with build, test, lint targets` — Makefile
- **T15-T17** (docs): `docs: add README and setup documentation` — README.md, docs/
- **T18** (verify): `ci: verify binary size and zero CGO` — verification script

---

## Success Criteria

### Verification Commands
```bash
go test -race -v ./...                    # Expected: all tests pass, 0 failures
go build -ldflags="-s -w -X main.version=v0.1.0" -o ask-master .  # Expected: success
stat -f%z ask-master                  # Expected: < 12582912 (12MB)
CGO_ENABLED=0 go build -ldflags="-s -w" . # Expected: success
golangci-lint run ./...                   # Expected: 0 issues
pio run -d firmware/ask_master        # Expected: SUCCESS compilation
```

### Final Checklist
- [x] All "Must Have" present (3 tools, offline degradation, WS reconnect, TDD, all error cases, audio, all UI states, input rules)
- [x] All "Must NOT Have" absent (no §11 features, no Go error returns from tools, no concurrent WS writes, no StaticJsonDocument, no fmt.Println, no String overuse in firmware, no state recovery on reconnect, no MaxLength without verification, no screen flicker, no AI slop)
- [x] All tests pass with `-race` flag
- [x] Binary size < 12MB stripped
- [x] Zero CGO build succeeds