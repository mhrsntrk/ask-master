#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFiUdp.h>
#include "config.h"
#include "config_manager.h"
#include "ui.h"
#include "ws_client.h"
#include <ArduinoJson.h>

#define DEBUG_SERIAL

enum State {
    SLEEP,
    WAKE,
    CONNECTING,
    IDLE,
    RENDERING,
    WAITING_INPUT,
    SENDING,
    CONFIGURING
};

enum SetupStep {
    SETUP_MENU,
    SETUP_WIFI_SELECT,
    SETUP_SCANNING,
    SETUP_SELECT_NETWORK,
    SETUP_PASSWORD,
    SETUP_SERVER_SELECT,
    SETUP_SERVER_IP,
    SETUP_SERVER_PORT,
    SETUP_CONFIRM
};

static constexpr const char* APP_VERSION = "ask-master v1.2.0";
static constexpr int UDP_BEACON_PORT = 8766;
static constexpr unsigned long BEACON_INTERVAL_MS = 30000;
static constexpr unsigned long WS_IDLE_TIMEOUT_MS = 30000;
static constexpr unsigned long WAKE_TIMEOUT_MS = 10000;
static constexpr const char* BEACON_PACKET = "ask-master-ping";
static constexpr const char* WAKE_PACKET = "ask-master-wake";

State currentState = SLEEP;
SetupStep setupStep = SETUP_SCANNING;
bool inSetupMode = false;
bool pendingSetup = false;
ConfigManager configManager;
WSClient wsClient;
WiFiUDP udp;
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

String savedNetworkNames[6];
int savedNetworkCount = 0;
String savedServerIPs[6];
int savedServerPorts[6];
int savedServerCount = 0;

unsigned long lastBeaconTime = 0;
unsigned long wsConnectTime = 0;
unsigned long lastActivityTime = 0;
unsigned long lastIdleRedraw = 0;

int scrollOffset = 0;
int maxScrollOffset = 0;

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
void drawSettingsMenu();
void saveSetupAndConnect();
void updateMaxScroll();
void startNormalOperation();
void enterSetupMode();
void sendBeacon();
void checkUDPPackets();
void transitionToSleep();
void transitionToWake();

#ifdef DEBUG_SERIAL
  #define DBG(...) Serial.println(__VA_ARGS__)
#else
  #define DBG(...)
#endif

void setup() {
    #ifdef DEBUG_SERIAL
    Serial.begin(115200);
    delay(500);
    #endif

    DBG("=== ask-master boot ===");

    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);

    drawSleepScreen();

    configManager.begin();
    bool hasConfig = configManager.load();

    DBG("hasConfig: " + String(hasConfig));
    DBG("SSID: " + String(configManager.getWiFiSSID()));
    DBG("IP: " + String(configManager.getServerIP()));

    if (!hasConfig) {
        DBG("No config, entering setup");
        runSetupFlow();
    } else {
        DBG("Starting normal operation");
        WiFi.mode(WIFI_STA);
        WiFi.begin(configManager.getWiFiSSID(), configManager.getWiFiPassword());
        udp.begin(UDP_BEACON_PORT);
        lastBeaconTime = millis();
        drawSleepScreen();
    }
}

void loop() {
    M5Cardputer.update();

    if (pendingSetup) {
        pendingSetup = false;
        enterSetupMode();
        return;
    }

    if (inSetupMode) {
        handleKeyboard();
        return;
    }

    checkUDPPackets();

    if (currentState == SLEEP) {
        if (WiFi.status() != WL_CONNECTED) {
            drawConnectingScreen();
            delay(500);
            return;
        }

        if (millis() - lastBeaconTime > BEACON_INTERVAL_MS) {
            lastBeaconTime = millis();
            sendBeacon();
        }

        handleKeyboard();
        return;
    }

    if (currentState == WAKE || currentState == CONNECTING || currentState == IDLE) {
        wsClient.loop();

        if (currentState == IDLE) {
            if (millis() - lastActivityTime > WS_IDLE_TIMEOUT_MS) {
                DBG("WS idle timeout, sleeping");
                transitionToSleep();
                return;
            }
            if (millis() - lastIdleRedraw > 100) {
                lastIdleRedraw = millis();
                drawIdle();
            }
        }

        if (currentState == WAKE && millis() - wsConnectTime > WAKE_TIMEOUT_MS) {
            DBG("Wake timeout, sleeping");
            transitionToSleep();
            return;
        }
    }

    handleKeyboard();
}

void transitionToSleep() {
    wsClient.disconnect();
    currentState = SLEEP;
    lastBeaconTime = millis();
    drawSleepScreen();
}

void transitionToWake() {
    currentState = WAKE;
    wsConnectTime = millis();
    lastActivityTime = millis();
    drawConnectingScreen();

    wsClient.disconnect();
    wsClient.begin(configManager.getServerIP(), configManager.getServerPort());
    wsClient.onMessage(onWSMessage);
    wsClient.onConnect(onWSConnect);
    wsClient.onDisconnect(onWSDisconnect);
}

void sendBeacon() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    IPAddress serverIP;
    serverIP.fromString(configManager.getServerIP());
    if (serverIP == IPAddress(0, 0, 0, 0)) {
        return;
    }

    udp.beginPacket(serverIP, UDP_BEACON_PORT);
    udp.write((const uint8_t*)BEACON_PACKET, strlen(BEACON_PACKET));
    udp.endPacket();
    DBG("Beacon sent to " + String(configManager.getServerIP()));
}

void checkUDPPackets() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    int packetSize = udp.parsePacket();
    if (packetSize == 0) {
        return;
    }

    char buffer[64];
    int len = udp.read(buffer, sizeof(buffer) - 1);
    if (len > 0) {
        buffer[len] = '\0';
        String msg = String(buffer);
        msg.trim();
        DBG("UDP received: " + msg);

        if (msg == WAKE_PACKET && currentState == SLEEP) {
            DBG("Wake packet received, connecting WS");
            transitionToWake();
        }
    }
}

void enterSetupMode() {
    DBG("Entering setup mode");
    wsClient.disconnect();
    WiFi.disconnect();
    udp.stop();
    runSetupFlow();
}

void scanNetworks() {
    networkScanComplete = false;
    scannedNetworkCount = 0;
    drawSetupScreen("Scanning...", "Looking for WiFi networks", "");
    DBG("Scanning WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    DBG("Found " + String(n) + " networks");
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
                DBG("  " + String(i) + ": " + ssid + " (" + String(WiFi.RSSI(i)) + " dBm)");
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
    setupStep = SETUP_MENU;
    inputBuffer[0] = '\0';
    inputLength = 0;
    drawSettingsMenu();
}

void saveSetupAndConnect() {
    if (!configManager.save()) {
        // NVS write failed — show a transient error stripe and bail out of
        // the setup-complete flow so the user knows their config did not
        // persist and can retry instead of seeing a working device that
        // forgets everything on the next reboot.
        drawSetupScreen("NVS save failed", "config NOT persisted - retry or factory reset", "");
        delay(2500);
        setupStep = SETUP_CONFIRM;
        return;
    }
    inSetupMode = false;
    currentState = SLEEP;
    WiFi.mode(WIFI_STA);
    WiFi.begin(configManager.getWiFiSSID(), configManager.getWiFiPassword());
    udp.begin(UDP_BEACON_PORT);
    lastBeaconTime = millis();
    drawSleepScreen();
}

void onWSMessage(const String& message) {
    if (currentState == CONFIGURING || currentState == SENDING || currentState == RENDERING || currentState == WAITING_INPUT) {
        return;
    }

    if (currentState == WAKE || currentState == CONNECTING) {
        currentState = IDLE;
        lastActivityTime = millis();
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
    scrollOffset = 0;
    maxScrollOffset = 0;

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
    updateMaxScroll();
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
    lastActivityTime = millis();
}

void onWSConnect() {
    DBG("WebSocket CONNECTED");
    currentState = IDLE;
    lastActivityTime = millis();
    drawIdle();
}

void onWSDisconnect() {
    DBG("WebSocket DISCONNECTED");
    if (currentState != CONFIGURING) {
        transitionToSleep();
    }
}

void renderCurrentScreen() {
    if (currentType == "ask") {
        drawAskScreen(currentQuestion.c_str(), currentContext.c_str(), inputBuffer, scrollOffset);
    } else if (currentType == "escalate") {
        drawEscalateScreen(currentQuestion.c_str(), currentContext.c_str(), inputBuffer, scrollOffset);
    } else if (currentType == "confirm") {
        drawConfirmScreen(currentQuestion.c_str(), currentContext.c_str(), scrollOffset);
    } else if (currentType == "choose") {
        drawChooseScreen(currentQuestion.c_str(), currentContext.c_str(), currentOptions, currentOptionCount, scrollOffset);
    }
}

void handleKeyboard() {
    if (inSetupMode) {
        handleSetupKeyboard();
        return;
    }

    if (currentState == SLEEP) {
        if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
            return;
        }
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        for (char c : status.word) {
            DBG("Key: " + String(c));
            if (c == 's' || c == 'S') {
                DBG("S pressed, entering setup");
                pendingSetup = true;
                return;
            }
            if (c == ' ' || c == 'w' || c == 'W') {
                DBG("Wake key pressed, connecting WS");
                transitionToWake();
                return;
            }
        }
        return;
    }

    if (currentState == IDLE || currentState == CONNECTING || currentState == WAKE) {
        if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
            return;
        }
        Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();
        for (char c : status.word) {
            if (c == 's' || c == 'S') {
                DBG("S pressed, entering setup");
                pendingSetup = true;
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

    // Arrow-key scroll. `;` and `.` are the physical up/down arrow keys on
    // Cardputer ADV. Always consume these chars (never fall through to input)
    // so the user can scroll long questions without typing the char.
    bool consumeAsScroll = false;
    for (char c : status.word) {
        if (c == ';' || c == 'u' || c == 'U') {
            consumeAsScroll = true;
            if (scrollOffset > 0) {
                scrollOffset -= 16;
                if (scrollOffset < 0) scrollOffset = 0;
            }
        } else if (c == '.' || c == 'd' || c == 'D') {
            consumeAsScroll = true;
            if (scrollOffset < maxScrollOffset) {
                scrollOffset += 16;
                if (scrollOffset > maxScrollOffset) scrollOffset = maxScrollOffset;
            }
        }
    }
    if (consumeAsScroll) {
        renderCurrentScreen();
        return;
    }

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
            drawAskScreen(currentQuestion.c_str(), currentContext.c_str(), inputBuffer, scrollOffset);
        } else {
            drawEscalateScreen(currentQuestion.c_str(), currentContext.c_str(), inputBuffer, scrollOffset);
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

    if (setupStep == SETUP_MENU) {
        for (char c : status.word) {
            if (c == '1') {
                savedNetworkCount = 0;
                WiFiProfile prof;
                for (int i = 0; i < MAX_WIFI_PROFILES && savedNetworkCount < 6; i++) {
                    if (configManager.getWiFiProfile(i, prof)) {
                        savedNetworkNames[savedNetworkCount] = prof.ssid;
                        scannedRSSI[savedNetworkCount] = 0;
                        savedNetworkCount++;
                    }
                }
                if (savedNetworkCount > 0) {
                    setupStep = SETUP_WIFI_SELECT;
                    drawWiFiSelectScreen(savedNetworkNames, savedNetworkCount, nullptr, true);
                } else {
                    scanNetworks();
                }
                return;
            }
            if (c == '2') {
                savedServerCount = 0;
                ServerProfile prof;
                for (int i = 0; i < MAX_SERVER_PROFILES && savedServerCount < 6; i++) {
                    if (configManager.getServerProfile(i, prof)) {
                        savedServerIPs[savedServerCount] = prof.ip;
                        savedServerPorts[savedServerCount] = prof.port;
                        savedServerCount++;
                    }
                }
                if (savedServerCount > 0) {
                    setupStep = SETUP_SERVER_SELECT;
                    const char* ipPtrs[6];
                    for (int i = 0; i < savedServerCount; i++) {
                        ipPtrs[i] = savedServerIPs[i].c_str();
                    }
                    drawServerSelectScreen(ipPtrs, savedServerPorts, savedServerCount);
                } else {
                    setupStep = SETUP_SERVER_IP;
                    inputBuffer[0] = '\0';
                    inputLength = 0;
                    if (configManager.getServerIP()[0] != '\0') {
                        strcpy(inputBuffer, configManager.getServerIP());
                        inputLength = strlen(inputBuffer);
                    }
                    drawSetupScreen("Server IP Address", "Your computer's IP (e.g. 192.168.1.5)", inputBuffer);
                }
                return;
            }
            if (c == '3') {
                configManager.clear();
                scanNetworks();
                return;
            }
        }
        return;
    }

    if (setupStep == SETUP_WIFI_SELECT) {
        for (char c : status.word) {
            if (c == 'n' || c == 'N') {
                scanNetworks();
                return;
            }
            if (c >= '1' && c <= '6') {
                int choice = c - '0';
                if (choice <= savedNetworkCount) {
                    WiFiProfile prof;
                    for (int i = 0, idx = 0; i < MAX_WIFI_PROFILES; i++) {
                        if (configManager.getWiFiProfile(i, prof)) {
                            if (idx == choice - 1) {
                                configManager.setWiFiSSID(prof.ssid);
                                configManager.setWiFiPassword(prof.password);
                                configManager.setActiveWiFiProfile(i);
                                break;
                            }
                            idx++;
                        }
                    }
                    setupStep = SETUP_SERVER_IP;
                    inputBuffer[0] = '\0';
                    inputLength = 0;
                    if (configManager.getServerIP()[0] != '\0') {
                        strcpy(inputBuffer, configManager.getServerIP());
                        inputLength = strlen(inputBuffer);
                    }
                    drawSetupScreen("Server IP Address", "Your computer's IP (e.g. 192.168.1.5)", inputBuffer);
                    return;
                }
            }
        }
        return;
    }

    if (setupStep == SETUP_SERVER_SELECT) {
        for (char c : status.word) {
            if (c == 'n' || c == 'N') {
                setupStep = SETUP_SERVER_IP;
                inputBuffer[0] = '\0';
                inputLength = 0;
                if (configManager.getServerIP()[0] != '\0') {
                    strcpy(inputBuffer, configManager.getServerIP());
                    inputLength = strlen(inputBuffer);
                }
                drawSetupScreen("Server IP Address", "Your computer's IP (e.g. 192.168.1.5)", inputBuffer);
                return;
            }
            if (c >= '1' && c <= '6') {
                int choice = c - '0';
                if (choice <= savedServerCount) {
                    ServerProfile prof;
                    for (int i = 0, idx = 0; i < MAX_SERVER_PROFILES; i++) {
                        if (configManager.getServerProfile(i, prof)) {
                            if (idx == choice - 1) {
                                configManager.setServerIP(prof.ip);
                                configManager.setServerPort(prof.port);
                                configManager.setActiveServerProfile(i);
                                break;
                            }
                            idx++;
                        }
                    }
                    setupStep = SETUP_CONFIRM;
                    drawSetupSummaryScreen(
                        configManager.getWiFiSSID(),
                        configManager.getServerIP(),
                        configManager.getServerPort()
                    );
                    return;
                }
            }
        }
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
                    WiFiProfile prof;
                    bool hasPassword = false;
                    for (int i = 0; i < MAX_WIFI_PROFILES; i++) {
                        if (configManager.getWiFiProfile(i, prof) && strcmp(prof.ssid, scannedNetworks[choice - 1].c_str()) == 0) {
                            strncpy(inputBuffer, prof.password, sizeof(inputBuffer) - 1);
                            inputBuffer[sizeof(inputBuffer) - 1] = '\0';
                            inputLength = strlen(inputBuffer);
                            hasPassword = true;
                            break;
                        }
                    }
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
                configManager.addWiFiProfile(configManager.getWiFiSSID(), configManager.getWiFiPassword());
                configManager.addServerProfile(configManager.getServerIP(), configManager.getServerPort());
                saveSetupAndConnect();
                return;
            }
            if (c == 'n' || c == 'N') {
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
                if (configManager.getServerIP()[0] != '\0') {
                    strcpy(inputBuffer, configManager.getServerIP());
                    inputLength = strlen(inputBuffer);
                }
                drawSetupScreen("Server IP Address", "Your computer's IP (e.g. 192.168.1.5)", inputBuffer);
                break;
            case SETUP_SERVER_IP:
                configManager.setServerIP(inputBuffer);
                setupStep = SETUP_SERVER_PORT;
                inputBuffer[0] = '\0';
                inputLength = 0;
                if (configManager.getServerPort() != 0) {
                    snprintf(inputBuffer, sizeof(inputBuffer), "%d", configManager.getServerPort());
                    inputLength = strlen(inputBuffer);
                } else {
                    strcpy(inputBuffer, "8765");
                    inputLength = 4;
                }
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
    delay(500);
    transitionToSleep();
}

void clearCurrentPrompt() {
    inputBuffer[0] = '\0';
    inputLength = 0;
    currentQuestion = "";
    currentContext = "";
    currentOptionCount = 0;
    currentType = "";
    scrollOffset = 0;
    maxScrollOffset = 0;

    for (int i = 0; i < 6; ++i) {
        currentOptions[i] = "";
    }
}

void drawConnectingScreen() {
    drawIdleScreen("Connecting...", "Waiting for agent...", true);
}

void drawIdle() {
    drawIdleScreen(APP_VERSION, WiFi.localIP().toString().c_str(), true);
}

void drawSettingsMenu() {
    char serverInfo[32];
    snprintf(serverInfo, sizeof(serverInfo), "%s:%d",
        configManager.getServerIP()[0] != '\0' ? configManager.getServerIP() : "-",
        configManager.getServerPort());
    drawSettingsMenuScreen(
        configManager.isConfigured(),
        configManager.getWiFiSSID()[0] != '\0' ? configManager.getWiFiSSID() : nullptr,
        serverInfo
    );
}

void updateMaxScroll() {
    maxScrollOffset = computeMaxScroll(currentType.c_str(),
                                       currentQuestion.c_str(),
                                       currentContext.c_str(),
                                       currentOptions, currentOptionCount);
}
