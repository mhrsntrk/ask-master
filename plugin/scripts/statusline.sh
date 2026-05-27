#!/usr/bin/env bash
# Optional statusline component. Prints a compact ask-master indicator.
# To enable, add to ~/.claude/settings.json:
#   "statusLine": {
#     "type": "command",
#     "command": "/absolute/path/to/plugin/scripts/statusline.sh"
#   }

if [ -S /tmp/ask-master.sock ] && [ -f /tmp/ask-master.lock ] \
   && pgrep -F /tmp/ask-master.lock >/dev/null 2>&1; then
  printf "[ask-master:on]"
else
  printf "[ask-master:off]"
fi
