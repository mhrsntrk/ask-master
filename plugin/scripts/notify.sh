#!/usr/bin/env bash
# Notification hook: fired when Claude Code emits a notification (idle prompt,
# permission request, etc.). Pings the Cardputer via the ask-master daemon's
# loopback /notify endpoint so the user knows attention is needed.
#
# Stdin: JSON payload from Claude Code. Best-effort message extraction.
# Exit: always 0 (hook must not block the harness).
#
# Debouncing: a per-user timestamp file enforces a minimum interval between
# device pings (default 10 s). This prevents a noisy plugin or rapid
# permission-prompt loop from spamming the Cardputer. Override with the
# ASK_MASTER_NOTIFY_INTERVAL env var (seconds).

set +e

MIN_INTERVAL=${ASK_MASTER_NOTIFY_INTERVAL:-10}
STATE_DIR="${XDG_RUNTIME_DIR:-${TMPDIR:-/tmp}}"
STATE_FILE="${STATE_DIR%/}/ask-master-notify.last"

now=$(date +%s)
if [ -f "$STATE_FILE" ]; then
  last=$(cat "$STATE_FILE" 2>/dev/null || echo 0)
  case "$last" in
    ''|*[!0-9]*) last=0 ;;
  esac
  if [ $((now - last)) -lt "$MIN_INTERVAL" ]; then
    exit 0  # silently debounced
  fi
fi
printf '%s' "$now" > "$STATE_FILE" 2>/dev/null

PAYLOAD=$(cat 2>/dev/null || true)

MESSAGE=$(printf '%s' "$PAYLOAD" \
  | sed -n 's/.*"message"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' \
  | head -n 1)

if [ -z "$MESSAGE" ]; then
  MESSAGE="Claude Code needs your attention"
fi

ESCAPED=${MESSAGE//\\/\\\\}
ESCAPED=${ESCAPED//\"/\\\"}

curl -s -m 2 -o /dev/null -X POST \
  -H 'Content-Type: application/json' \
  -d "$(printf '{"message":"%s","context":"hook:Notification"}' "$ESCAPED")" \
  http://127.0.0.1:8765/notify 2>/dev/null

exit 0
