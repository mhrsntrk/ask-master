#include "ui.h"
#include <M5Cardputer.h>
#include "config.h"
#include "sprites.h"

static M5Canvas canvas(&M5Cardputer.Display);
static bool canvasInitialized = false;

static unsigned long lastBlinkTime = 0;
static bool isBlinking = false;
static int sleepZOffset = 0;
static unsigned long lastZTime = 0;

void initCanvasIfNeeded() {
    if (!canvasInitialized) {
        int dw = M5Cardputer.Display.width();
        int dh = M5Cardputer.Display.height();
        canvas.createSprite(dw, dh);
        canvas.setTextSize(1);
        canvasInitialized = true;
    }
}

static void drawSpriteFrame(int cx, int cy, const uint8_t bitmap[SPRITE_HEIGHT][SPRITE_BYTES_PER_ROW]) {
    int x0 = cx - SPRITE_WIDTH / 2;
    int y0 = cy - SPRITE_HEIGHT / 2;
    for (int row = 0; row < SPRITE_HEIGHT; row++) {
        for (int col = 0; col < SPRITE_WIDTH; col++) {
            int byteIdx = col / 8;
            int bitIdx = 7 - (col % 8);
            uint8_t byteVal = pgm_read_byte(&bitmap[row][byteIdx]);
            if ((byteVal >> bitIdx) & 1) {
                canvas.drawPixel(x0 + col, y0 + row, TFT_WHITE);
            }
        }
    }
}

void drawCharacterFace(int cx, int cy, bool sleeping) {
    unsigned long now = millis();

    if (sleeping) {
        int frameIdx = (now / 800) % 2;
        if (frameIdx == 0) {
            drawSpriteFrame(cx, cy, SPRITE_SLEEP_1);
        } else {
            drawSpriteFrame(cx, cy, SPRITE_SLEEP_2);
        }

        if (now - lastZTime > 600) {
            lastZTime = now;
            sleepZOffset = 0;
        }
        sleepZOffset = (now - lastZTime) / 40;
        if (sleepZOffset < 12) {
            int alpha = 255 - (sleepZOffset * 21);
            uint16_t zColor = canvas.color565(alpha, alpha, alpha);
            canvas.setTextColor(zColor);
            canvas.setTextDatum(top_left);
            canvas.drawString("Z", cx + 10, cy - 8 - sleepZOffset);
            if (sleepZOffset > 3) {
                canvas.drawString("z", cx + 16, cy - 2 - sleepZOffset);
            }
        }
    } else {
        if (isBlinking) {
            if (now - lastBlinkTime > 150) {
                isBlinking = false;
                lastBlinkTime = now;
            }
            drawSpriteFrame(cx, cy, SPRITE_IDLE_2);
        } else {
            drawSpriteFrame(cx, cy, SPRITE_IDLE_1);
            if (now - lastBlinkTime > 3000 + (random() % 2000)) {
                isBlinking = true;
                lastBlinkTime = now;
            }
        }
    }
}

int drawWordWrapped(const char* text, int x, int y, int maxWidth, uint16_t color, int scrollY, int clipY, int clipHeight) {
    if (!text) return y;
    canvas.setTextColor(color);
    canvas.setTextDatum(top_left);
    int cursorX = x;
    int cursorY = y - scrollY;
    int lineHeight = canvas.fontHeight();
    int endClip = clipY + clipHeight;
    
    char buffer[512];
    strncpy(buffer, text, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';
    
    char* word = strtok(buffer, " \n");
    while (word != NULL) {
        int wWidth = canvas.textWidth(word);
        int spaceWidth = canvas.textWidth(" ");
        
        if (cursorX + wWidth > x + maxWidth && cursorX > x) {
            cursorX = x;
            cursorY += lineHeight;
        }
        
        if (cursorY + lineHeight > clipY && cursorY < endClip) {
            canvas.drawString(word, cursorX, cursorY);
        }
        cursorX += wWidth + spaceWidth;
        word = strtok(NULL, " \n");
    }
    return cursorY + lineHeight + scrollY;
}

int measureWordWrappedHeight(const char* text, int x, int maxWidth) {
    if (!text || text[0] == '\0') return 0;
    int cursorX = x;
    int cursorY = 0;
    int lineHeight = canvas.fontHeight();
    
    char buffer[512];
    strncpy(buffer, text, sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';
    
    char* word = strtok(buffer, " \n");
    while (word != NULL) {
        int wWidth = canvas.textWidth(word);
        int spaceWidth = canvas.textWidth(" ");
        
        if (cursorX + wWidth > x + maxWidth && cursorX > x) {
            cursorX = x;
            cursorY += lineHeight;
        }
        
        cursorX += wWidth + spaceWidth;
        word = strtok(NULL, " \n");
    }
    return cursorY + lineHeight;
}

void drawScrollIndicators(int dw, int scrollY, int maxScroll) {
    if (maxScroll <= 0) return;
    
    canvas.setTextDatum(top_right);
    if (scrollY > 0) {
        canvas.setTextColor(TFT_WHITE);
        canvas.drawString("\x18", dw - 2, 22);
    }
    if (scrollY < maxScroll) {
        canvas.setTextColor(TFT_WHITE);
        canvas.drawString("\x19", dw - 2, 55);
    }
}

void drawIdleScreen(const char* version, const char* ip, bool showSetupHint) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    int dh = M5Cardputer.Display.height();

    canvas.fillSprite(BLACK);

    drawCharacterFace(dw / 2, dh / 2 - 14, false);

    canvas.setTextDatum(middle_center);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Waiting for agent...", dw / 2, dh / 2 + 8);

    if (showSetupHint) {
        canvas.setTextColor(TFT_LIGHTGREY);
        canvas.setTextDatum(middle_center);
        canvas.drawString("[S] Settings", dw / 2, dh / 2 + 20);
    }

    if (version) {
        canvas.setTextColor(TFT_LIGHTGREY);
        canvas.setTextDatum(bottom_center);
        canvas.drawString(version, dw / 2, dh - 5);
    }

    if (ip) {
        canvas.setTextDatum(top_center);
        canvas.drawString(ip, dw / 2, 5);
    }

    canvas.pushSprite(0, 0);
}

void drawSetupScreen(const char* label, const char* context, const char* inputBuffer) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    
    canvas.fillSprite(BLACK);
    canvas.fillRect(0, 0, dw, 20, canvas.color565(0, 100, 0));
    
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_center);
    canvas.drawString("SETUP", dw / 2, 10);
    
    int y = 25;
    if (label) y = drawWordWrapped(label, 5, y, dw - 10, TFT_GREEN, 0, 0, 0);
    if (context && strlen(context) > 0) {
        y += 5;
        y = drawWordWrapped(context, 5, y, dw - 10, TFT_LIGHTGREY, 0, 0, 0);
    }
    
    y += 5;
    canvas.drawLine(0, y, dw, y, TFT_DARKGREY);
    y += 5;
    
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(top_left);
    canvas.setCursor(5, y);
    canvas.print("> ");
    if (inputBuffer) canvas.print(inputBuffer);
    
    canvas.pushSprite(0, 0);
}

void drawSetupSummaryScreen(const char* ssid, const char* serverIP, uint16_t port) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    int dh = M5Cardputer.Display.height();
    
    canvas.fillSprite(BLACK);
    canvas.fillRect(0, 0, dw, 20, canvas.color565(0, 100, 0));
    
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_center);
    canvas.drawString("SETUP", dw / 2, 10);
    
    int y = 25;
    canvas.setTextColor(TFT_GREEN);
    canvas.setTextDatum(top_left);
    canvas.setCursor(5, y);
    canvas.print("WiFi: ");
    canvas.setTextColor(TFT_WHITE);
    y = drawWordWrapped(ssid ? ssid : "-", 40, y, dw - 45, TFT_WHITE, 0, 0, 0);

    y += 5;
    canvas.setTextColor(TFT_GREEN);
    canvas.setCursor(5, y);
    canvas.print("Server: ");
    canvas.setTextColor(TFT_WHITE);
    char serverInfo[32];
    snprintf(serverInfo, sizeof(serverInfo), "%s:%d", serverIP ? serverIP : "-", port);
    y = drawWordWrapped(serverInfo, 50, y, dw - 55, TFT_WHITE, 0, 0, 0);
    
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(bottom_center);
    canvas.drawString("[Y] Save   [N] Retry", dw / 2, dh - 5);
    
    canvas.pushSprite(0, 0);
}

void drawAskScreen(const char* question, const char* context, const char* inputBuffer, int scrollY) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    
    canvas.fillSprite(BLACK);
    canvas.fillRect(0, 0, dw, 20, canvas.color565(0, 0, 128));
    
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_center);
    canvas.drawString("ASK", dw / 2, 10);
    
    int y = 25;
    if (question) y = drawWordWrapped(question, 5, y, dw - 10, TFT_YELLOW, scrollY, 25, 33);
    if (context && strlen(context) > 0) {
        y += 5;
        y = drawWordWrapped(context, 5, y, dw - 10, TFT_LIGHTGREY, scrollY, 25, 33);
    }
    
    canvas.drawLine(0, 60, dw, 60, TFT_DARKGREY);
    
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(top_left);
    canvas.setCursor(5, 65);
    canvas.print("> ");
    if (inputBuffer) canvas.print(inputBuffer);
    
    canvas.pushSprite(0, 0);
}

void drawEscalateScreen(const char* question, const char* context, const char* inputBuffer, int scrollY) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    
    canvas.fillSprite(BLACK);
    canvas.fillRect(0, 0, dw, 20, canvas.color565(200, 100, 0));
    
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_center);
    canvas.drawString("ESCALATE", dw / 2, 10);
    
    int y = 25;
    if (question) y = drawWordWrapped(question, 5, y, dw - 10, TFT_YELLOW, scrollY, 25, 33);
    if (context && strlen(context) > 0) {
        y += 5;
        y = drawWordWrapped(context, 5, y, dw - 10, TFT_LIGHTGREY, scrollY, 25, 33);
    }
    
    canvas.drawLine(0, 60, dw, 60, TFT_DARKGREY);
    
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(top_left);
    canvas.setCursor(5, 65);
    canvas.print("> ");
    if (inputBuffer) canvas.print(inputBuffer);
    
    canvas.pushSprite(0, 0);
}

void drawConfirmScreen(const char* statement, const char* consequence, int scrollY) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    int dh = M5Cardputer.Display.height();
    
    canvas.fillSprite(BLACK);
    canvas.fillRect(0, 0, dw, 20, canvas.color565(128, 0, 0));
    
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_center);
    canvas.drawString("CONFIRM", dw / 2, 10);
    
    int y = 25;
    if (statement) y = drawWordWrapped(statement, 5, y, dw - 10, TFT_ORANGE, scrollY, 25, 36);
    if (consequence && strlen(consequence) > 0) {
        y += 5;
        y = drawWordWrapped(consequence, 5, y, dw - 10, TFT_LIGHTGREY, scrollY, 25, 36);
    }
    
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(bottom_center);
    canvas.drawString("[Y] Yes   [N] No", dw / 2, dh - 5);
    
    canvas.pushSprite(0, 0);
}

void drawChooseScreen(const char* question, const char* context, const String options[], int optionCount, int scrollY) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    int dh = M5Cardputer.Display.height();
    
    canvas.fillSprite(BLACK);
    canvas.fillRect(0, 0, dw, 20, canvas.color565(0, 128, 128));
    
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_center);
    canvas.drawString("CHOOSE", dw / 2, 10);
    
    int y = 25;
    if (question) y = drawWordWrapped(question, 5, y, dw - 10, TFT_CYAN, scrollY, 25, 36);
    if (context && strlen(context) > 0) {
        y += 5;
        y = drawWordWrapped(context, 5, y, dw - 10, TFT_LIGHTGREY, scrollY, 25, 36);
    }
    
    y += 5;
    canvas.setTextDatum(top_left);
    for (int i = 0; i < optionCount && i < 6; i++) {
        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(5, y - scrollY);
        if (y - scrollY + canvas.fontHeight() > 25 && y - scrollY < dh - 5) {
            canvas.printf("%d. ", i + 1);
            y = drawWordWrapped(options[i].c_str(), 20, y, dw - 25, TFT_WHITE, scrollY, 25, 36);
        } else {
            y = drawWordWrapped(options[i].c_str(), 20, y, dw - 25, TFT_WHITE, scrollY, 25, 36);
        }
    }
    
    canvas.pushSprite(0, 0);
}

void drawSleepScreen() {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    int dh = M5Cardputer.Display.height();

    canvas.fillSprite(BLACK);

    drawCharacterFace(dw / 2, dh / 2 - 14, true);

    canvas.setTextDatum(middle_center);
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("Sleeping...", dw / 2, dh / 2 + 8);
    canvas.setTextColor(TFT_LIGHTGREY);
    canvas.setTextDatum(bottom_center);
    canvas.drawString("[S] Settings  [W] Wake", dw / 2, dh - 5);
    canvas.pushSprite(0, 0);
}

void drawSettingsMenuScreen(bool hasConfig, const char* currentSSID, const char* currentServer) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    int dh = M5Cardputer.Display.height();
    
    canvas.fillSprite(BLACK);
    canvas.fillRect(0, 0, dw, 20, canvas.color565(0, 100, 0));
    
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_center);
    canvas.drawString("SETTINGS", dw / 2, 10);
    
    int y = 25;
    canvas.setTextColor(TFT_GREEN);
    canvas.setTextDatum(top_left);
    canvas.setCursor(5, y);
    canvas.print("[1] WiFi");
    
    y += canvas.fontHeight() + 2;
    canvas.setTextColor(TFT_CYAN);
    canvas.setCursor(5, y);
    canvas.print("[2] Server");
    
    y += canvas.fontHeight() + 2;
    canvas.setTextColor(TFT_YELLOW);
    canvas.setCursor(5, y);
    canvas.print("[3] Reset All");
    
    if (hasConfig && currentSSID && currentServer) {
        y += canvas.fontHeight() + 4;
        canvas.setTextColor(TFT_LIGHTGREY);
        canvas.setCursor(5, y);
        canvas.print("Current:");
        y += canvas.fontHeight() + 1;
        canvas.setCursor(5, y);
        canvas.print(currentSSID);
        y += canvas.fontHeight() + 1;
        canvas.setCursor(5, y);
        canvas.print(currentServer);
    }
    
    canvas.pushSprite(0, 0);
}

void drawWiFiSelectScreen(const String networks[], int networkCount, int8_t rssi[], bool showSaved) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    int dh = M5Cardputer.Display.height();
    
    canvas.fillSprite(BLACK);
    canvas.fillRect(0, 0, dw, 20, canvas.color565(0, 80, 160));
    
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_center);
    if (showSaved) {
        canvas.drawString("SAVED NETWORKS", dw / 2, 10);
    } else {
        canvas.drawString("WiFi NETWORKS", dw / 2, 10);
    }
    
    int y = 25;
    canvas.setTextDatum(top_left);
    for (int i = 0; i < networkCount && i < 6; i++) {
        const char* ssid = networks[i].c_str();
        int nameWidth = canvas.textWidth(ssid);
        int maxNameWidth = dw - 50;
        
        canvas.setTextColor(TFT_CYAN);
        canvas.setCursor(5, y);
        canvas.printf("%d.", i + 1);
        
        canvas.setTextColor(TFT_WHITE);
        if (nameWidth > maxNameWidth) {
            char truncated[24];
            strncpy(truncated, ssid, sizeof(truncated) - 1);
            truncated[sizeof(truncated) - 1] = '\0';
            truncated[20] = '.';
            truncated[21] = '.';
            truncated[22] = '.';
            truncated[23] = '\0';
            canvas.drawString(truncated, 22, y);
        } else {
            canvas.drawString(ssid, 22, y);
        }
        
        if (rssi) {
            canvas.setTextColor(TFT_GREEN);
            int8_t signal = rssi[i];
            if (signal < -70) canvas.setTextColor(TFT_RED);
            else if (signal < -60) canvas.setTextColor(TFT_YELLOW);
            canvas.setCursor(dw - 35, y);
            canvas.printf("%ddBm", signal);
        }
        
        y += canvas.fontHeight() + 2;
    }
    
    if (showSaved) {
        canvas.setTextColor(TFT_LIGHTGREY);
        canvas.setTextDatum(bottom_center);
        canvas.drawString("1-6 Select  [N] New", dw / 2, dh - 5);
    } else {
        canvas.setTextColor(TFT_LIGHTGREY);
        canvas.setTextDatum(bottom_center);
        canvas.drawString("1-6 Select  [R] Rescan", dw / 2, dh - 5);
    }
    
    canvas.pushSprite(0, 0);
}

void drawServerSelectScreen(const char* ips[], int ports[], int count) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    int dh = M5Cardputer.Display.height();
    
    canvas.fillSprite(BLACK);
    canvas.fillRect(0, 0, dw, 20, canvas.color565(0, 80, 160));
    
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_center);
    canvas.drawString("SAVED SERVERS", dw / 2, 10);
    
    int y = 25;
    canvas.setTextDatum(top_left);
    for (int i = 0; i < count && i < 6; i++) {
        canvas.setTextColor(TFT_CYAN);
        canvas.setCursor(5, y);
        canvas.printf("%d.", i + 1);
        
        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(22, y);
        canvas.printf("%s:%d", ips[i], ports[i]);
        
        y += canvas.fontHeight() + 2;
    }
    
    canvas.setTextColor(TFT_LIGHTGREY);
    canvas.setTextDatum(bottom_center);
    canvas.drawString("1-6 Select  [N] New", dw / 2, dh - 5);
    
    canvas.pushSprite(0, 0);
}

void drawNetworkListScreen(const String networks[], int networkCount, int8_t rssi[]) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    int dh = M5Cardputer.Display.height();
    
    canvas.fillSprite(BLACK);
    canvas.fillRect(0, 0, dw, 20, canvas.color565(0, 80, 160));
    
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_center);
    canvas.drawString("WiFi NETWORKS", dw / 2, 10);
    
    int y = 25;
    canvas.setTextDatum(top_left);
    for (int i = 0; i < networkCount && i < 6; i++) {
        const char* ssid = networks[i].c_str();
        int nameWidth = canvas.textWidth(ssid);
        int maxNameWidth = dw - 50;
        
        canvas.setTextColor(TFT_CYAN);
        canvas.setCursor(5, y);
        canvas.printf("%d.", i + 1);
        
        canvas.setTextColor(TFT_WHITE);
        if (nameWidth > maxNameWidth) {
            char truncated[24];
            strncpy(truncated, ssid, sizeof(truncated) - 1);
            truncated[sizeof(truncated) - 1] = '\0';
            truncated[20] = '.';
            truncated[21] = '.';
            truncated[22] = '.';
            truncated[23] = '\0';
            canvas.drawString(truncated, 22, y);
        } else {
            canvas.drawString(ssid, 22, y);
        }
        
        canvas.setTextColor(TFT_GREEN);
        int8_t signal = rssi[i];
        if (signal < -70) canvas.setTextColor(TFT_RED);
        else if (signal < -60) canvas.setTextColor(TFT_YELLOW);
        canvas.setCursor(dw - 35, y);
        canvas.printf("%ddBm", signal);
        
        y += canvas.fontHeight() + 2;
    }
    
    canvas.setTextColor(TFT_LIGHTGREY);
    canvas.setTextDatum(bottom_center);
    canvas.drawString("1-6 Select  [R] Rescan", dw / 2, dh - 5);
    
    canvas.pushSprite(0, 0);
}
