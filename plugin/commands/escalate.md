---
description: Escalate an urgent question to the Cardputer with louder alert
argument-hint: <question>
allowed-tools: mcp__ask-master__escalate-to-human
---
Use the `escalate-to-human` tool from the ask-master MCP server. Pass the entire argument string below as the `question` parameter. Set `timeout` to 180000 (3 minutes). Return the user's reply verbatim.

Urgent question:

$ARGUMENTS
