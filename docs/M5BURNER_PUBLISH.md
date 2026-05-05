# M5Burner Publish Description

Copy and paste the following into M5Burner's publish form:

---

## Name
ask-master

## Version
1.1.0

## Description

**Physical Human-in-the-Loop for AI Coding Agents**

Turn your M5Stack Cardputer into a dedicated hardware interface for AI coding agents. Instead of cluttering your chat with questions, your AI agent sends them directly to this compact device — complete with screen, keyboard, and audible alerts.

**What it does:**
- Receives questions from Claude Code, Cursor, OpenCode, Windsurf, and other MCP-compatible agents
- Displays the question on the Cardputer screen with distinct visuals per question type
- Plays audible beeps to grab your attention when you're AFK
- Supports 4 interaction modes: open text, yes/no confirm, multiple choice, and urgent escalation
- Keeps your main screen free of interruptions

**First-time setup:**
After flashing, the device automatically scans for WiFi networks. Select yours by number, enter the password, then type your computer's local IP address. Done — no source code editing required. Press **S** on the idle screen anytime to reconfigure.

**Requirements:**
- M5Stack Cardputer ADV device
- ask-master server running on your computer (install via Homebrew: `brew install mhrsntrk/ask-master/ask-master`)
- AI agent with MCP support

**Links:**
- GitHub: https://github.com/mhrsntrk/ask-master
- Documentation & setup guide: https://github.com/mhrsntrk/ask-master#readme
- Server binary releases: https://github.com/mhrsntrk/ask-master/releases

## Device Type
Cardputer-Adv

## Category
Tool / Utility

## Tags
AI, MCP, assistant, coding, automation, IoT
