<div align="center">

# ask-master

**Physical Human-in-the-Loop for AI coding agents.**
When the model needs you, it pings a tiny screen on your desk — not your chat.

[![Release](https://img.shields.io/github/v/release/mhrsntrk/ask-master?style=flat-square)](https://github.com/mhrsntrk/ask-master/releases)
[![Build](https://img.shields.io/github/actions/workflow/status/mhrsntrk/ask-master/release.yml?style=flat-square)](https://github.com/mhrsntrk/ask-master/actions)
[![Go](https://img.shields.io/badge/go-1.22%2B-00ADD8?style=flat-square&logo=go)](https://go.dev)
[![License](https://img.shields.io/badge/license-MIT-blue?style=flat-square)](LICENSE)
[![skills.sh](https://skills.sh/b/mhrsntrk/ask-master-skill)](https://skills.sh/mhrsntrk/ask-master-skill)

</div>

---

`ask-master` is an [MCP](https://modelcontextprotocol.io) server that gives Claude Code, OpenCode, Cursor, or Windsurf a side-channel to you: a [M5Stack Cardputer](https://shop.m5stack.com/products/m5stack-cardputer-kit-w-m5stamps3) sitting on your desk. The agent asks questions there, you answer with the device's keyboard, and your chat history stays clean.

**Use cases.** Long-running agent loops you walk away from. Pair-programming sessions where you don't want approval prompts cluttering chat. Critical decisions you want a buzzer for. Anything where the model needs human input but the user might be AFK.

## How it works

```
┌──────────────────────────────────────────────────────────────────┐
│  MCP Client  (Claude Code / OpenCode / Cursor / Windsurf)        │
│                                                                  │
│   calls: ask-human, confirm, choose, escalate-to-human           │
│         │                                                        │
│         ▼  stdio (JSON-RPC 2.0)                                  │
├──────────────────────────────────────────────────────────────────┤
│  ask-master  (Go daemon)                                         │
│  ├── main.go      — MCP server init + ServeStdio                 │
│  ├── bridge.go    — WebSocket bridge + /notify HTTP endpoint     │
│  ├── daemon.go    — multi-session Unix socket + lock             │
│  └── tools.go     — ask-human, confirm, choose, escalate-to-human│
│         │                                                        │
│         ▼  WebSocket  ws://0.0.0.0:8765                          │
├──────────────────────────────────────────────────────────────────┤
│  Cardputer ADV firmware  (Arduino / C++)                         │
│  ├── WiFi + WebSocket auto-reconnect (3s)                        │
│  ├── UDP beacon (port 8766) — presence + wake                    │
│  ├── On-device setup menu  (no source edits required)            │
│  └── TFT renderer + keyboard                                     │
└──────────────────────────────────────────────────────────────────┘
```

## Tools

| Tool | When to use | Device shows | Reply method |
|------|-------------|--------------|--------------|
| `ask-human` | Open-ended question, free-text answer | Blue **ASK** screen, 1000 Hz beep | Type, Enter |
| `confirm` | Yes / no decision | Red **CONFIRM** screen, 1300 Hz beep | `Y` / `N` |
| `choose` | 2–6 multiple-choice options | Teal **CHOOSE** screen, 900 Hz beep | Number key |
| `escalate-to-human` | Urgent — louder alert, use sparingly | Orange **ESCALATE** screen, 1500 Hz beep | Type, Enter |

When the device is offline, tools return safe fallbacks (`[CARDPUTER OFFLINE] …`, `false`, or the first option) so the agent never hangs.

## Quick start

### 1. Install the server

**macOS (Homebrew):**
```bash
brew install mhrsntrk/ask-master/ask-master
```

**Other platforms / source:**
```bash
git clone https://github.com/mhrsntrk/ask-master && cd ask-master
make build      # produces ./ask-master
```

### 2. Flash the Cardputer

**M5Burner (recommended — no PC after first flash):**

1. Install [M5Burner](https://docs.m5stack.com/en/uiflow/m5burner/introduction).
2. In the **User Custom** tab, enter share code `KXgpvtfPA52RKfQK` (or search `ask-master`).
3. Select your Cardputer ADV and click **Burn**.
4. On first boot the device scans WiFi, prompts for password and your computer's IP.

Other paths: [M5Launcher (OTA)](#firmware-other-paths) · [PlatformIO build](#firmware-other-paths)

### 3. Wire it into Claude Code

```text
/plugin marketplace add mhrsntrk/ask-master
/plugin install ask-master@ask-master
```

The plugin bundles the MCP server registration, the [skill](skill/SKILL.md), slash commands, a `SessionStart` context hook, and a `Notification` hook that auto-pings the device when Claude Code goes idle. The `ask-master` binary must be on `$PATH` (handled by step 1).

**One required hand-edit** — Claude Code plugins can't pre-grant MCP permissions. Append to `~/.claude/settings.json` so the agent doesn't prompt on every call (which defeats the AFK purpose):

```json
{
  "permissions": {
    "allow": [
      "mcp__ask-master__ask-human",
      "mcp__ask-master__confirm",
      "mcp__ask-master__choose",
      "mcp__ask-master__escalate-to-human"
    ]
  }
}
```

That's it. Ask the agent something AFK and watch the Cardputer light up.

## Other MCP clients

<details>
<summary><b>Claude Code (manual, no plugin)</b></summary>

```bash
claude mcp add ask-master --scope user -- /path/to/ask-master --ws-addr 0.0.0.0:8765
```

Or hand-edit `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "ask-master": {
      "command": "/path/to/ask-master",
      "args": ["--ws-addr", "0.0.0.0:8765"]
    }
  }
}
```
</details>

<details>
<summary><b>OpenCode</b></summary>

`opencode.jsonc`:
```jsonc
{
  "mcp": {
    "ask-master": {
      "type": "local",
      "command": ["/path/to/ask-master", "--ws-addr", "0.0.0.0:8765"],
      "enabled": true,
      "timeout": 310000
    }
  },
  "instructions": "Escalate to physical device via ask-master tools only if chat goes unanswered for 2 minutes."
}
```
</details>

<details>
<summary><b>Cursor</b></summary>

`~/.cursor/mcp.json`:
```json
{
  "mcpServers": {
    "ask-master": { "command": "/path/to/ask-master" }
  }
}
```
`.cursorrules`:
```text
If I'm away from the keyboard (no reply in 2 mins), use the ask-master MCP tools to ping my Cardputer.
```
</details>

<details>
<summary><b>Windsurf</b></summary>

`~/.codeium/windsurf/mcp_config.json`:
```json
{
  "mcpServers": {
    "ask-master": { "command": "/path/to/ask-master" }
  }
}
```
Windsurf rules:
```text
Always try chat first. If no reply within 120 seconds, escalate the question to my physical Cardputer using the ask-master MCP server.
```
</details>

## Plugin features (Claude Code)

The bundled plugin gives you more than just the MCP server:

| Slash command | Action |
|---------------|--------|
| `/ask-master:ask <q>` | Force an `ask-human` call right now |
| `/ask-master:escalate <q>` | Force an `escalate-to-human` call (louder alert) |
| `/ask-master:status` | Print daemon + socket health |
| `/ask-master:notify-test [msg]` | Fire a notification at the loopback `/notify` endpoint |

**Notification hook.** Whenever Claude Code emits a `Notification` event (permission prompt, idle wait), the plugin's `scripts/notify.sh` POSTs to `http://127.0.0.1:8765/notify`. The daemon dispatches an `escalate`-style alert to the device — fire-and-forget, the harness is never blocked. Device offline → 503, hook silently no-ops.

**Loopback-only.** The `/notify` HTTP endpoint rejects non-`127.0.0.1` / `::1` callers with 403, even when the WebSocket listener is bound to `0.0.0.0` for the device's LAN access.

**Optional statusline.** Add to `~/.claude/settings.json`:
```json
{
  "statusLine": {
    "type": "command",
    "command": "${HOME}/.claude/plugins/ask-master/scripts/statusline.sh"
  }
}
```
Prints `[ask-master:on]` / `[ask-master:off]` in the Claude Code status bar.

See [`plugin/README.md`](plugin/README.md) for the full plugin reference.

## CLI flags

| Flag | Default | Description |
|------|---------|-------------|
| `--ws-addr` | `0.0.0.0:8765` | WebSocket bridge listen address. The `/notify` HTTP endpoint shares this listener but is loopback-only. |
| `--timeout` | `300` | Default tool answer timeout (seconds). |
| `--log-level` | `info` | `debug` / `info` / `warn` / `error`. |
| `--version` | — | Print version and exit. |

## Skill (non-Claude-Code agents)

For OpenCode, Cursor, Windsurf, or any other [skills.sh](https://skills.sh)-aware agent, install the escalation playbook standalone:

```bash
npx skills add mhrsntrk/ask-master-skill
```

> **Claude Code users — skip this.** The plugin from step 3 already bundles the skill (as a symlink to the same source file). Installing both loads it into agent context twice.

## Troubleshooting

<details>
<summary><b>Device shows <code>CONNECTING…</code> forever</b></summary>

- The Cardputer can't reach the IP you entered. Press **S** on the idle screen to re-open setup and re-enter the host IP.
- Your firewall may be blocking incoming TCP on `8765`. Allow it for your local subnet.
- The server isn't actually running. Check `pgrep -f ask-master` or `lsof -i :8765`.
</details>

<details>
<summary><b><code>[CARDPUTER OFFLINE]</code> in agent output</b></summary>

The device hasn't sent a UDP beacon in the last 2 minutes. Check WiFi on the device (top-right indicator) and confirm UDP `8766` isn't blocked between the device and your machine.
</details>

<details>
<summary><b><code>/notify</code> returns 503</b></summary>

Same as above — device presence not seen. The Notification hook treats this as a no-op.
</details>

<details>
<summary><b>Port 8765 already in use</b></summary>

Run with `--ws-addr 0.0.0.0:8865` (or any free port) and re-enter that port on the device via the **S** setup menu.
</details>

<details>
<summary><b>Multiple MCP clients want the same server</b></summary>

`ask-master` runs as a daemon. The first invocation claims a Unix socket at `/tmp/ask-master.sock`; subsequent invocations proxy stdio to it. Multiple Claude Code, OpenCode, Cursor sessions can share one device cleanly.
</details>

## Firmware: other paths

<details>
<summary><b>M5Launcher (OTA, no PC after initial setup)</b></summary>

1. Install [M5Launcher](https://github.com/bmorcelli/Launcher) on the Cardputer.
2. Open M5Launcher → **OTA** → search `ask-master`.
3. Install over WiFi, then go through the on-device setup wizard.
</details>

<details>
<summary><b>PlatformIO (build from source)</b></summary>

```bash
cd firmware/ask_master
pio run -t upload
```

To produce a single merged binary for M5Burner v3:
```bash
./merge_bin.sh
```
Output: `ask-master-cardputer.bin`.

Press **S** on the idle screen to re-enter setup at any time.
</details>

## Development

```bash
make build       # produce ./ask-master with version stamped from git describe
make test        # go test ./... -race -v
make lint        # golangci-lint
make firmware    # PlatformIO build
```

Layout:
```
.
├── bridge.go / bridge_test.go   — WebSocket + /notify endpoint
├── daemon.go                    — Unix-socket multi-session daemon
├── tools.go / tools_test.go     — MCP tool registration
├── config.go / config_test.go   — flags
├── main.go                      — entry, daemon/client routing
├── internal/truncate/           — string clipping helpers
├── firmware/ask_master/         — Arduino / PlatformIO source
├── skill/SKILL.md               — escalation playbook (source of truth)
├── plugin/                      — Claude Code plugin (skill is symlinked)
└── .claude-plugin/              — marketplace.json for the repo
```

The `skill/SKILL.md` file is the source of truth. The plugin's copy is a symlink; the standalone skill repo (`mhrsntrk/ask-master-skill`) is synced via GitHub Actions on push to `master`. Set repo secret `SKILL_REPO_PAT` to a PAT with `repo` scope to enable that workflow.

## Security

`ask-master` runs as a daemon on your laptop. The WebSocket bridge binds to all interfaces (`0.0.0.0:8765`) so the Cardputer can reach it from your LAN. To keep that surface honest:

- **IP-binding auth on the WS handshake.** A peer can only complete the WS upgrade if its IP matches the most recent UDP-beacon source (the real device beacons every few seconds before connecting). Random LAN peers fail the handshake with `403`.
- **Frame size cap.** The WS reader is limited to 64 KB per frame — a malicious peer cannot crash the daemon with an oversized message.
- **Queue cap.** At most 32 questions pending; further calls return an error to the MCP client instead of growing memory forever.
- **`/notify` rate limit.** The loopback notification endpoint is throttled to one alert every 2 seconds (`429` on excess). The plugin's `Notification` hook adds its own 10-second debounce on the client side.
- **Loopback-only `/notify`.** The endpoint rejects non-`127.0.0.1` / `::1` callers with `403` even though the listener itself is LAN-bound.
- **Per-user runtime state.** Daemon Unix socket lives under `$XDG_RUNTIME_DIR` (Linux) or `$HOME/Library/Caches/ask-master` (macOS), no longer in world-writable `/tmp`.

**Known limitations.** IP-binding auth is best-effort — an attacker on the same LAN segment who can both (a) take down the real device and (b) spoof its IP can still pair with the daemon. A pre-shared-secret pairing flow with on-device token entry is planned for the next release.

If you're running on an untrusted network and don't have a Cardputer hooked up, pass `--ws-addr 127.0.0.1:8765` to bind to loopback only.

## Roadmap

- [x] Daemon mode for multi-session sharing
- [x] On-device WiFi + server setup (no source edits)
- [x] M5Burner distribution
- [x] Claude Code plugin: skill + slash commands + `Notification` hook
- [x] Loopback `/notify` HTTP endpoint
- [x] IP-binding WS auth + frame size cap + notify rate limit (v1.4.1)
- [ ] Pre-shared-secret pairing flow with on-device token entry
- [ ] Per-question metadata (which agent / project asked)
- [ ] Multi-device fan-out (one server, several Cardputers)
- [ ] Web UI fallback when the device is offline

## Why "ask-master"?

> **Trigger warning:** references to Git branch names and first-name etymology.

In 2020 a lot of people got very upset about the word *master* and spent serious effort renaming default Git branches to *main*. Heroic stuff.

Then I named this tool `ask-master`. My first name is **Mahir**, which translates literally to **"master"** in English. My parents picked it ~30 years ago with no regard for your branch-naming conventions.

If the name bothers you, the **Fork** button is right there — `ask-main`, `ask-primary`, `ask-trunk` are all valid life choices. I won't be mad. I might laugh.

*Mahir out.*

## License

[MIT](LICENSE) — © Mahir Şentürk
