#include "ui.h"
#include "ui_theme.h"
#include <M5Cardputer.h>
#include "config.h"

// ============================================================================
// ask-master UI — Bruce-style terminal, size-2 body for readability
//
// Layout (240 x 135):
//   y=0..13    status bar     (15 px, textSize 1, dim chrome)
//   y=14       hairline
//   y=16..96   body           (size-2 text, ~5 rows of 16 px)
//   y=97       hairline (when input row present)
//   y=99..114  input row      (size 2, ~16 px)
//   y=115      hairline
//   y=116..134 footer         (19 px, size 2 hint text, accent fill)
// ============================================================================

static M5Canvas canvas(&M5Cardputer.Display);
static bool canvasInitialized = false;

// Caret blink state
static unsigned long lastCaretToggle = 0;
static bool caretVisible = true;

// Escalate flash state
static unsigned long lastFlashTime = 0;
static bool escalateBright = true;

// ---- Init ------------------------------------------------------------------
void initCanvasIfNeeded() {
    if (!canvasInitialized) {
        int dw = M5Cardputer.Display.width();
        int dh = M5Cardputer.Display.height();
        canvas.createSprite(dw, dh);
        canvas.setTextSize(1);
        canvasInitialized = true;
    }
}

// ---- Color helpers ---------------------------------------------------------
static inline uint16_t accentBright(const UIAccent& a) {
    return canvas.color565(a.r, a.g, a.b);
}
static inline uint16_t cDim()   { return canvas.color565(0x80, 0x80, 0x80); }
static inline uint16_t cMid()   { return canvas.color565(0xB0, 0xB0, 0xB0); }
static inline uint16_t cMuted() { return canvas.color565(0x40, 0x40, 0x40); }

// Layout constants (Bruce-style, sized for textSize 2 body)
#define UI_STATUS_H        14
#define UI_HAIR1_Y         14
#define UI_BODY_Y          17
#define UI_BODY_BOTTOM_Y   96    // when input row shown
#define UI_BODY_BOTTOM_NOI 113   // no input row
#define UI_HAIR2_Y         97
#define UI_INPUT_Y         100
#define UI_HAIR3_Y         115
#define UI_FOOTER_Y        117
#define UI_FOOTER_H        18
#define UI_PAD             14
#define UI_LINE_GAP        3   // extra px between wrapped body lines

// ---- Status bar (top, size 1) ----------------------------------------------
static void drawStatusBar(const char* label, const UIAccent& acc, bool online) {
    canvas.fillRect(0, 0, 240, UI_STATUS_H, UI_RGB_BG);
    canvas.setTextSize(1);

    canvas.setTextColor(accentBright(acc));
    canvas.setTextDatum(top_left);
    canvas.drawString(label ? label : "", UI_PAD, 3);

    canvas.setTextDatum(top_right);
    int dotX = 240 - UI_PAD - 4;
    int textRightX = 240 - UI_PAD - 12;
    if (online) {
        canvas.fillCircle(dotX, 6, 2, canvas.color565(0x00, 0xCC, 0x66));
        canvas.setTextColor(cDim());
        canvas.drawString("ON", textRightX, 3);
    } else {
        canvas.drawCircle(dotX, 6, 2, canvas.color565(0xCC, 0x40, 0x40));
        canvas.setTextColor(cDim());
        canvas.drawString("OFF", textRightX, 3);
    }

    canvas.drawLine(0, UI_HAIR1_Y, 240, UI_HAIR1_Y, cMuted());
}

// ---- Footer (size 2, accent fill) ------------------------------------------
static void drawFooter3(const char* left, const char* center, const char* right,
                        const UIAccent& acc) {
    canvas.drawLine(0, UI_HAIR3_Y, 240, UI_HAIR3_Y, cMuted());
    canvas.fillRect(0, UI_FOOTER_Y, 240, UI_FOOTER_H, accentBright(acc));
    canvas.setTextSize(1);
    canvas.setTextColor(UI_RGB_BG);
    int midY = UI_FOOTER_Y + UI_FOOTER_H / 2;

    if (left && left[0]) {
        canvas.setTextDatum(middle_left);
        canvas.drawString(left, UI_PAD + 2, midY);
    }
    if (center && center[0]) {
        canvas.setTextDatum(middle_center);
        canvas.drawString(center, 120, midY);
    }
    if (right && right[0]) {
        canvas.setTextDatum(middle_right);
        canvas.drawString(right, 240 - UI_PAD - 2, midY);
    }
}

// Big-text action footer for screens where the keys ARE the choice (CONFIRM).
// Taller band, size-2 text, accent fill, black text.
#define UI_BIG_FOOTER_H 25
#define UI_BIG_FOOTER_Y (135 - UI_BIG_FOOTER_H)
static void drawBigFooter3(const char* left, const char* center, const char* right,
                            const UIAccent& acc) {
    canvas.drawLine(0, UI_BIG_FOOTER_Y - 1, 240, UI_BIG_FOOTER_Y - 1, cMuted());
    canvas.fillRect(0, UI_BIG_FOOTER_Y, 240, UI_BIG_FOOTER_H, accentBright(acc));
    canvas.setTextSize(2);
    canvas.setTextColor(UI_RGB_BG);
    int midY = UI_BIG_FOOTER_Y + UI_BIG_FOOTER_H / 2;

    if (left && left[0]) {
        canvas.setTextDatum(middle_left);
        canvas.drawString(left, UI_PAD + 2, midY);
    }
    if (center && center[0]) {
        canvas.setTextDatum(middle_center);
        canvas.drawString(center, 120, midY);
    }
    if (right && right[0]) {
        canvas.setTextDatum(middle_right);
        canvas.drawString(right, 240 - UI_PAD - 2, midY);
    }
}

static void drawFooterDim(const char* text) {
    canvas.drawLine(0, UI_HAIR3_Y, 240, UI_HAIR3_Y, cMuted());
    canvas.setTextSize(1);
    canvas.setTextColor(cMid());
    canvas.setTextDatum(middle_center);
    canvas.drawString(text ? text : "", 120, UI_FOOTER_Y + UI_FOOTER_H / 2);
}

// ---- Caret (size 2 height = 16 px) -----------------------------------------
static void drawCaret(int x, int y) {
    unsigned long now = millis();
    if (now - lastCaretToggle > 500) {
        lastCaretToggle = now;
        caretVisible = !caretVisible;
    }
    if (caretVisible) {
        canvas.fillRect(x, y, 2, 14, UI_RGB_FG);
    }
}

// ---- Word-wrap (uses current textSize via fontHeight) ----------------------
int drawWordWrappedColored(const char* text, int x, int y, int maxWidth,
                            uint16_t color, int scrollY, int clipTop, int clipBottom) {
    if (!text) return y;
    canvas.setTextColor(color);
    canvas.setTextDatum(top_left);
    int cursorX = x;
    int cursorY = y - scrollY;
    int lineHeight = canvas.fontHeight() + UI_LINE_GAP;

    char buffer[512];
    strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* word = strtok(buffer, " \n");
    while (word != nullptr) {
        int wWidth = canvas.textWidth(word);
        int spaceWidth = canvas.textWidth(" ");

        if (cursorX + wWidth > x + maxWidth && cursorX > x) {
            cursorX = x;
            cursorY += lineHeight;
        }

        if (cursorY + lineHeight > clipTop && cursorY < clipBottom) {
            canvas.drawString(word, cursorX, cursorY);
        }
        cursorX += wWidth + spaceWidth;
        word = strtok(nullptr, " \n");
    }
    return cursorY + lineHeight + scrollY;
}

int measureWordWrappedHeight(const char* text, int x, int maxWidth) {
    if (!text || text[0] == '\0') return 0;
    int cursorX = x;
    int cursorY = 0;
    int lineHeight = canvas.fontHeight() + UI_LINE_GAP;

    char buffer[512];
    strncpy(buffer, text, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* word = strtok(buffer, " \n");
    while (word != nullptr) {
        int wWidth = canvas.textWidth(word);
        int spaceWidth = canvas.textWidth(" ");

        if (cursorX + wWidth > x + maxWidth && cursorX > x) {
            cursorX = x;
            cursorY += lineHeight;
        }

        cursorX += wWidth + spaceWidth;
        word = strtok(nullptr, " \n");
    }
    return cursorY + lineHeight;
}

// Hardware clip helpers — prevent text bleed into status bar / footer.
static inline void clipToBody(int top, int bottom) {
    canvas.setClipRect(0, top, 240, bottom - top + 1);
}
static inline void clearClip() {
    canvas.clearClipRect();
}

// Body viewport for each screen type — used by computeMaxScroll
static inline void bodyViewportForType(const char* type, int& bodyTop, int& bodyBottom) {
    bodyTop = UI_BODY_Y;
    if (!type) { bodyBottom = UI_BODY_BOTTOM_NOI; return; }
    if (!strcmp(type, "ask") || !strcmp(type, "escalate")) {
        bodyBottom = UI_BODY_BOTTOM_Y;  // input row below
    } else if (!strcmp(type, "confirm")) {
        bodyBottom = UI_BIG_FOOTER_Y - 2;  // big action footer below
    } else {
        bodyBottom = UI_BODY_BOTTOM_NOI;  // choose, settings, etc
    }
}

int computeMaxScroll(const char* type, const char* question, const char* context,
                      const String options[], int optionCount) {
    initCanvasIfNeeded();
    canvas.setTextSize(2);

    int bodyTop, bodyBottom;
    bodyViewportForType(type, bodyTop, bodyBottom);
    int bodyMaxW = 240 - 2 * UI_PAD - 10;  // gutter for scroll triangles
    int viewportH = bodyBottom - bodyTop;

    int contentH = 0;
    if (question && question[0]) {
        contentH += measureWordWrappedHeight(question, 0, bodyMaxW);
    }
    if (context && context[0]) {
        contentH += 6;
        contentH += measureWordWrappedHeight(context, 0, bodyMaxW);
    }
    if (type && !strcmp(type, "choose")) {
        contentH += 6;  // gap before options
        int rowH = canvas.fontHeight() + 4;
        contentH += rowH * optionCount;
    }

    int maxScroll = contentH - viewportH;
    return maxScroll > 0 ? maxScroll : 0;
}

// Scroll triangles in body's right gutter
static void drawScrollHints(int scrollY, int contentHeight, int viewportHeight,
                             int viewTop, int viewBottom) {
    if (contentHeight <= viewportHeight) return;
    uint16_t mark = cMid();
    if (scrollY > 0) {
        canvas.fillTriangle(228, viewTop + 5, 236, viewTop + 5,
                            232, viewTop, mark);
    }
    if (scrollY < contentHeight - viewportHeight) {
        canvas.fillTriangle(228, viewBottom - 5, 236, viewBottom - 5,
                            232, viewBottom, mark);
    }
}

// ============================================================================
// IDLE
// ============================================================================
void drawIdleScreen(const char* version, const char* ip, bool showSetupHint) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    bool connecting = version && strstr(version, "Connecting") != nullptr;
    drawStatusBar(connecting ? "CONNECTING" : "IDLE", ACC_IDLE, !connecting);

    // Small label (size 1, dim) — idle is not important
    canvas.setTextSize(1);
    canvas.setTextColor(cDim());
    canvas.setTextDatum(middle_center);
    canvas.drawString("ask-master", 120, UI_BODY_Y + 12);

    // Status (size 2, prominent — primary content)
    canvas.setTextSize(2);
    canvas.setTextColor(UI_RGB_FG);
    canvas.drawString(connecting ? "connecting" : "ready", 120, UI_BODY_Y + 38);

    // IP (size 1, dim)
    if (ip && ip[0]) {
        canvas.setTextSize(1);
        canvas.setTextColor(cMid());
        canvas.drawString(ip, 120, UI_BODY_Y + 64);
    }

    // Version (size 1, dim)
    if (version && !connecting) {
        canvas.setTextSize(1);
        canvas.setTextColor(cDim());
        canvas.drawString(version, 120, UI_BODY_Y + 82);
    }

    drawFooterDim(showSetupHint ? "[S] Settings" : "");

    canvas.pushSprite(0, 0);
}

// ============================================================================
// SLEEP
// ============================================================================
void drawSleepScreen() {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("SLEEP", ACC_IDLE, true);

    canvas.setTextSize(3);
    canvas.setTextColor(cMid());
    canvas.setTextDatum(middle_center);
    canvas.drawString("zZz", 120, UI_BODY_Y + 20);

    canvas.setTextSize(1);
    canvas.setTextColor(cDim());
    canvas.drawString("any key to wake", 120, UI_BODY_Y + 58);

    drawFooterDim("[S] Settings   [W] Wake");

    canvas.pushSprite(0, 0);
}

// ============================================================================
// ASK / ESCALATE — shared free-text input layout
// ============================================================================
static void drawQuestionScreen(const char* stateLabel, const UIAccent& acc,
                                const char* question, const char* context,
                                const char* inputBuffer, int scrollY,
                                bool escalateFlash) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    UIAccent renderAcc = acc;
    if (escalateFlash) {
        unsigned long now = millis();
        if (now - lastFlashTime > 200) {
            lastFlashTime = now;
            escalateBright = !escalateBright;
        }
        if (!escalateBright) {
            renderAcc.r = (acc.r * 2) / 5;
            renderAcc.g = (acc.g * 2) / 5;
            renderAcc.b = (acc.b * 2) / 5;
        }
    }

    drawStatusBar(stateLabel, renderAcc, true);

    int bodyLeft = UI_PAD;
    int bodyRight = 240 - UI_PAD - 10; // leave gutter for scroll triangles
    int bodyMaxW = bodyRight - bodyLeft;
    int bodyTop = UI_BODY_Y;
    int bodyBottom = UI_BODY_BOTTOM_Y;

    // SIZE 2 body text — hardware-clip to prevent bleed into status/input
    canvas.setTextSize(2);
    clipToBody(bodyTop, bodyBottom);
    int y = bodyTop;
    if (question) {
        y = drawWordWrappedColored(question, bodyLeft, y, bodyMaxW,
                                    UI_RGB_FG, scrollY, bodyTop, bodyBottom);
    }
    if (context && context[0] != '\0') {
        y += 4;
        y = drawWordWrappedColored(context, bodyLeft, y, bodyMaxW,
                                    cMid(), scrollY, bodyTop, bodyBottom);
    }
    clearClip();

    int contentH = y - bodyTop;
    int viewportH = bodyBottom - bodyTop;
    drawScrollHints(scrollY, contentH, viewportH, bodyTop, bodyBottom);

    // Input row
    canvas.drawLine(0, UI_HAIR2_Y, 240, UI_HAIR2_Y, cMuted());
    canvas.setTextSize(2);
    canvas.setTextColor(accentBright(renderAcc));
    canvas.setTextDatum(top_left);
    canvas.setCursor(UI_PAD, UI_INPUT_Y);
    canvas.print("> ");

    canvas.setTextColor(UI_RGB_FG);
    int inputX = UI_PAD + canvas.textWidth("> ");
    if (inputBuffer) canvas.drawString(inputBuffer, inputX, UI_INPUT_Y);

    int caretX = inputX + (inputBuffer ? canvas.textWidth(inputBuffer) : 0) + 1;
    drawCaret(caretX, UI_INPUT_Y);

    drawFooter3("ENTER", ";/. scroll", "ESC", renderAcc);

    canvas.pushSprite(0, 0);
}

void drawAskScreen(const char* question, const char* context, const char* inputBuffer, int scrollY) {
    drawQuestionScreen("ASK", ACC_ASK, question, context, inputBuffer, scrollY, false);
}

void drawEscalateScreen(const char* question, const char* context, const char* inputBuffer, int scrollY) {
    drawQuestionScreen("ESCALATE", ACC_ESCALATE, question, context, inputBuffer, scrollY, true);
}

// ============================================================================
// CONFIRM
// ============================================================================
void drawConfirmScreen(const char* statement, const char* consequence, int scrollY) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("CONFIRM", ACC_CONFIRM, true);

    int bodyLeft = UI_PAD;
    int bodyRight = 240 - UI_PAD - 10;
    int bodyMaxW = bodyRight - bodyLeft;
    int bodyTop = UI_BODY_Y;
    int bodyBottom = UI_BIG_FOOTER_Y - 2; // make room for big action footer

    canvas.setTextSize(2);
    clipToBody(bodyTop, bodyBottom);
    int y = bodyTop;
    if (statement) {
        y = drawWordWrappedColored(statement, bodyLeft, y, bodyMaxW,
                                    UI_RGB_FG, scrollY, bodyTop, bodyBottom);
    }
    if (consequence && consequence[0] != '\0') {
        y += 6;
        y = drawWordWrappedColored(consequence, bodyLeft, y, bodyMaxW,
                                    cMid(), scrollY, bodyTop, bodyBottom);
    }
    clearClip();

    int contentH = y - bodyTop;
    int viewportH = bodyBottom - bodyTop;
    drawScrollHints(scrollY, contentH, viewportH, bodyTop, bodyBottom);

    // Big action footer — Y/N are CONTENT user must read + act on
    drawBigFooter3("[Y] YES", "", "[N] NO", ACC_CONFIRM);

    canvas.pushSprite(0, 0);
}

// ============================================================================
// CHOOSE — full-width numbered list, size 2 row text
// ============================================================================
void drawChooseScreen(const char* question, const char* context, const String options[], int optionCount, int scrollY) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("CHOOSE", ACC_CHOOSE, true);

    int bodyLeft = UI_PAD;
    int bodyRight = 240 - UI_PAD - 10;
    int bodyMaxW = bodyRight - bodyLeft;
    int bodyTop = UI_BODY_Y;
    int bodyBottom = UI_BODY_BOTTOM_NOI;

    // Question header at size 2 — clip to body
    canvas.setTextSize(2);
    clipToBody(bodyTop, bodyBottom);
    int y = bodyTop - scrollY;
    int qLineY = drawWordWrappedColored(question ? question : "", bodyLeft, bodyTop, bodyMaxW,
                                        UI_RGB_FG, scrollY, bodyTop, bodyBottom);
    int optionsTop = qLineY + 6;

    // Options: size 2, indexed
    canvas.setTextSize(2);
    int rowH = canvas.fontHeight() + 4;
    for (int i = 0; i < optionCount && i < 6; i++) {
        int yPos = optionsTop + i * rowH - scrollY;
        if (yPos + rowH < bodyTop) continue;
        if (yPos > bodyBottom) break;

        // Index in accent
        canvas.setTextColor(accentBright(ACC_CHOOSE));
        canvas.setTextDatum(top_left);
        canvas.setCursor(bodyLeft + 2, yPos);
        canvas.printf("%d", i + 1);

        // Vertical separator
        canvas.drawLine(bodyLeft + 18, yPos - 1, bodyLeft + 18, yPos + rowH - 3, cMuted());

        // Name (truncate visually if too long)
        canvas.setTextColor(UI_RGB_FG);
        const char* name = options[i].c_str();
        char buf[40];
        strncpy(buf, name, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        int avail = bodyRight - (bodyLeft + 22);
        while (canvas.textWidth(buf) > avail && strlen(buf) > 4) {
            int n = strlen(buf);
            buf[n - 1] = '\0';
            if (n >= 4) {
                buf[n - 2] = '.';
                buf[n - 3] = '.';
                buf[n - 4] = '.';
            }
        }
        canvas.drawString(buf, bodyLeft + 22, yPos);
    }
    clearClip();

    int totalH = optionCount * rowH + (optionsTop - bodyTop);
    int viewportH = bodyBottom - bodyTop;
    drawScrollHints(scrollY, totalH, viewportH, bodyTop, bodyBottom);

    drawFooter3("1-6 pick", ";/. scroll", "ESC", ACC_CHOOSE);

    canvas.pushSprite(0, 0);
}

// ============================================================================
// SETUP
// ============================================================================
void drawSetupScreen(const char* label, const char* context, const char* inputBuffer) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("SETUP", ACC_SETUP, false);

    int bodyLeft = UI_PAD;
    int bodyRight = 240 - UI_PAD - 10;
    int bodyMaxW = bodyRight - bodyLeft;
    int bodyTop = UI_BODY_Y;

    canvas.setTextSize(2);
    int y = bodyTop;
    if (label) {
        canvas.setTextColor(accentBright(ACC_SETUP));
        canvas.setTextDatum(top_left);
        y = drawWordWrappedColored(label, bodyLeft, y, bodyMaxW,
                                    accentBright(ACC_SETUP), 0, bodyTop, UI_BODY_BOTTOM_Y);
    }
    if (context && context[0] != '\0') {
        y += 4;
        canvas.setTextSize(1);
        y = drawWordWrappedColored(context, bodyLeft, y, bodyMaxW,
                                    cMid(), 0, bodyTop, UI_BODY_BOTTOM_Y);
    }

    // Input row
    canvas.drawLine(0, UI_HAIR2_Y, 240, UI_HAIR2_Y, cMuted());
    canvas.setTextSize(2);
    canvas.setTextColor(accentBright(ACC_SETUP));
    canvas.setTextDatum(top_left);
    canvas.setCursor(UI_PAD, UI_INPUT_Y);
    canvas.print("> ");

    canvas.setTextColor(UI_RGB_FG);
    int inputX = UI_PAD + canvas.textWidth("> ");
    if (inputBuffer) canvas.drawString(inputBuffer, inputX, UI_INPUT_Y);

    int caretX = inputX + (inputBuffer ? canvas.textWidth(inputBuffer) : 0) + 1;
    drawCaret(caretX, UI_INPUT_Y);

    drawFooter3("ENTER", "", "ESC back", ACC_SETUP);

    canvas.pushSprite(0, 0);
}

void drawSetupSummaryScreen(const char* ssid, const char* serverIP, uint16_t port) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("SETUP", ACC_SETUP, false);

    canvas.setTextSize(2);
    int bodyLeft = UI_PAD;
    int y = UI_BODY_Y;

    canvas.setTextColor(accentBright(ACC_SETUP));
    canvas.setTextDatum(top_left);
    canvas.drawString("Review", bodyLeft, y);
    y += canvas.fontHeight() + 6;

    canvas.setTextSize(1);
    canvas.setTextColor(cMid());
    canvas.drawString("WiFi", bodyLeft, y);
    canvas.setTextColor(UI_RGB_FG);
    canvas.drawString(ssid ? ssid : "-", bodyLeft + 40, y);

    y += canvas.fontHeight() + 4;
    canvas.setTextColor(cMid());
    canvas.drawString("Server", bodyLeft, y);
    canvas.setTextColor(UI_RGB_FG);
    char serverInfo[40];
    snprintf(serverInfo, sizeof(serverInfo), "%s:%d", serverIP ? serverIP : "-", port);
    canvas.drawString(serverInfo, bodyLeft + 40, y);

    drawFooter3("[Y] Save", "", "[N] Retry", ACC_SETUP);

    canvas.pushSprite(0, 0);
}

// ============================================================================
// SETTINGS
// ============================================================================
void drawSettingsMenuScreen(bool hasConfig, const char* currentSSID, const char* currentServer) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("SETTINGS", ACC_SETUP, true);

    int bodyLeft = UI_PAD;
    int y = UI_BODY_Y;
    canvas.setTextSize(2);
    int rowH = canvas.fontHeight() + 4;

    struct { const char* key; const char* label; } items[] = {
        { "1", "WiFi" },
        { "2", "Server" },
        { "3", "Reset" }
    };

    for (int i = 0; i < 3; i++) {
        canvas.setTextColor(accentBright(ACC_SETUP));
        canvas.setTextDatum(top_left);
        canvas.setCursor(bodyLeft + 2, y);
        canvas.print(items[i].key);
        canvas.drawLine(bodyLeft + 18, y - 1, bodyLeft + 18, y + rowH - 3, cMuted());
        canvas.setTextColor(UI_RGB_FG);
        canvas.drawString(items[i].label, bodyLeft + 22, y);
        y += rowH;
    }

    if (hasConfig && currentSSID && currentServer) {
        y += 4;
        canvas.drawLine(bodyLeft, y - 3, 240 - UI_PAD, y - 3, cMuted());
        canvas.setTextSize(1);
        canvas.setTextColor(cDim());
        canvas.setCursor(bodyLeft, y);
        canvas.printf("Now: %s", currentSSID);
    }

    drawFooter3("1-3 pick", "", "ESC", ACC_SETUP);

    canvas.pushSprite(0, 0);
}

// ============================================================================
// WiFi / Server lists
// ============================================================================
static void drawNetworkListBody(const String networks[], int networkCount,
                                 const int8_t* rssi) {
    canvas.setTextSize(2);
    int rowH = canvas.fontHeight() + 4;
    int listTop = UI_BODY_Y;
    int listLeft = UI_PAD;
    int listRight = 240 - UI_PAD;
    int rssiW = rssi ? 50 : 0; // narrow width at size 2
    int nameMaxX = listRight - rssiW;

    for (int i = 0; i < networkCount && i < 5; i++) {
        int y = listTop + i * rowH;
        if (y + rowH > UI_BODY_BOTTOM_NOI) break;

        canvas.setTextColor(accentBright(ACC_SETUP));
        canvas.setTextDatum(top_left);
        canvas.setCursor(listLeft + 2, y);
        canvas.printf("%d", i + 1);

        canvas.drawLine(listLeft + 18, y - 1, listLeft + 18, y + rowH - 3, cMuted());

        canvas.setTextColor(UI_RGB_FG);
        const char* ssid = networks[i].c_str();
        char buf[32];
        strncpy(buf, ssid, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        int avail = nameMaxX - (listLeft + 22);
        while (canvas.textWidth(buf) > avail && strlen(buf) > 4) {
            int n = strlen(buf);
            buf[n - 1] = '\0';
            if (n >= 4) {
                buf[n - 2] = '.';
                buf[n - 3] = '.';
                buf[n - 4] = '.';
            }
        }
        canvas.drawString(buf, listLeft + 22, y);

        if (rssi) {
            int8_t signal = rssi[i];
            uint16_t col = canvas.color565(0x00, 0xCC, 0x66);
            if (signal < -70) col = canvas.color565(0xCC, 0x40, 0x40);
            else if (signal < -60) col = canvas.color565(0xFF, 0xB0, 0x00);
            canvas.setTextSize(1);
            canvas.setTextColor(col);
            canvas.setTextDatum(top_right);
            char rssiBuf[10];
            snprintf(rssiBuf, sizeof(rssiBuf), "%d", signal);
            canvas.drawString(rssiBuf, listRight - 1, y + 4);
            canvas.setTextSize(2);
        }
    }
}

void drawNetworkListScreen(const String networks[], int networkCount, int8_t rssi[]) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("WiFi", ACC_SETUP, false);
    drawNetworkListBody(networks, networkCount, rssi);
    drawFooter3("1-5 pick", "", "[R] Rescan", ACC_SETUP);

    canvas.pushSprite(0, 0);
}

void drawWiFiSelectScreen(const String networks[], int networkCount, int8_t rssi[], bool showSaved) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar(showSaved ? "WiFi (saved)" : "WiFi", ACC_SETUP, false);
    drawNetworkListBody(networks, networkCount, showSaved ? nullptr : rssi);
    drawFooter3("1-5 pick", "", showSaved ? "[N] New" : "[R] Rescan", ACC_SETUP);

    canvas.pushSprite(0, 0);
}

void drawServerSelectScreen(const char* ips[], int ports[], int count) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("Server", ACC_SETUP, false);

    canvas.setTextSize(2);
    int rowH = canvas.fontHeight() + 4;
    int listTop = UI_BODY_Y;
    int listLeft = UI_PAD;

    for (int i = 0; i < count && i < 5; i++) {
        int y = listTop + i * rowH;
        if (y + rowH > UI_BODY_BOTTOM_NOI) break;

        canvas.setTextColor(accentBright(ACC_SETUP));
        canvas.setTextDatum(top_left);
        canvas.setCursor(listLeft + 2, y);
        canvas.printf("%d", i + 1);
        canvas.drawLine(listLeft + 18, y - 1, listLeft + 18, y + rowH - 3, cMuted());

        canvas.setTextColor(UI_RGB_FG);
        canvas.setCursor(listLeft + 22, y);
        canvas.printf("%s:%d", ips[i], ports[i]);
    }

    drawFooter3("1-5 pick", "", "[N] New", ACC_SETUP);

    canvas.pushSprite(0, 0);
}
