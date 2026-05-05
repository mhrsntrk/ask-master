## Patterns & Conventions
- Go 1.22+ used for module initialization.
- Dependencies: github.com/mark3labs/mcp-go (v0.51.0), github.com/gorilla/websocket (v1.5.3).
- Verification requires a temporary main.go since 'go build .' fails without source files.
- mcp-go does not have a top-level package; used github.com/mark3labs/mcp-go/server for build verification.
