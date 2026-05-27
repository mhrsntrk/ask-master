# ask-master Claude Code plugin

One-command bundle that wires the [ask-master](https://github.com/mhrsntrk/ask-master) MCP server into Claude Code.

## What's inside

| Component | Effect |
|-----------|--------|
| `.mcp.json` | Registers the `ask-master` MCP server (binary must be on `$PATH`) |
| `skills/ask-master/SKILL.md` | Escalation playbook the agent reads on demand |
| `commands/ask.md` | `/ask-master:ask <question>` — force a direct `ask-human` call |
| `commands/escalate.md` | `/ask-master:escalate <question>` — force an `escalate-to-human` call |
| `commands/status.md` | `/ask-master:status` — print daemon + socket health |
| `commands/notify-test.md` | `/ask-master:notify-test [msg]` — fire a notification at the loopback `/notify` endpoint |
| `hooks/hooks.json` | `SessionStart` injects escalation rules; `Notification` auto-pings device on idle |
| `scripts/notify.sh` | Curls `POST http://127.0.0.1:8765/notify` whenever Claude Code emits a Notification event |
| `scripts/statusline.sh` | Optional statusline indicator (`[ask-master:on/off]`) |

## Prerequisites

The plugin only wires Claude Code. You still need the `ask-master` binary on your `$PATH`:

```bash
brew install mhrsntrk/ask-master/ask-master   # macOS
# or: go install github.com/mhrsntrk/ask-master@latest
```

And a flashed M5Stack Cardputer ADV — see the main repo README.

## Install

```text
/plugin marketplace add mhrsntrk/ask-master
/plugin install ask-master@ask-master
```

## Recommended user config

Claude Code plugins can't pre-grant tool permissions. Add to `~/.claude/settings.json` so the agent doesn't prompt on every call (defeats the AFK purpose):

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

## Optional: statusline

Add to `~/.claude/settings.json`:

```json
{
  "statusLine": {
    "type": "command",
    "command": "${HOME}/.claude/plugins/ask-master/scripts/statusline.sh"
  }
}
```

Adjust the path if the installer puts the plugin somewhere else.

## Slash commands

| Command | Action |
|---------|--------|
| `/ask-master:ask <q>` | Send `q` to the device via `ask-human`, return reply |
| `/ask-master:escalate <q>` | Send `q` via `escalate-to-human` (louder alert) |
| `/ask-master:status` | Report daemon socket + PID state |
| `/ask-master:notify-test [msg]` | Fire a fire-and-forget alert via the daemon's `/notify` HTTP endpoint |

## Auto-notify on idle

The `Notification` hook fires whenever Claude Code emits a notification (permission prompt, idle wait, etc.). It POSTs to `http://127.0.0.1:8765/notify` on the ask-master daemon, which buzzes the Cardputer with an "escalate" payload. The hook is fire-and-forget — Claude Code is never blocked.

Requirements:
- Daemon listening on default port `8765` (any address; the hook always uses `127.0.0.1`).
- Cardputer online (UDP beacon seen in last 2 minutes). Otherwise `/notify` returns 503 and the hook silently no-ops.

The `/notify` endpoint is loopback-only — requests from non-127.0.0.1 / non-::1 hosts are rejected with 403 even though the WS listener may be bound to `0.0.0.0` for device LAN access.

## Uninstall

```text
/plugin uninstall ask-master@ask-master
```
