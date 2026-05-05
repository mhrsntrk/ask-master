
- Used `drawIdleScreen("Connecting...", "Waiting for WiFi / WS")` as the CONNECTING view so the sketch stays within the existing UI abstraction and avoids direct display rendering.
- Set disconnect state to `CONNECTING` and connect state to `IDLE`; reconnect does not restore prior payload/question state and never re-sends earlier replies.
Binary verification (size: 7.5MB, CGO: disabled, tests: passing) completed successfully on Tue May  5 01:05:44 CEST 2026.
