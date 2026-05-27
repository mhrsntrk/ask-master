---
description: Show ask-master daemon and Cardputer connection status
allowed-tools: Bash(test:*), Bash(ls:*), Bash(stat:*), Bash(pgrep:*)
---
Report ask-master health using these checks. Run each one and summarize in a 3-line report.

1. Daemon socket present:
   `test -S /tmp/ask-master.sock && echo "socket: ok" || echo "socket: missing"`

2. Daemon PID alive:
   `[ -f /tmp/ask-master.lock ] && pgrep -F /tmp/ask-master.lock >/dev/null 2>&1 && echo "daemon: running ($(cat /tmp/ask-master.lock))" || echo "daemon: not running"`

3. Socket last modified (proxy for last activity):
   `[ -S /tmp/ask-master.sock ] && stat -f "socket mtime: %Sm" /tmp/ask-master.sock 2>/dev/null || stat -c "socket mtime: %y" /tmp/ask-master.sock 2>/dev/null || echo "socket mtime: n/a"`

If daemon not running, tell user: "Start it with: `ask-master` (or wait for Claude Code to spawn it on next MCP call)."
