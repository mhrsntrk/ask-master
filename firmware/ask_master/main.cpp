#include <Arduino.h>
#include <M5Cardputer.h>
#include "config.h"
#include "config_manager.h"
#include "ui.h"
#include "ws_client.h"
#include <ArduinoJson.h>

enum State {
    CONNECTING,
    IDLE,
    RENDERING,
    WAITING_INPUT,
    SENDING,
    CONFIGURING
};

enum SetupStep {
    SETUP_SCANNING,
    SETUP_SELECT_NETWORK,
    SETUP_PASSWORD,
    SETUP_SERVER_IP,
    SETUP_SERVER_PORT,
    SETUP_CONFIRM
};

static constexpr const char* APP_VERSION = "ask-master v0.1";

State currentState = CONNECTING;
SetupStep setupStep = SETUP_SCANNING;
bool inSetupMode = false;
ConfigManager configManager;
WSClient wsClient;
char inputBuffer[81] = {0};
int inputLength = 0;
String currentQuestion;
String currentContext;
String currentOptions[6];
int currentOptionCount = 0;
String currentType;

String scannedNetworks[6];
int8_t scannedRSSI[6];
int scannedNetworkCount = 0;
bool networkScanComplete = false;

void onWSMessage(const String& message);
void onWSConnect();
void onWSDisconnect();
void renderCurrentScreen();
void handleKeyboard();
void handleSetupKeyboard();
void sendReply(const String& reply);
void clearCurrentPrompt();
void drawConnectingScreen();
void drawIdle();
void runSetupFlow();
void saveSetupAndConnect();
void startNormalOperation();

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);

    drawConnectingScreen();

    configManager.begin();
    bool hasConfig = configManager.load();

    if (!hasConfig) {
        runSetupFlow();
    } else {
        startNormalOperation();
    }
}

void loop() {
    M5Cardputer.update();
    wsClient.loop();
    handleKeyboard();
}

void scanNetworks() {
    networkScanComplete = false;
    scannedNetworkCount = 0;
    drawSetupScreen("Scanning...", "Looking for WiFi networks", "");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    if (n > 0) {
        for (int i = 0; i < n && scannedNetworkCount < 6; i++) {
            String ssid = WiFi.SSID(i);
            bool duplicate = false;
            for (int j = 0; j < scannedNetworkCount; j++) {
                if (scannedNetworks[j] == ssid) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate && ssid.length() > 0) {
                scannedNetworks[scannedNetworkCount] = ssid;
                scannedRSSI[scannedNetworkCount] = WiFi.RSSI(i);
                scannedNetworkCount++;
            }
        }
    }
    networkScanComplete = true;
    setupStep = SETUP_SELECT_NETWORK;
    drawNetworkListScreen(scannedNetworks, scannedNetworkCount, scannedRSSI);
    M5Cardputer.Speaker.tone(BEEP_FREQ_CHOOSE, BEEP_DURATION_MS);
}

void runSetupFlow() {
    inSetupMode = true;
    currentState = CONFIGURING;
    setupStep = SETUP_SCANNING;
    inputBuffer[0] = '\0';
    inputLength = 0;
    scanNetworks();
}

void saveSetupAndConnect() {
    configManager.save();
    inSetupMode = false;
    currentState = CONNECTING;
    drawConnectingScreen();
    startNormalOperation();
}

void startNormalOperation() {
    WiFi.begin(configManager.getWiFiSSID(), configManager.getWiFiPassword());
    int wifiTimeout = 0;
    const int maxWifiTimeout = 30; // 30 * 500ms = 15 seconds

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        drawConnectingScreen();
        wifiTimeout++;

        // Check for 'S' key to enter setup during connection
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
            for (char c : status.word) {
                if (c == 's' || c == 'S') {
                    WiFi.disconnect();
                    configManager.clear();
                    runSetupFlow();
                    return;
                }
            }
        }

        if (wifiTimeout >= maxWifiTimeout) {
            WiFi.disconnect();
            configManager.clear();
            runSetupFlow();
            return;
        }
    }

    wsClient.begin(configManager.getServerIP(), configManager.getServerPort());
    wsClient.onMessage(onWSMessage);
    wsClient.onConnect(onWSConnect);
    wsClient.onDisconnect(onWSDisconnect);

    currentState = IDLE;
    drawIdle();
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
    if (inSetupMode) {
        handleSetupKeyboard();
        return;
    }

    // Allow 'S' key to enter setup from IDLE or CONNECTING states
    if (currentState == IDLE || currentState == CONNECTING) {
        if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
            return;
        }
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        for (char c : status.word) {
            if (c == 's' || c == 'S') {
                WiFi.disconnect();
                configManager.clear();
                runSetupFlow();
                return;
            }
        }
        return;
    }

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

void handleSetupKeyboard() {
    if (currentState != CONFIGURING) {
        return;
    }

    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
        return;
    }

    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

    if (setupStep == SETUP_SCANNING) {
        return;
    }

    if (setupStep == SETUP_SELECT_NETWORK) {
        for (char c : status.word) {
            if (c == 'r' || c == 'R') {
                scanNetworks();
                return;
            }
            if (c >= '1' && c <= '6') {
                int choice = c - '0';
                if (choice <= scannedNetworkCount) {
                    configManager.setWiFiSSID(scannedNetworks[choice - 1].c_str());
                    setupStep = SETUP_PASSWORD;
                    inputBuffer[0] = '\0';
                    inputLength = 0;
                    drawSetupScreen("WiFi Password", scannedNetworks[choice - 1].c_str(), inputBuffer);
                    return;
                }
            }
        }
        return;
    }

    if (setupStep == SETUP_CONFIRM) {
        for (char c : status.word) {
            if (c == 'y' || c == 'Y') {
                saveSetupAndConnect();
                return;
            }
            if (c == 'n' || c == 'N') {
                configManager.clear();
                runSetupFlow();
                return;
            }
        }
        return;
    }

    for (char c : status.word) {
        if (inputLength < 80) {
            inputBuffer[inputLength++] = c;
            inputBuffer[inputLength] = '\0';
        }
    }

    if (status.del && inputLength > 0) {
        inputBuffer[--inputLength] = '\0';
    }

    if (status.enter && inputLength > 0) {
        switch (setupStep) {
            case SETUP_PASSWORD:
                configManager.setWiFiPassword(inputBuffer);
                setupStep = SETUP_SERVER_IP;
                inputBuffer[0] = '\0';
                inputLength = 0;
                drawSetupScreen("Server IP Address", "Your computer's IP (e.g. 192.168.1.5)", inputBuffer);
                break;
            case SETUP_SERVER_IP:
                configManager.setServerIP(inputBuffer);
                setupStep = SETUP_SERVER_PORT;
                inputBuffer[0] = '\0';
                inputLength = 0;
                strcpy(inputBuffer, "8765");
                inputLength = 4;
                drawSetupScreen("Server Port", "WebSocket port (default: 8765)", inputBuffer);
                break;
            case SETUP_SERVER_PORT:
                configManager.setServerPort(atoi(inputBuffer));
                setupStep = SETUP_CONFIRM;
                drawSetupSummaryScreen(
                    configManager.getWiFiSSID(),
                    configManager.getServerIP(),
                    configManager.getServerPort()
                );
                break;
            default:
                break;
        }
        return;
    }

    const char* label = "";
    const char* context = "";
    switch (setupStep) {
        case SETUP_PASSWORD:
            label = "WiFi Password";
            context = configManager.getWiFiSSID();
            break;
        case SETUP_SERVER_IP:
            label = "Server IP Address";
            context = "Your computer's IP (e.g. 192.168.1.5)";
            break;
        case SETUP_SERVER_PORT:
            label = "Server Port";
            context = "WebSocket port (default: 8765)";
            break;
        default:
            break;
    }
    drawSetupScreen(label, context, inputBuffer);
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
    drawIdleScreen("Connecting...", "Waiting for WiFi / WS", false);
}

void drawIdle() {
    drawIdleScreen(APP_VERSION, WiFi.localIP().toString().c_str(), true);
}
