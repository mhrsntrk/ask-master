# ask-master

Physical Human-in-the-Loop MCP server for AI coding agents using M5Stack Cardputer.

[![skills.sh](https://skills.sh/b/mhrsntrk/ask-master-skill)](https://skills.sh/mhrsntrk/ask-master-skill)

## Why "ask-master"?

> **Trigger Warning:** This section contains references to Git branch names and first-name etymology. Reader discretion is advised.

In 2020, a lot of people got very upset about the word "master" and spent countless hours renaming their default Git branches to "main" — as if the branch name was the single greatest injustice in software development. It was a truly heroic effort. The word "master" was successfully defeated. Peace had returned to the repositories.

Then I came along and named this tool **ask-master**.

Why? Because my first name is **Mahir**, and — plot twist — it literally translates to **"master"** in English. That's right. I didn't choose the name; my parents did, approximately thirty years ago, with absolutely no regard for your Git branch naming conventions.

So if the name offends you, I fully support your right to be offended. I also support your right to click the **Fork** button and rename it to `ask-main`, `ask-primary`, `ask-trunk`, or whatever feels safest. I won't be mad. I might laugh, but I won't be mad.

*Mahir out.*

## Overview

`ask-master` is a standalone MCP (Model Context Protocol) server that enables AI coding agents like Claude Code, OpenCode, Cursor, or Windsurf to pause and ask you questions via a physical M5Stack Cardputer device. This keeps your main screen clear of interruptions while providing a dedicated hardware interface for agent-human interaction.

## Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│  MCP Client (Claude Code / OpenCode / Cursor / Windsurf)         │
│                                                                  │
│   agent calls: ask-human() / confirm() / choose() / escalate()   │
│         │                                                        │
│         ▼  stdio (JSON-RPC 2.0 over stdin/stdout)                │
├──────────────────────────────────────────────────────────────────┤
│  ask-master  (Go binary)                                         │
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

## Prerequisites

- Go 1.22+ (for the server)
- PlatformIO CLI or Arduino IDE (for the firmware)
- M5Stack Cardputer ADV device

## Quick Start

### macOS (Homebrew)

```bash
brew install mhrsntrk/ask-master/ask-master
```

### Linux / Windows / Build from Source

1. Build the Go server:
   ```bash
   make build
   ```

2. The binary `ask-master` will be created in the root directory.

### Firmware Setup

The firmware supports **on-device configuration** — no need to edit source code. Just flash it and configure WiFi + server IP directly on the Cardputer.

#### Option A: M5Burner (Easiest — No Build Required)

1. Download and install [M5Burner](https://docs.m5stack.com/en/uiflow/m5burner/introduction)
2. Search for `ask-master` in the **User Custom** tab
3. Select your device (Cardputer ADV) and click **Burn**
4. After flashing, the device **automatically scans for WiFi networks** and shows a list:
   - Select your network by pressing **1-6**
   - Press **R** to rescan if your network isn't visible
   - Enter your **WiFi Password**
   - Enter your computer's **IP address**
   - Confirm the settings

#### Option B: M5Launcher (OTA — No PC Required After Initial Setup)

1. Install [M5Launcher](https://github.com/bmorcelli/Launcher) on your Cardputer
2. Open M5Launcher → **OTA** → Search `ask-master`
3. Install over Wi-Fi
4. Configure on-device as described above

#### Option C: Build from Source (PlatformIO)

```bash
cd firmware/ask_master
pio run -t upload
```

After flashing, the device enters setup mode automatically:
1. **Scans for WiFi networks** — select yours by number
2. Enter your **WiFi Password**
3. Enter your computer's **IP address**
4. Confirm the settings

To reconfigure later, press **S** on the idle screen.

#### Building a Merged Binary for Distribution

To create a single `.bin` for M5Burner v3:

```bash
cd firmware/ask_master
./merge_bin.sh
```

This produces `ask-master-cardputer.bin` which you can upload to M5Burner.

## Configuration

The server accepts the following CLI flags:

| Flag | Default | Description |
|------|---------|-------------|
| `--ws-addr` | `0.0.0.0:8765` | WebSocket bridge listen address |
| `--timeout` | `300` | Default tool answer timeout in seconds |
| `--log-level` | `info` | `debug` / `info` / `warn` / `error` |
| `--version` | — | Print version and exit |

## Client Setup

Add `ask-master` to your preferred AI agent configuration. Replace `/path/to/bin/` with the actual path to your compiled binary.

### Claude Code
Add to `~/.claude/settings.json`:
```json
{
  "mcpServers": {
    "ask-master": {
      "command": "/path/to/bin/ask-master",
      "args": ["--ws-addr", "0.0.0.0:8765"]
    }
  }
}
```

### OpenCode
Add to `opencode.jsonc`:
```jsonc
{
  "mcp": {
    "ask-master": {
      "type": "local",
      "command": ["/path/to/bin/ask-master", "--ws-addr", "0.0.0.0:8765"],
      "enabled": true,
      "timeout": 310000
    }
  }
}
```

### Cursor
Add to `~/.cursor/mcp.json`:
```json
{
  "mcpServers": {
    "ask-master": {
      "command": "/path/to/bin/ask-master"
    }
  }
}
```

### Windsurf
Add to `~/.codeium/windsurf/mcp_config.json`:
```json
{
  "mcpServers": {
    "ask-master": {
      "command": "/path/to/bin/ask-master"
    }
  }
}
```

## Agent Instructions

By default, AI agents don't know when to use physical hardware vs chat. To get the best experience, configure your agent with instructions to follow this escalation pattern:

1. Ask in chat first.
2. Wait up to 2 minutes for a response.
3. If no response, use `ask-master` tools (`ask-human`, `confirm`, `choose`, `escalate-to-human`).

The `escalate-to-human` tool is especially useful as an "attention grabber" for urgent cases, as it triggers a more prominent alert on the Cardputer.

### Claude Code
Add these instructions to `CLAUDE.md` in your project root or `~/.claude/CLAUDE.md`:
```markdown
When you need clarification or a decision:
1. Ask in the chat first.
2. Wait 2 minutes.
3. If I don't respond, use the ask-master MCP tools to alert me on my physical device.
4. For urgent blockers, use escalate-to-human immediately.
```

### OpenCode
Add to the `instructions` field in your `opencode.jsonc`:
```json
{
  "instructions": "Escalate to physical device via ask-master tools only if chat goes unanswered for 2 minutes."
}
```

### Cursor
Add to your `.cursorrules` file:
```text
If I'm away from the keyboard (no reply in 2 mins), use the ask-master MCP tools to ping my Cardputer.
```

### Windsurf
Add to your Windsurf rules or instructions settings:
```text
Always try chat first. If no reply within 120 seconds, escalate the question to my physical Cardputer using the ask-master MCP server.
```

## Skill

A dedicated skill is available at [mhrsntrk/ask-master-skill](https://github.com/mhrsntrk/ask-master-skill) for AI agents. Install it with:

```bash
npx skills add mhrsntrk/ask-master-skill
```

## Development

### Syncing the Skill

The `skill/SKILL.md` file is automatically synced to the [ask-master-skill](https://github.com/mhrsntrk/ask-master-skill) repository via GitHub Actions when changes are pushed to `master`.

**Setup required:**
1. Create a Personal Access Token (PAT) at https://github.com/settings/tokens with `repo` scope
2. Add it as a repository secret named `SKILL_REPO_PAT` at https://github.com/mhrsntrk/ask-master/settings/secrets/actions

