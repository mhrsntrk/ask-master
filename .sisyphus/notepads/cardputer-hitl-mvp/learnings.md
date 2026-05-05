
- Task 9 firmware sketch should treat reconnect as a fresh idle session: clear prompt state on both connect and disconnect callbacks, then redraw via UI helpers only.
- For ASK input, maintain a fixed `char inputBuffer[81]` and re-render the ask screen after typing/backspace instead of writing directly to the display.

## README.md Setup (Task 15)
- Followed PRD §2.1 for the ASCII architecture diagram.
- Included setup guides for both Go server and Cardputer firmware.
- Provided configuration snippets for Claude Code, OpenCode, Cursor, and Windsurf as required.
- Ensured all required sections (Overview, Architecture, Prerequisites, Quick Start, Configuration, Client Setup, Firmware) were included.
Makefile targets verified: build, install, test, lint, firmware, clean, all
Build and tests passing.
## Verification Results - Tue May  5 01:05:44 CEST 2026

### Binary Size
Size: 7,855,042 bytes (~7.5 MB)
Limit: 12,582,912 bytes (12 MB)
Status: PASS

### CGO Status
CGO_ENABLED=0 Build: Success
Status: PASS

### Tests
Race Tests: 37 passed in 2 packages
Status: PASS
## Task 17: Client Setup Documentation

- Created docs/client-setup.md with 4 client configurations (Claude Code, OpenCode, Cursor, Windsurf) exactly as specified in PRD §6.
- Included a troubleshooting section as required.
- Verified presence of all 4 configs.
- Saved evidence to .sisyphus/evidence/task-17-client.txt.
