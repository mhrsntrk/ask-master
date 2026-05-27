#!/usr/bin/env bash
# Notification hook: fired when Claude Code emits a notification (idle prompt,
# permission request, etc.). Pings the Cardputer via the ask-master daemon's
# loopback /notify endpoint so the user knows attention is needed.
#
# Stdin: JSON payload from Claude Code. Best-effort message extraction.
# Exit: always 0 (hook must not block the harness).

set +e

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
