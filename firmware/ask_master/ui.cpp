#include "ui.h"
#include "ui_theme.h"
#include "sprites.h"
#include <M5Cardputer.h>
#include "config.h"

// ============================================================================
// ask-master UI — Modern Terminal + 24x24 Mascot
// ============================================================================

static M5Canvas canvas(&M5Cardputer.Display);
static bool canvasInitialized = false;

// Caret blink state
static unsigned long lastCaretToggle = 0;
static bool caretVisible = true;

// Idle face blink state (reuses existing pattern)
static unsigned long lastBlinkTime = 0;
static bool isBlinking = false;

// Escalate flash state (250 ms on / 250 ms off)
static unsigned long lastFlashTime = 0;
static bool escalateBright = true;

// Sleep Z particle state (kept from previous UI)
static int sleepZOffset = 0;
static unsigned long lastZTime = 0;

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
static inline uint16_t accentMid(const UIAccent& a) {
    return canvas.color565((a.r * 3) / 4, (a.g * 3) / 4, (a.b * 3) / 4);
}
static inline uint16_t accentDim(const UIAccent& a) {
    return canvas.color565(a.r / 2, a.g / 2, a.b / 2);
}

static inline uint16_t cDim()   { return canvas.color565(0x7C, 0x7C, 0x7C); }
static inline uint16_t cMuted() { return canvas.color565(0x40, 0x40, 0x40); }
static inline uint16_t cDimmer() { return canvas.color565(0x20, 0x20, 0x20); }

// ---- Sprite renderer (24x24 4-color from ASCII strings) -------------------
static void drawSprite24(int cx, int cy, const Sprite24& sprite, const UIAccent& acc) {
    int x0 = cx - SPRITE_W / 2;
    int y0 = cy - SPRITE_H / 2;
    uint16_t br = accentBright(acc);
    uint16_t md = accentMid(acc);
    uint16_t dm = accentDim(acc);

    for (int row = 0; row < SPRITE_H; row++) {
        const char* line = sprite[row]; // ESP32: flash is memory-mapped
        if (!line) continue;
        for (int col = 0; col < SPRITE_W; col++) {
            char c = line[col];
            uint16_t color;
            switch (c) {
                case '#': color = br; break;
                case '=': color = md; break;
                case '+': color = dm; break;
                default:  continue;
            }
            canvas.drawPixel(x0 + col, y0 + row, color);
        }
    }
}

// ---- Status bar (top 8 px) -------------------------------------------------
static void drawStatusBar(const char* label, bool online) {
    // Background
    canvas.fillRect(0, UI_STATUS_TOP, 240, UI_STATUS_BOTTOM, UI_RGB_BG);

    // Left: state label
    canvas.setTextColor(cDim());
    canvas.setTextDatum(top_left);
    canvas.drawString(label ? label : "", UI_PAD_X, 0);

    // Right: connection dot + ON/OFF
    canvas.setTextDatum(top_right);
    if (online) {
        canvas.fillCircle(240 - UI_PAD_X - 4, 4, 2, canvas.color565(0x00, 0xCC, 0x66));
        canvas.setTextColor(cDim());
        canvas.drawString("ON", 240 - UI_PAD_X - 10, 0);
    } else {
        canvas.fillCircle(240 - UI_PAD_X - 4, 4, 2, canvas.color565(0xCC, 0x40, 0x40));
        canvas.setTextColor(cDim());
        canvas.drawString("OFF", 240 - UI_PAD_X - 10, 0);
    }

    // Hairline separator
    canvas.drawLine(0, UI_HAIRLINE_TOP_Y, 240, UI_HAIRLINE_TOP_Y, cMuted());
}

// ---- Footer (bottom 15 px, accent fill, black text) -----------------------
static void drawFooter(const char* hint, const UIAccent& acc) {
    canvas.drawLine(0, UI_HAIRLINE_BOT_Y, 240, UI_HAIRLINE_BOT_Y, cMuted());
    canvas.fillRect(0, UI_FOOTER_TOP, 240, UI_FOOTER_BOTTOM - UI_FOOTER_TOP + 1,
                    accentBright(acc));
    canvas.setTextColor(UI_RGB_BG);
    canvas.setTextDatum(middle_center);
    canvas.drawString(hint ? hint : "", 120, (UI_FOOTER_TOP + UI_FOOTER_BOTTOM) / 2);
}

// ---- Dim footer (no accent, for less prominent states) --------------------
static void drawFooterDim(const char* hint) {
    canvas.drawLine(0, UI_HAIRLINE_BOT_Y, 240, UI_HAIRLINE_BOT_Y, cMuted());
    canvas.setTextColor(cDim());
    canvas.setTextDatum(middle_center);
    canvas.drawString(hint ? hint : "", 120, (UI_FOOTER_TOP + UI_FOOTER_BOTTOM) / 2);
}

// ---- Caret -----------------------------------------------------------------
static void drawCaret(int x, int y) {
    unsigned long now = millis();
    if (now - lastCaretToggle > 500) {
        lastCaretToggle = now;
        caretVisible = !caretVisible;
    }
    if (caretVisible) {
        canvas.fillRect(x, y, 1, 8, UI_RGB_FG);
    }
}

// ---- Word-wrapped text rendering ------------------------------------------
int drawWordWrappedColored(const char* text, int x, int y, int maxWidth,
                            uint16_t color, int scrollY, int clipTop, int clipBottom) {
    if (!text) return y;
    canvas.setTextColor(color);
    canvas.setTextDatum(top_left);
    int cursorX = x;
    int cursorY = y - scrollY;
    int lineHeight = canvas.fontHeight();

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
    int lineHeight = canvas.fontHeight();

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

// ---- Scroll affordances ----------------------------------------------------
static void drawScrollHints(int scrollY, int contentHeight, int viewportHeight) {
    if (contentHeight <= viewportHeight) return;
    uint16_t mark = cDim();
    canvas.setTextColor(mark);
    canvas.setTextDatum(top_right);
    if (scrollY > 0) {
        canvas.fillTriangle(232, UI_BODY_TOP + 2, 236, UI_BODY_TOP + 2,
                            234, UI_BODY_TOP - 1, mark);
    }
    if (scrollY < contentHeight - viewportHeight) {
        canvas.fillTriangle(232, UI_BODY_BOTTOM - 2, 236, UI_BODY_BOTTOM - 2,
                            234, UI_BODY_BOTTOM + 1, mark);
    }
}

// ============================================================================
// IDLE screen
// ============================================================================
void drawIdleScreen(const char* version, const char* ip, bool showSetupHint) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    // Status bar: show IP as "label" if it's a real status; otherwise "IDLE"
    bool connecting = version && strstr(version, "Connecting") != nullptr;
    drawStatusBar(connecting ? "CONNECTING" : "IDLE", !connecting);

    // Mascot centered horizontally, top of body
    drawSprite24(120, UI_BODY_TOP + 24, SPRITE_IDLE, ACC_IDLE);

    // Animated blink (swap sprite occasionally)
    unsigned long now = millis();
    if (isBlinking) {
        if (now - lastBlinkTime > 150) {
            isBlinking = false;
            lastBlinkTime = now;
        }
        drawSprite24(120, UI_BODY_TOP + 24, SPRITE_IDLE_BLINK, ACC_IDLE);
    } else {
        if (now - lastBlinkTime > 3000 + (random() % 2000)) {
            isBlinking = true;
            lastBlinkTime = now;
        }
    }

    // Secondary text under mascot
    canvas.setTextColor(UI_RGB_FG);
    canvas.setTextDatum(middle_center);
    canvas.drawString(ip ? ip : "Waiting for agent...", 120, UI_BODY_TOP + 60);

    // Version mini-line (dim)
    if (version && !connecting) {
        canvas.setTextColor(cDim());
        canvas.setTextDatum(middle_center);
        canvas.drawString(version, 120, UI_BODY_TOP + 78);
    }

    // Footer
    if (showSetupHint) {
        drawFooterDim("[S] Settings");
    } else {
        drawFooterDim("");
    }

    canvas.pushSprite(0, 0);
}

// ============================================================================
// SLEEP screen
// ============================================================================
void drawSleepScreen() {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("SLEEP", true);

    UIAccent acc = ACC_IDLE;
    drawSprite24(120, UI_BODY_TOP + 28, SPRITE_SLEEP, acc);

    // Z particles
    unsigned long now = millis();
    if (now - lastZTime > 600) {
        lastZTime = now;
        sleepZOffset = 0;
    }
    sleepZOffset = (now - lastZTime) / 40;
    if (sleepZOffset < 14) {
        int alpha = 255 - (sleepZOffset * 18);
        uint16_t zColor = canvas.color565(alpha, alpha, alpha);
        canvas.setTextColor(zColor);
        canvas.setTextDatum(top_left);
        canvas.drawString("Z", 138, UI_BODY_TOP + 14 - sleepZOffset);
        if (sleepZOffset > 4) {
            canvas.drawString("z", 144, UI_BODY_TOP + 18 - sleepZOffset);
        }
    }

    canvas.setTextDatum(middle_center);
    canvas.setTextColor(cDim());
    canvas.drawString("Sleeping...", 120, UI_BODY_TOP + 66);

    drawFooterDim("[S] Settings   [W] Wake");

    canvas.pushSprite(0, 0);
}

// ============================================================================
// ASK / ESCALATE — shared question + free-text input layout
// ============================================================================
static void drawQuestionScreen(const char* stateLabel, const UIAccent& acc,
                                const Sprite24& sprite, const char* question,
                                const char* context, const char* inputBuffer,
                                int scrollY, bool escalateFlash) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    UIAccent renderAcc = acc;
    if (escalateFlash) {
        unsigned long now = millis();
        if (now - lastFlashTime > 250) {
            lastFlashTime = now;
            escalateBright = !escalateBright;
        }
        if (!escalateBright) {
            // Half-brightness frame
            renderAcc.r = (acc.r * 2) / 5;
            renderAcc.g = (acc.g * 2) / 5;
            renderAcc.b = (acc.b * 2) / 5;
        }
    }

    drawStatusBar(stateLabel, true);

    // Mascot top-left of body
    drawSprite24(UI_MASCOT_CX, UI_MASCOT_CY, sprite, renderAcc);

    // Body text region (right of mascot)
    int bodyLeft = UI_BODY_TEXT_X;
    int bodyRight = 240 - UI_PAD_X;
    int bodyMaxW = bodyRight - bodyLeft;
    int bodyTop = UI_BODY_TOP;
    int inputAreaTop = UI_BODY_BOTTOM - 12;
    int textClipBottom = inputAreaTop - 2;

    // Question (white, primary)
    int y = bodyTop;
    if (question) {
        y = drawWordWrappedColored(question, bodyLeft, y, bodyMaxW,
                                    UI_RGB_FG, scrollY, bodyTop, textClipBottom);
    }
    // Context (dim, smaller visual weight)
    if (context && context[0] != '\0') {
        y += 3;
        y = drawWordWrappedColored(context, bodyLeft, y, bodyMaxW,
                                    cDim(), scrollY, bodyTop, textClipBottom);
    }

    int contentHeight = y - bodyTop;
    int viewportHeight = textClipBottom - bodyTop;
    drawScrollHints(scrollY, contentHeight, viewportHeight);

    // Input separator + prompt
    canvas.drawLine(bodyLeft, inputAreaTop - 1, bodyRight, inputAreaTop - 1, cMuted());
    canvas.setTextColor(accentBright(renderAcc));
    canvas.setTextDatum(top_left);
    canvas.setCursor(bodyLeft, inputAreaTop);
    canvas.print("> ");

    canvas.setTextColor(UI_RGB_FG);
    int inputX = bodyLeft + canvas.textWidth("> ");
    if (inputBuffer) canvas.drawString(inputBuffer, inputX, inputAreaTop);

    int caretX = inputX + (inputBuffer ? canvas.textWidth(inputBuffer) : 0) + 1;
    drawCaret(caretX, inputAreaTop);

    // Footer
    drawFooter("ENTER send   ESC cancel", renderAcc);

    canvas.pushSprite(0, 0);
}

void drawAskScreen(const char* question, const char* context, const char* inputBuffer, int scrollY) {
    drawQuestionScreen("ASK", ACC_ASK, SPRITE_ASK, question, context, inputBuffer, scrollY, false);
}

void drawEscalateScreen(const char* question, const char* context, const char* inputBuffer, int scrollY) {
    drawQuestionScreen("ESCALATE", ACC_ESCALATE, SPRITE_ESCALATE,
                       question, context, inputBuffer, scrollY, true);
}

// ============================================================================
// CONFIRM — yes/no
// ============================================================================
void drawConfirmScreen(const char* statement, const char* consequence, int scrollY) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("CONFIRM", true);
    drawSprite24(UI_MASCOT_CX, UI_MASCOT_CY, SPRITE_CONFIRM, ACC_CONFIRM);

    int bodyLeft = UI_BODY_TEXT_X;
    int bodyRight = 240 - UI_PAD_X;
    int bodyMaxW = bodyRight - bodyLeft;
    int bodyTop = UI_BODY_TOP;
    int textClipBottom = UI_BODY_BOTTOM;

    int y = bodyTop;
    if (statement) {
        y = drawWordWrappedColored(statement, bodyLeft, y, bodyMaxW,
                                    UI_RGB_FG, scrollY, bodyTop, textClipBottom);
    }
    if (consequence && consequence[0] != '\0') {
        y += 3;
        y = drawWordWrappedColored(consequence, bodyLeft, y, bodyMaxW,
                                    cDim(), scrollY, bodyTop, textClipBottom);
    }

    int contentHeight = y - bodyTop;
    int viewportHeight = textClipBottom - bodyTop;
    drawScrollHints(scrollY, contentHeight, viewportHeight);

    drawFooter("[Y] Yes   [N] No   ESC cancel", ACC_CONFIRM);

    canvas.pushSprite(0, 0);
}

// ============================================================================
// CHOOSE — 2-6 options
// ============================================================================
void drawChooseScreen(const char* question, const char* context, const String options[], int optionCount, int scrollY) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("CHOOSE", true);
    drawSprite24(UI_MASCOT_CX, UI_MASCOT_CY, SPRITE_CHOOSE, ACC_CHOOSE);

    int bodyLeft = UI_BODY_TEXT_X;
    int bodyRight = 240 - UI_PAD_X;
    int bodyMaxW = bodyRight - bodyLeft;
    int bodyTop = UI_BODY_TOP;
    int optionsTop = bodyTop + 12;

    // Question on top row
    int y = bodyTop;
    if (question) {
        y = drawWordWrappedColored(question, bodyLeft, y, bodyMaxW,
                                    UI_RGB_FG, 0, bodyTop, optionsTop - 1);
    }
    if (context && context[0] != '\0') {
        canvas.setTextColor(cDim());
        canvas.setTextDatum(top_left);
        canvas.drawString(context, bodyLeft, y);
    }

    // Options list — full width below mascot row
    int rowY = optionsTop;
    int optLeft = UI_PAD_X + 2;
    int optRight = 240 - UI_PAD_X;
    int rowH = canvas.fontHeight() + 4;
    int maxRows = (UI_BODY_BOTTOM - optionsTop) / rowH;

    for (int i = 0; i < optionCount && i < 6; i++) {
        int yPos = rowY + i * rowH - scrollY;
        if (yPos + rowH < optionsTop) continue;
        if (yPos > UI_BODY_BOTTOM) break;

        // Index bullet (in accent color)
        canvas.setTextColor(accentBright(ACC_CHOOSE));
        canvas.setTextDatum(top_left);
        canvas.setCursor(optLeft, yPos);
        canvas.printf("%d", i + 1);

        // Vertical separator
        canvas.drawLine(optLeft + 8, yPos - 1, optLeft + 8, yPos + rowH - 3, cMuted());

        // Option text
        canvas.setTextColor(UI_RGB_FG);
        canvas.drawString(options[i].c_str(), optLeft + 14, yPos);
    }

    // Scroll hints — measure total rows
    int totalH = optionCount * rowH;
    int viewportH = UI_BODY_BOTTOM - optionsTop;
    drawScrollHints(scrollY, totalH, viewportH);

    drawFooter("1-6 pick   ESC cancel", ACC_CHOOSE);

    canvas.pushSprite(0, 0);
}

// ============================================================================
// SETUP family (label/context/input)
// ============================================================================
void drawSetupScreen(const char* label, const char* context, const char* inputBuffer) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("SETUP", false);
    drawSprite24(UI_MASCOT_CX, UI_MASCOT_CY, SPRITE_SETUP, ACC_SETUP);

    int bodyLeft = UI_BODY_TEXT_X;
    int bodyRight = 240 - UI_PAD_X;
    int bodyMaxW = bodyRight - bodyLeft;
    int bodyTop = UI_BODY_TOP;
    int inputAreaTop = UI_BODY_BOTTOM - 12;

    int y = bodyTop;
    if (label) {
        y = drawWordWrappedColored(label, bodyLeft, y, bodyMaxW,
                                    accentBright(ACC_SETUP), 0, bodyTop, inputAreaTop - 2);
    }
    if (context && context[0] != '\0') {
        y += 3;
        y = drawWordWrappedColored(context, bodyLeft, y, bodyMaxW,
                                    cDim(), 0, bodyTop, inputAreaTop - 2);
    }

    canvas.drawLine(bodyLeft, inputAreaTop - 1, bodyRight, inputAreaTop - 1, cMuted());
    canvas.setTextColor(accentBright(ACC_SETUP));
    canvas.setTextDatum(top_left);
    canvas.setCursor(bodyLeft, inputAreaTop);
    canvas.print("> ");

    canvas.setTextColor(UI_RGB_FG);
    int inputX = bodyLeft + canvas.textWidth("> ");
    if (inputBuffer) canvas.drawString(inputBuffer, inputX, inputAreaTop);

    int caretX = inputX + (inputBuffer ? canvas.textWidth(inputBuffer) : 0) + 1;
    drawCaret(caretX, inputAreaTop);

    drawFooter("ENTER next   ESC back", ACC_SETUP);

    canvas.pushSprite(0, 0);
}

void drawSetupSummaryScreen(const char* ssid, const char* serverIP, uint16_t port) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("SETUP", false);
    drawSprite24(UI_MASCOT_CX, UI_MASCOT_CY, SPRITE_SETUP, ACC_SETUP);

    int bodyLeft = UI_BODY_TEXT_X;
    int y = UI_BODY_TOP;

    canvas.setTextColor(accentBright(ACC_SETUP));
    canvas.setTextDatum(top_left);
    canvas.setCursor(bodyLeft, y);
    canvas.print("Review settings:");

    y += canvas.fontHeight() + 6;
    canvas.setTextColor(cDim());
    canvas.setCursor(bodyLeft, y);
    canvas.print("WiFi");
    canvas.setTextColor(UI_RGB_FG);
    canvas.drawString(ssid ? ssid : "-", bodyLeft + 38, y);

    y += canvas.fontHeight() + 4;
    canvas.setTextColor(cDim());
    canvas.setCursor(bodyLeft, y);
    canvas.print("Server");
    canvas.setTextColor(UI_RGB_FG);
    char serverInfo[40];
    snprintf(serverInfo, sizeof(serverInfo), "%s:%d", serverIP ? serverIP : "-", port);
    canvas.drawString(serverInfo, bodyLeft + 38, y);

    drawFooter("[Y] Save   [N] Retry", ACC_SETUP);

    canvas.pushSprite(0, 0);
}

// ============================================================================
// SETTINGS menu
// ============================================================================
void drawSettingsMenuScreen(bool hasConfig, const char* currentSSID, const char* currentServer) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("SETTINGS", true);
    drawSprite24(UI_MASCOT_CX, UI_MASCOT_CY, SPRITE_SETUP, ACC_SETUP);

    int bodyLeft = UI_BODY_TEXT_X;
    int y = UI_BODY_TOP;
    int rowH = canvas.fontHeight() + 4;

    struct { const char* key; const char* label; } items[] = {
        { "1", "WiFi network" },
        { "2", "Server" },
        { "3", "Reset all" }
    };

    for (int i = 0; i < 3; i++) {
        canvas.setTextColor(accentBright(ACC_SETUP));
        canvas.setTextDatum(top_left);
        canvas.setCursor(bodyLeft, y);
        canvas.print(items[i].key);
        canvas.drawLine(bodyLeft + 8, y - 1, bodyLeft + 8, y + rowH - 3, cMuted());
        canvas.setTextColor(UI_RGB_FG);
        canvas.drawString(items[i].label, bodyLeft + 14, y);
        y += rowH;
    }

    if (hasConfig && currentSSID && currentServer) {
        y += 4;
        canvas.drawLine(bodyLeft, y - 2, 240 - UI_PAD_X, y - 2, cMuted());
        canvas.setTextColor(cDim());
        canvas.setCursor(bodyLeft, y);
        canvas.printf("Now: %s", currentSSID);
        y += canvas.fontHeight() + 1;
        canvas.setCursor(bodyLeft, y);
        canvas.print(currentServer);
    }

    drawFooter("1-3 pick   ESC back", ACC_SETUP);

    canvas.pushSprite(0, 0);
}

// ============================================================================
// WiFi network list
// ============================================================================
static void drawNetworkListBody(const String networks[], int networkCount,
                                 const int8_t* rssi, bool isSaved) {
    int rowH = canvas.fontHeight() + 4;
    int listTop = UI_BODY_TOP + 2;
    int listLeft = UI_PAD_X + 2;
    int listRight = 240 - UI_PAD_X;
    int nameMaxX = rssi ? listRight - 42 : listRight;

    for (int i = 0; i < networkCount && i < 6; i++) {
        int y = listTop + i * rowH;
        if (y + rowH > UI_BODY_BOTTOM) break;

        canvas.setTextColor(accentBright(ACC_SETUP));
        canvas.setTextDatum(top_left);
        canvas.setCursor(listLeft, y);
        canvas.printf("%d", i + 1);

        canvas.drawLine(listLeft + 8, y - 1, listLeft + 8, y + rowH - 3, cMuted());

        // Name (truncated if too long)
        canvas.setTextColor(UI_RGB_FG);
        const char* ssid = networks[i].c_str();
        char truncated[28];
        strncpy(truncated, ssid, sizeof(truncated) - 1);
        truncated[sizeof(truncated) - 1] = '\0';
        int avail = nameMaxX - (listLeft + 14);
        while (canvas.textWidth(truncated) > avail && strlen(truncated) > 4) {
            truncated[strlen(truncated) - 1] = '\0';
            int n = strlen(truncated);
            if (n >= 3) {
                truncated[n - 1] = '.';
                truncated[n - 2] = '.';
                truncated[n - 3] = '.';
            }
        }
        canvas.drawString(truncated, listLeft + 14, y);

        // RSSI badge
        if (rssi) {
            int8_t signal = rssi[i];
            uint16_t col = canvas.color565(0x00, 0xCC, 0x66);
            if (signal < -70) col = canvas.color565(0xCC, 0x40, 0x40);
            else if (signal < -60) col = canvas.color565(0xFF, 0xB0, 0x00);
            canvas.setTextColor(col);
            canvas.setTextDatum(top_right);
            char rssiBuf[8];
            snprintf(rssiBuf, sizeof(rssiBuf), "%ddBm", signal);
            canvas.drawString(rssiBuf, listRight - 1, y);
        }
    }
}

void drawNetworkListScreen(const String networks[], int networkCount, int8_t rssi[]) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("WiFi", false);
    drawNetworkListBody(networks, networkCount, rssi, false);
    drawFooter("1-6 pick   [R] Rescan", ACC_SETUP);

    canvas.pushSprite(0, 0);
}

void drawWiFiSelectScreen(const String networks[], int networkCount, int8_t rssi[], bool showSaved) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar(showSaved ? "WiFi (saved)" : "WiFi", false);
    drawNetworkListBody(networks, networkCount, showSaved ? nullptr : rssi, showSaved);
    drawFooter(showSaved ? "1-6 pick   [N] New" : "1-6 pick   [R] Rescan", ACC_SETUP);

    canvas.pushSprite(0, 0);
}

void drawServerSelectScreen(const char* ips[], int ports[], int count) {
    initCanvasIfNeeded();
    canvas.fillSprite(UI_RGB_BG);

    drawStatusBar("Server", false);

    int rowH = canvas.fontHeight() + 4;
    int listTop = UI_BODY_TOP + 2;
    int listLeft = UI_PAD_X + 2;

    for (int i = 0; i < count && i < 6; i++) {
        int y = listTop + i * rowH;
        if (y + rowH > UI_BODY_BOTTOM) break;

        canvas.setTextColor(accentBright(ACC_SETUP));
        canvas.setTextDatum(top_left);
        canvas.setCursor(listLeft, y);
        canvas.printf("%d", i + 1);
        canvas.drawLine(listLeft + 8, y - 1, listLeft + 8, y + rowH - 3, cMuted());

        canvas.setTextColor(UI_RGB_FG);
        canvas.setCursor(listLeft + 14, y);
        canvas.printf("%s:%d", ips[i], ports[i]);
    }

    drawFooter("1-6 pick   [N] New", ACC_SETUP);

    canvas.pushSprite(0, 0);
}
