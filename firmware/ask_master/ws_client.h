#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <WiFi.h>
#include <WebSocketsClient.h>
#include "config.h"

class WSClient {
public:
    WSClient() : _onMessage(nullptr), _onConnect(nullptr), _onDisconnect(nullptr) {}

    void begin(const char* host, uint16_t port, uint16_t reconnectInterval = 3000) {
        _webSocket.begin(host, port, "/");
        _webSocket.onEvent([this](WStype_t type, uint8_t* payload, size_t length) {
            this->webSocketEvent(type, payload, length);
        });
        _webSocket.setReconnectInterval(reconnectInterval);
    }

    void loop() {
        _webSocket.loop();
    }

    void send(const String& message) {
        _webSocket.sendTXT(message.c_str(), message.length());
    }

    bool isConnected() {
        return _webSocket.isConnected();
    }

    void onMessage(void (*callback)(const String&)) {
        _onMessage = callback;
    }

    void onConnect(void (*callback)()) {
        _onConnect = callback;
    }

    void onDisconnect(void (*callback)()) {
        _onDisconnect = callback;
    }

private:
    WebSocketsClient _webSocket;
    void (*_onMessage)(const String&);
    void (*_onConnect)();
    void (*_onDisconnect)();

    void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
        switch (type) {
            case WStype_DISCONNECTED:
                if (_onDisconnect) _onDisconnect();
                break;
            case WStype_CONNECTED:
                if (_onConnect) _onConnect();
                break;
            case WStype_TEXT:
                if (_onMessage) {
                    String message = String((char*)payload);
                    _onMessage(message);
                }
                break;
            default:
                break;
        }
    }
};

#endif // WS_CLIENT_H
