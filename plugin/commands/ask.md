---
description: Send a question directly to the Cardputer via ask-master
argument-hint: <question>
allowed-tools: mcp__ask-master__ask-human
---
Use the `ask-human` tool from the ask-master MCP server. Pass the entire argument string below as the `question` parameter. Set `timeout` to 120000 (2 minutes). Return the user's reply verbatim with no extra commentary.

Question to ask:

$ARGUMENTS
