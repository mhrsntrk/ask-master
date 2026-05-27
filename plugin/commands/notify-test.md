---
description: Manually trigger a Cardputer notification to verify the /notify endpoint
argument-hint: [message]
allowed-tools: Bash(curl:*)
---
Send a test notification to the Cardputer through the ask-master daemon's loopback notify endpoint.

If `$ARGUMENTS` is non-empty, use it as the message; otherwise use a default.

Run:

```
curl -s -m 3 -o - -w "\nHTTP %{http_code}\n" -X POST \
  -H 'Content-Type: application/json' \
  -d "$(printf '{"message":"%s","context":"slash:/notify-test"}' "${ARG:-/notify-test ping}")" \
  http://127.0.0.1:8765/notify
```

Where `ARG` = `$ARGUMENTS`.

Report the HTTP status code and any response body. Tell the user: 202 = queued, 503 = device offline, 403 = bind issue, anything else = daemon down.
