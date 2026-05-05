#include "ui.h"
#include <M5Cardputer.h>
#include "config.h"

static M5Canvas canvas(&M5Cardputer.Display);
static bool canvasInitialized = false;

void initCanvasIfNeeded() {
    if (!canvasInitialized) {
        int dw = M5Cardputer.Display.width();
        int dh = M5Cardputer.Display.height();
        canvas.createSprite(dw, dh);
        canvas.setTextSize(1);
        canvasInitialized = true;
    }
}

int drawWordWrapped(const char* text, int x, int y, int maxWidth, uint16_t color) {
    if (!text) return y;
    canvas.setTextColor(color);
    canvas.setTextDatum(top_left);
    int cursorX = x;
    int cursorY = y;
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
        
        canvas.drawString(word, cursorX, cursorY);
        cursorX += wWidth + spaceWidth;
        word = strtok(NULL, " \n");
    }
    return cursorY + lineHeight;
}

void drawIdleScreen(const char* version, const char* ip) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    int dh = M5Cardputer.Display.height();
    
    canvas.fillSprite(BLACK);
    
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString("Waiting for agent...", dw / 2, dh / 2);
    
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

void drawAskScreen(const char* question, const char* context, const char* inputBuffer) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    
    canvas.fillSprite(BLACK);
    canvas.fillRect(0, 0, dw, 20, canvas.color565(0, 0, 128));
    
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_center);
    canvas.drawString("ASK", dw / 2, 10);
    
    int y = 25;
    if (question) y = drawWordWrapped(question, 5, y, dw - 10, TFT_YELLOW);
    if (context && strlen(context) > 0) {
        y += 5;
        y = drawWordWrapped(context, 5, y, dw - 10, TFT_LIGHTGREY);
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

void drawEscalateScreen(const char* question, const char* context, const char* inputBuffer) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    
    canvas.fillSprite(BLACK);
    canvas.fillRect(0, 0, dw, 20, canvas.color565(200, 100, 0));
    
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_center);
    canvas.drawString("ESCALATE", dw / 2, 10);
    
    int y = 25;
    if (question) y = drawWordWrapped(question, 5, y, dw - 10, TFT_YELLOW);
    if (context && strlen(context) > 0) {
        y += 5;
        y = drawWordWrapped(context, 5, y, dw - 10, TFT_LIGHTGREY);
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

void drawConfirmScreen(const char* statement, const char* consequence) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    int dh = M5Cardputer.Display.height();
    
    canvas.fillSprite(BLACK);
    canvas.fillRect(0, 0, dw, 20, canvas.color565(128, 0, 0));
    
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_center);
    canvas.drawString("CONFIRM", dw / 2, 10);
    
    int y = 25;
    if (statement) y = drawWordWrapped(statement, 5, y, dw - 10, TFT_ORANGE);
    if (consequence && strlen(consequence) > 0) {
        y += 5;
        y = drawWordWrapped(consequence, 5, y, dw - 10, TFT_LIGHTGREY);
    }
    
    canvas.setTextColor(TFT_WHITE);
    canvas.setTextDatum(bottom_center);
    canvas.drawString("[Y] Yes   [N] No", dw / 2, dh - 5);
    
    canvas.pushSprite(0, 0);
}

void drawChooseScreen(const char* question, const char* context, const String options[], int optionCount) {
    initCanvasIfNeeded();
    int dw = M5Cardputer.Display.width();
    
    canvas.fillSprite(BLACK);
    canvas.fillRect(0, 0, dw, 20, canvas.color565(0, 128, 128));
    
    canvas.setTextColor(WHITE);
    canvas.setTextDatum(middle_center);
    canvas.drawString("CHOOSE", dw / 2, 10);
    
    int y = 25;
    if (question) y = drawWordWrapped(question, 5, y, dw - 10, TFT_CYAN);
    if (context && strlen(context) > 0) {
        y += 5;
        y = drawWordWrapped(context, 5, y, dw - 10, TFT_LIGHTGREY);
    }
    
    y += 5;
    canvas.setTextDatum(top_left);
    for (int i = 0; i < optionCount && i < 6; i++) {
        canvas.setTextColor(TFT_WHITE);
        canvas.setCursor(5, y);
        canvas.printf("%d. ", i + 1);
        y = drawWordWrapped(options[i].c_str(), 20, y, dw - 25, TFT_WHITE);
    }
    
    canvas.pushSprite(0, 0);
}
