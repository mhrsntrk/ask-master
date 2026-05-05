---
name: ask-master
description: Physical Human-in-the-Loop MCP server for AI coding agents using M5Stack Cardputer. Provides ask-human, confirm, choose, and escalate-to-human tools that route questions to a dedicated hardware device when the user is away from keyboard or when immediate attention is needed. Use when you need human input and chat responses are slow or when you want guaranteed attention.
license: MIT
compatibility: Requires ask-master MCP server binary and M5Stack Cardputer ADV device
metadata:
  author: mhrsntrk
  version: "1.0.1"
allowed-tools: ask-human confirm choose escalate-to-human
---

# Ask-Master: Physical Human-in-the-Loop

Route your human questions to a dedicated hardware device instead of cluttering the chat. Ask-master connects your AI agent to an M5Stack Cardputer — a small WiFi-enabled device with a screen and keyboard that sits on the user's desk.

## Why Use Ask-Master?

- **Screen real estate**: Don't clutter the user's chat with long questions
- **AFK detection**: When the user steps away, questions don't get lost
- **Urgency escalation**: For critical decisions, buzz the physical device
- **Dedicated interface**: A purpose-built screen for agent-human interaction

## Escalation Pattern (ALWAYS FOLLOW)

When you need human input, follow this escalation sequence:

1. **Ask in chat first** — Always start with the normal conversational flow
2. **Wait up to 2 minutes** — Give the user time to respond
3. **If no response**, use ask-master tools to route to the Cardputer device
4. **For urgent blockers**, skip straight to `escalate-to-human`

### Decision Tree

```
Need human input?
├── User actively chatting?
│   └── Ask normally in chat
├── No response for 2+ minutes?
│   ├── Simple question?
│   │   └── Use ask-human
│   ├── Yes/No decision?
│   │   └── Use confirm
│   ├── Multiple choice?
│   │   └── Use choose
│   └── URGENT / blocker?
│       └── Use escalate-to-human (higher priority alert)
└── User just stepped away but question is critical?
    └── Use escalate-to-human immediately
```

## Tool Reference

### ask-human

Use for open-ended questions when the user hasn't responded in chat.

**Parameters:**
- `question` (required): The question to ask
- `context` (optional): Additional context to display
- `timeout` (optional): Timeout in milliseconds (default: 30000)

**Example:**
```json
{
  "question": "What should I name this variable?",
  "context": "Currently using 'tmp' but it's unclear",
  "timeout": 120000
}
```

**Offline fallback**: Returns `[CARDPUTER OFFLINE] Please answer manually: {question}`

### confirm

Use for yes/no decisions when the user hasn't responded in chat.

**Parameters:**
- `statement` (required): Statement to confirm
- `consequence` (optional): What happens if they confirm
- `timeout` (optional): Timeout in milliseconds

**Example:**
```json
{
  "statement": "Delete the old migration files?",
  "consequence": "This will remove 3 unused migration files"
}
```

**Offline fallback**: Returns `false`

### choose

Use when presenting 2-6 options and the user hasn't responded in chat.

**Parameters:**
- `question` (required): Question to ask
- `options` (required): Array of 2-6 string options
- `context` (optional): Additional context
- `timeout` (optional): Timeout in milliseconds

**Example:**
```json
{
  "question": "Which database should we use?",
  "options": ["PostgreSQL", "MySQL", "SQLite"],
  "context": "For the new user service"
}
```

**Offline fallback**: Returns the first option

### escalate-to-human

Use ONLY when:
1. You already asked in chat and got no response after 2+ minutes, OR
2. The question is urgent and needs immediate attention

This is the "louder" version of ask-human — it triggers a higher-pitched alert on the Cardputer to grab attention.

**Parameters:**
- `question` (required): The question to ask
- `context` (optional): Additional context
- `chat_wait_time_seconds` (optional): How long you waited in chat before escalating
- `timeout` (optional): Timeout in milliseconds

**Example:**
```json
{
  "question": "Production deployment blocked — approve rollback?",
  "context": "Last commit introduced a critical bug",
  "chat_wait_time_seconds": 180
}
```

**Offline fallback**: Same as ask-human

## Best Practices

### Do
- Always try chat first — the Cardputer is an escalation, not a replacement
- Use `escalate-to-human` sparingly — save it for truly urgent or ignored questions
- Provide clear, concise questions — the Cardputer screen is small (240x135px)
- Include `context` when the question needs background information
- Set appropriate timeouts — 30-120 seconds is typical

### Don't
- Spam the device with multiple questions in succession
- Use ask-master for trivial decisions that don't need human input
- Forget to mention in chat that you've escalated to the device
- Send questions longer than ~120 characters (they get truncated)

## Setup

The user must have the ask-master MCP server configured. If tools are unavailable, the user hasn't set it up yet.

**Quick setup check:**
- Is `ask-master` in the MCP server list? If not, direct the user to install it from https://github.com/mhrsntrk/ask-master
- Is the Cardputer powered on and connected to WiFi? The server shows connection status

## Device Behavior

The Cardputer has four distinct alert types:

| Tool | Screen Header | Beep Frequency | Input Method |
|------|---------------|----------------|--------------|
| ask-human | Blue "ASK" | 1000 Hz | Free text typing |
| confirm | Red "CONFIRM" | 1300 Hz | Y/N keys |
| choose | Teal "CHOOSE" | 900 Hz | Number keys 1-6 |
| escalate-to-human | Orange "ESCALATE" | 1500 Hz | Free text typing |

The device auto-reconnects to the server every 3 seconds if disconnected.

## Troubleshooting

### "Cardputer not connected"
The device is offline. Fall back to chat-based questioning.

### No response after timeout
The user may be away from the device. Note this in your reasoning and either:
- Proceed with a safe default (if applicable)
- Try again later
- Document the blocker and continue with other tasks

### Multiple pending questions
Only one question can be active on the device at a time. Queue your questions mentally and ask them sequentially, not in parallel.

## Examples

### Example 1: Simple Escalation
```
You: I've refactored the auth module. Should I proceed with the user module next?
[2 minutes pass, no response]
You: [uses ask-human] "Should I proceed with refactoring the user module next?"
```

### Example 2: Urgent Escalation
```
You: The tests are failing because the DB migration is incompatible. 
     [uses escalate-to-human] "DB migration v3 is incompatible with current schema. 
     Approve dropping and recreating the test database?"
```

### Example 3: Decision by Choice
```
You: For the caching layer, which approach do you prefer?
     [uses choose] "Cache strategy?" options: ["Redis", "Memcached", "In-memory"]
```

## Remember

The Cardputer is a physical device sitting on the user's desk. Treat it with the same respect you'd treat calling someone's phone — use it when needed, but don't abuse it. The escalation pattern exists to balance responsiveness with respect for the user's attention.
