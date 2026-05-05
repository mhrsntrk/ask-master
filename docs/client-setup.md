# Client Setup Guide

This guide provides configuration snippets for adding the `ask-master` MCP server to various AI coding agents.

## Claude Code

Add the following to your `~/.claude/settings.json`:

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

## OpenCode

Add the following to your `opencode.jsonc`:

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

## Cursor

Add the following to your `~/.cursor/mcp.json`:

```json
{
  "mcpServers": {
    "ask-master": {
      "command": "/path/to/bin/ask-master"
    }
  }
}
```

## Windsurf

Add the following to your `~/.codeium/windsurf/mcp_config.json`:

```json
{
  "mcpServers": {
    "ask-master": {
      "command": "/path/to/bin/ask-master"
    }
  }
}
```

## Troubleshooting

- **Server Path**: Ensure the `command` path points to the actual location of your compiled `ask-master` binary.
- **WebSocket Address**: If you change the `--ws-addr` in the server arguments, make sure the Cardputer's `config.h` is updated to match the new host and port.
- **Connection Issues**: Verify that your computer and the Cardputer are on the same local network and that no firewalls are blocking the WebSocket port (default 8765).
- **Timeouts**: The default timeout is 300 seconds. If your agent times out too quickly, check if the client configuration allows for longer timeouts (like the `timeout` key in OpenCode).
- **Logs**: Run the server with `--log-level debug` to see detailed connection and tool call information in the server's stderr.
