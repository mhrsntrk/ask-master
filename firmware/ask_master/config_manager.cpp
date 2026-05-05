#include "config_manager.h"

ConfigManager::ConfigManager() 
    : _configured(false), _serverPort(8765) {
    _ssid[0] = '\0';
    _password[0] = '\0';
    _serverIP[0] = '\0';
}

bool ConfigManager::begin() {
    return prefs.begin("ask-master", false);
}

bool ConfigManager::load() {
    _configured = prefs.getBool("configured", false);
    if (_configured) {
        prefs.getString("ssid", _ssid, sizeof(_ssid));
        prefs.getString("password", _password, sizeof(_password));
        prefs.getString("serverIP", _serverIP, sizeof(_serverIP));
        _serverPort = prefs.getUShort("serverPort", 8765);
        return true;
    }
    return false;
}

void ConfigManager::save() {
    prefs.putBool("configured", true);
    prefs.putString("ssid", _ssid);
    prefs.putString("password", _password);
    prefs.putString("serverIP", _serverIP);
    prefs.putUShort("serverPort", _serverPort);
    _configured = true;
}

bool ConfigManager::isConfigured() const {
    return _configured;
}

void ConfigManager::clear() {
    prefs.clear();
    _configured = false;
    _ssid[0] = '\0';
    _password[0] = '\0';
    _serverIP[0] = '\0';
    _serverPort = 8765;
}

void ConfigManager::setWiFiSSID(const char* ssid) {
    strncpy(_ssid, ssid, sizeof(_ssid) - 1);
    _ssid[sizeof(_ssid) - 1] = '\0';
}

void ConfigManager::setWiFiPassword(const char* password) {
    strncpy(_password, password, sizeof(_password) - 1);
    _password[sizeof(_password) - 1] = '\0';
}

void ConfigManager::setServerIP(const char* ip) {
    strncpy(_serverIP, ip, sizeof(_serverIP) - 1);
    _serverIP[sizeof(_serverIP) - 1] = '\0';
}

void ConfigManager::setServerPort(uint16_t port) {
    _serverPort = port;
}

const char* ConfigManager::getWiFiSSID() const {
    return _ssid;
}

const char* ConfigManager::getWiFiPassword() const {
    return _password;
}

const char* ConfigManager::getServerIP() const {
    return _serverIP;
}

uint16_t ConfigManager::getServerPort() const {
    return _serverPort;
}
