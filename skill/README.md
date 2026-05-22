# ask-master-skill

> A skill that teaches AI coding agents **when and how** to ask you questions on a physical M5Stack Cardputer instead of cluttering the chat.

[![skills.sh](https://skills.sh/b/mhrsntrk/ask-master-skill)](https://skills.sh/mhrsntrk/ask-master-skill)
[![Main repo](https://img.shields.io/badge/source-mhrsntrk%2Fask--master-blue)](https://github.com/mhrsntrk/ask-master)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](./SKILL.md)

This is the **skill** companion to [`ask-master`](https://github.com/mhrsntrk/ask-master) — a Human-in-the-Loop MCP server. The MCP server exposes the tools; this skill teaches the agent the *behavior* around them: try chat first, wait, and only escalate to the physical device when a question goes unanswered or is genuinely urgent.

Install the skill and your agent stops treating the Cardputer as a chat replacement and starts treating it like a phone call — used when needed, never abused.

## What this skill does

`ask-master` connects your AI agent (Claude Code, OpenCode, Cursor, Windsurf) to an M5Stack Cardputer — a small WiFi device with a screen and keyboard that sits on your desk. When the agent needs your input, it can route the question to the device instead of the chat window.

Without this skill, agents don't know *when* hardware is better than chat. The skill encodes the escalation pattern:

1. **Ask in chat first** — normal conversational flow.
2. **Wait up to ~2 minutes** for a response.
3. **No response?** Route the question to the Cardputer via the appropriate tool.
4. **Urgent blocker?** Skip straight to `escalate-to-human` for a louder alert.

## The four tools

| Tool | Use it for | Device alert | Input on device | Offline fallback |
|------|------------|--------------|-----------------|------------------|
| `ask-human` | Open-ended questions | Blue "ASK", 1000 Hz | Free text | `[CARDPUTER OFFLINE] Please answer manually: …` |
| `confirm` | Yes/no decisions | Red "CONFIRM", 1300 Hz | Y/N keys | `false` |
| `choose` | 2–6 options | Teal "CHOOSE", 900 Hz | Number keys 1–6 | First option |
| `escalate-to-human` | Urgent / ignored questions | Orange "ESCALATE", 1500 Hz | Free text | Same as `ask-human` |

Each tool accepts an optional `timeout` (milliseconds; 30–120 s is typical). Only one question can be active on the device at a time — questions are answered sequentially, not in parallel. Keep prompts under ~120 characters; the 240×135px screen truncates long text.

See [`SKILL.md`](./SKILL.md) for the full tool reference, parameters, decision tree, and worked examples.

## Prerequisites

This skill only works once the underlying system is in place:

- The **`ask-master` MCP server** binary, configured in your agent. ([install instructions](https://github.com/mhrsntrk/ask-master#client-setup))
- An **M5Stack Cardputer ADV** device flashed with the ask-master firmware and connected to WiFi.

If the tools aren't available in your agent, the server isn't configured yet — set it up from the [main repo](https://github.com/mhrsntrk/ask-master) first.

## Install the skill

```bash
npx skills add mhrsntrk/ask-master-skill
```

Or browse it on [skills.sh](https://skills.sh/mhrsntrk/ask-master-skill).

## How it fits together

```
You ── desk ──┐
              │  (answers on the Cardputer)
        ┌─────▼──────────┐      WebSocket       ┌──────────────────┐   stdio/MCP   ┌──────────────┐
        │  M5Stack       │ ◄──ws://…:8765────►   │  ask-master      │ ◄───────────► │  AI agent    │
        │  Cardputer ADV │                       │  (Go MCP server) │               │  (Claude…)   │
        └────────────────┘                       └──────────────────┘               └──────────────┘
                                                          ▲
                                                   this skill teaches
                                                   the agent WHEN to use it
```

## Best practices

- Always try chat first — the Cardputer is an escalation, not a replacement.
- Use `escalate-to-human` sparingly; reserve it for truly urgent or ignored questions.
- Tell the user in chat when you've escalated to the device.
- Keep questions short and provide `context` only when it adds real signal.

## Links

- **MCP server & firmware:** https://github.com/mhrsntrk/ask-master
- **Setup guide (server + device):** https://github.com/mhrsntrk/ask-master#quick-start
- **Skill source of truth:** [`SKILL.md`](./SKILL.md)

## License

MIT — see the [main repository](https://github.com/mhrsntrk/ask-master/blob/master/LICENSE).
