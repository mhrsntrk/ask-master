#include <Arduino.h>
#include <M5Cardputer.h>
#include "config.h"
#include "ui.h"
#include "ws_client.h"
#include <ArduinoJson.h>

enum State {
    CONNECTING,
    IDLE,
    RENDERING,
    WAITING_INPUT,
    SENDING
};

static constexpr const char* APP_VERSION = "ask-master v0.1";

State currentState = CONNECTING;
WSClient wsClient;
char inputBuffer[81] = {0};
int inputLength = 0;
String currentQuestion;
String currentContext;
String currentOptions[6];
int currentOptionCount = 0;
String currentType;

void onWSMessage(const String& message);
void onWSConnect();
void onWSDisconnect();
void renderCurrentScreen();
void handleKeyboard();
void sendReply(const String& reply);
void clearCurrentPrompt();
void drawConnectingScreen();
void drawIdle();

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);

    drawConnectingScreen();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        drawConnectingScreen();
    }

    wsClient.begin(WS_HOST, WS_PORT);
    wsClient.onMessage(onWSMessage);
    wsClient.onConnect(onWSConnect);
    wsClient.onDisconnect(onWSDisconnect);

    currentState = IDLE;
    drawIdle();
}

void loop() {
    M5Cardputer.update();
    wsClient.loop();
    handleKeyboard();
}

void onWSMessage(const String& message) {
    if (currentState != IDLE) {
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        return;
    }

    clearCurrentPrompt();
    currentType = doc["type"].as<String>();
    currentQuestion = doc["question"].as<String>();
    currentContext = doc["context"].as<String>();

    if (currentType == "choose") {
        JsonArray opts = doc["options"].as<JsonArray>();
        for (JsonVariant option : opts) {
            if (currentOptionCount >= 6) {
                break;
            }
            currentOptions[currentOptionCount++] = option.as<String>();
        }
    }

    currentState = RENDERING;
    renderCurrentScreen();

    if (currentType == "ask") {
        M5Cardputer.Speaker.tone(BEEP_FREQ_ASK, BEEP_DURATION_MS);
    } else if (currentType == "escalate") {
        M5Cardputer.Speaker.tone(BEEP_FREQ_ESCALATE, BEEP_DURATION_MS);
    } else if (currentType == "confirm") {
        M5Cardputer.Speaker.tone(BEEP_FREQ_CONFIRM, BEEP_DURATION_MS);
    } else if (currentType == "choose") {
        M5Cardputer.Speaker.tone(BEEP_FREQ_CHOOSE, BEEP_DURATION_MS);
    }

    currentState = WAITING_INPUT;
}

void onWSConnect() {
    clearCurrentPrompt();
    currentState = IDLE;
    drawIdle();
}

void onWSDisconnect() {
    clearCurrentPrompt();
    currentState = CONNECTING;
    drawConnectingScreen();
}

void renderCurrentScreen() {
    if (currentType == "ask") {
        drawAskScreen(currentQuestion.c_str(), currentContext.c_str(), inputBuffer);
    } else if (currentType == "escalate") {
        drawEscalateScreen(currentQuestion.c_str(), currentContext.c_str(), inputBuffer);
    } else if (currentType == "confirm") {
        drawConfirmScreen(currentQuestion.c_str(), currentContext.c_str());
    } else if (currentType == "choose") {
        drawChooseScreen(currentQuestion.c_str(), currentContext.c_str(), currentOptions, currentOptionCount);
    }
}

void handleKeyboard() {
    if (currentState != WAITING_INPUT) {
        return;
    }

    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
        return;
    }

    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

    if (currentType == "ask" || currentType == "escalate") {
        for (char c : status.word) {
            if (inputLength < 80) {
                inputBuffer[inputLength++] = c;
                inputBuffer[inputLength] = '\0';
            }
        }

        if (status.del && inputLength > 0) {
            inputBuffer[--inputLength] = '\0';
        }

        if (status.enter) {
            sendReply(String(inputBuffer));
            return;
        }

        if (currentType == "ask") {
            drawAskScreen(currentQuestion.c_str(), currentContext.c_str(), inputBuffer);
        } else {
            drawEscalateScreen(currentQuestion.c_str(), currentContext.c_str(), inputBuffer);
        }
        return;
    }

    if (currentType == "confirm") {
        for (char c : status.word) {
            if (c == 'y' || c == 'Y') {
                sendReply("y");
                return;
            }
            if (c == 'n' || c == 'N') {
                sendReply("n");
                return;
            }
        }
        return;
    }

    if (currentType == "choose") {
        for (char c : status.word) {
            if (c >= '1' && c <= '6') {
                int choice = c - '0';
                if (choice <= currentOptionCount) {
                    sendReply(String(c));
                    return;
                }
            }
        }
    }
}

void sendReply(const String& reply) {
    currentState = SENDING;
    wsClient.send(reply);
    M5Cardputer.Speaker.tone(BEEP_ANSWER_FREQ, BEEP_ANSWER_DURATION_MS);

    clearCurrentPrompt();
    currentState = IDLE;
    drawIdle();
}

void clearCurrentPrompt() {
    inputBuffer[0] = '\0';
    inputLength = 0;
    currentQuestion = "";
    currentContext = "";
    currentOptionCount = 0;
    currentType = "";

    for (int i = 0; i < 6; ++i) {
        currentOptions[i] = "";
    }
}

void drawConnectingScreen() {
    drawIdleScreen("Connecting...", "Waiting for WiFi / WS");
}

void drawIdle() {
    drawIdleScreen(APP_VERSION, WiFi.localIP().toString().c_str());
}
