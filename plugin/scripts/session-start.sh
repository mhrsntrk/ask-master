#!/usr/bin/env bash
# SessionStart hook: inject ask-master availability + escalation rules
# as additional context. stdout is passed to Claude as system context.

set -e

DAEMON_STATUS="unknown"
if [ -S /tmp/ask-master.sock ]; then
  DAEMON_STATUS="running"
else
  DAEMON_STATUS="not running (will start on first MCP tool call)"
fi

cat <<EOF
ask-master plugin loaded. Daemon: ${DAEMON_STATUS}.

Available MCP tools (route human questions to the Cardputer device):
- ask-human: open-ended question, free-text reply
- confirm: yes/no statement
- choose: multiple-choice (2-6 options)
- escalate-to-human: urgent, louder alert

Escalation rules:
1. Ask in chat first. Wait at least 2 minutes for a reply.
2. If no reply, use ask-human / confirm / choose to route to the device.
3. For urgent blockers, use escalate-to-human immediately.
4. Mention in chat when you escalate so the user knows where to look.
EOF
