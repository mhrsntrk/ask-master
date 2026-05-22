#include "config_manager.h"

ConfigManager::ConfigManager() 
    : _configured(false), _serverPort(8765), _activeWiFiProfile(0), _activeServerProfile(0) {
    _ssid[0] = '\0';
    _password[0] = '\0';
    _serverIP[0] = '\0';
    
    for (int i = 0; i < MAX_WIFI_PROFILES; i++) {
        _wifiProfiles[i].ssid[0] = '\0';
        _wifiProfiles[i].password[0] = '\0';
        _wifiProfiles[i].active = false;
    }
    
    for (int i = 0; i < MAX_SERVER_PROFILES; i++) {
        _serverProfiles[i].ip[0] = '\0';
        _serverProfiles[i].port = 8765;
        _serverProfiles[i].active = false;
    }
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
        
        _activeWiFiProfile = prefs.getInt("activeWiFi", 0);
        _activeServerProfile = prefs.getInt("activeServer", 0);
        
        loadWiFiProfiles();
        loadServerProfiles();
        
        if (_ssid[0] == '\0' || _serverIP[0] == '\0') {
            _configured = false;
            return false;
        }
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
    prefs.putInt("activeWiFi", _activeWiFiProfile);
    prefs.putInt("activeServer", _activeServerProfile);
    
    saveWiFiProfiles();
    saveServerProfiles();
    
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
    _activeWiFiProfile = 0;
    _activeServerProfile = 0;
    
    for (int i = 0; i < MAX_WIFI_PROFILES; i++) {
        _wifiProfiles[i].ssid[0] = '\0';
        _wifiProfiles[i].password[0] = '\0';
        _wifiProfiles[i].active = false;
    }
    
    for (int i = 0; i < MAX_SERVER_PROFILES; i++) {
        _serverProfiles[i].ip[0] = '\0';
        _serverProfiles[i].port = 8765;
        _serverProfiles[i].active = false;
    }
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

int ConfigManager::getWiFiProfileCount() const {
    int count = 0;
    for (int i = 0; i < MAX_WIFI_PROFILES; i++) {
        if (_wifiProfiles[i].active && _wifiProfiles[i].ssid[0] != '\0') {
            count++;
        }
    }
    return count;
}

int ConfigManager::getServerProfileCount() const {
    int count = 0;
    for (int i = 0; i < MAX_SERVER_PROFILES; i++) {
        if (_serverProfiles[i].active && _serverProfiles[i].ip[0] != '\0') {
            count++;
        }
    }
    return count;
}

bool ConfigManager::getWiFiProfile(int index, WiFiProfile& profile) const {
    if (index < 0 || index >= MAX_WIFI_PROFILES) return false;
    if (!_wifiProfiles[index].active) return false;
    profile = _wifiProfiles[index];
    return true;
}

bool ConfigManager::getServerProfile(int index, ServerProfile& profile) const {
    if (index < 0 || index >= MAX_SERVER_PROFILES) return false;
    if (!_serverProfiles[index].active) return false;
    profile = _serverProfiles[index];
    return true;
}

bool ConfigManager::addWiFiProfile(const char* ssid, const char* password) {
    if (!ssid || ssid[0] == '\0') return false;
    
    for (int i = 0; i < MAX_WIFI_PROFILES; i++) {
        if (_wifiProfiles[i].active && strcmp(_wifiProfiles[i].ssid, ssid) == 0) {
            strncpy(_wifiProfiles[i].password, password, sizeof(_wifiProfiles[i].password) - 1);
            _wifiProfiles[i].password[sizeof(_wifiProfiles[i].password) - 1] = '\0';
            return true;
        }
    }
    
    for (int i = 0; i < MAX_WIFI_PROFILES; i++) {
        if (!_wifiProfiles[i].active) {
            strncpy(_wifiProfiles[i].ssid, ssid, sizeof(_wifiProfiles[i].ssid) - 1);
            _wifiProfiles[i].ssid[sizeof(_wifiProfiles[i].ssid) - 1] = '\0';
            strncpy(_wifiProfiles[i].password, password, sizeof(_wifiProfiles[i].password) - 1);
            _wifiProfiles[i].password[sizeof(_wifiProfiles[i].password) - 1] = '\0';
            _wifiProfiles[i].active = true;
            return true;
        }
    }
    return false;
}

bool ConfigManager::addServerProfile(const char* ip, uint16_t port) {
    if (!ip || ip[0] == '\0') return false;
    
    for (int i = 0; i < MAX_SERVER_PROFILES; i++) {
        if (_serverProfiles[i].active && strcmp(_serverProfiles[i].ip, ip) == 0 && _serverProfiles[i].port == port) {
            return true;
        }
    }
    
    for (int i = 0; i < MAX_SERVER_PROFILES; i++) {
        if (!_serverProfiles[i].active) {
            strncpy(_serverProfiles[i].ip, ip, sizeof(_serverProfiles[i].ip) - 1);
            _serverProfiles[i].ip[sizeof(_serverProfiles[i].ip) - 1] = '\0';
            _serverProfiles[i].port = port;
            _serverProfiles[i].active = true;
            return true;
        }
    }
    return false;
}

bool ConfigManager::removeWiFiProfile(int index) {
    if (index < 0 || index >= MAX_WIFI_PROFILES) return false;
    _wifiProfiles[index].ssid[0] = '\0';
    _wifiProfiles[index].password[0] = '\0';
    _wifiProfiles[index].active = false;
    if (_activeWiFiProfile == index) {
        _activeWiFiProfile = 0;
    }
    return true;
}

bool ConfigManager::removeServerProfile(int index) {
    if (index < 0 || index >= MAX_SERVER_PROFILES) return false;
    _serverProfiles[index].ip[0] = '\0';
    _serverProfiles[index].port = 8765;
    _serverProfiles[index].active = false;
    if (_activeServerProfile == index) {
        _activeServerProfile = 0;
    }
    return true;
}

void ConfigManager::setActiveWiFiProfile(int index) {
    if (index >= 0 && index < MAX_WIFI_PROFILES && _wifiProfiles[index].active) {
        _activeWiFiProfile = index;
        strncpy(_ssid, _wifiProfiles[index].ssid, sizeof(_ssid) - 1);
        _ssid[sizeof(_ssid) - 1] = '\0';
        strncpy(_password, _wifiProfiles[index].password, sizeof(_password) - 1);
        _password[sizeof(_password) - 1] = '\0';
    }
}

void ConfigManager::setActiveServerProfile(int index) {
    if (index >= 0 && index < MAX_SERVER_PROFILES && _serverProfiles[index].active) {
        _activeServerProfile = index;
        strncpy(_serverIP, _serverProfiles[index].ip, sizeof(_serverIP) - 1);
        _serverIP[sizeof(_serverIP) - 1] = '\0';
        _serverPort = _serverProfiles[index].port;
    }
}

int ConfigManager::getActiveWiFiProfile() const {
    return _activeWiFiProfile;
}

int ConfigManager::getActiveServerProfile() const {
    return _activeServerProfile;
}

bool ConfigManager::hasWiFiProfile(const char* ssid) const {
    if (!ssid || ssid[0] == '\0') return false;
    for (int i = 0; i < MAX_WIFI_PROFILES; i++) {
        if (_wifiProfiles[i].active && strcmp(_wifiProfiles[i].ssid, ssid) == 0) {
            return true;
        }
    }
    return false;
}

bool ConfigManager::hasServerProfile(const char* ip, uint16_t port) const {
    if (!ip || ip[0] == '\0') return false;
    for (int i = 0; i < MAX_SERVER_PROFILES; i++) {
        if (_serverProfiles[i].active && strcmp(_serverProfiles[i].ip, ip) == 0 && _serverProfiles[i].port == port) {
            return true;
        }
    }
    return false;
}

void ConfigManager::loadWiFiProfiles() {
    for (int i = 0; i < MAX_WIFI_PROFILES; i++) {
        char keySsid[16];
        char keyPass[20];
        snprintf(keySsid, sizeof(keySsid), "wf_ssid_%d", i);
        snprintf(keyPass, sizeof(keyPass), "wf_pass_%d", i);
        
        _wifiProfiles[i].active = prefs.getBool(keySsid, false);
        if (_wifiProfiles[i].active) {
            prefs.getString(keySsid, _wifiProfiles[i].ssid, sizeof(_wifiProfiles[i].ssid));
            prefs.getString(keyPass, _wifiProfiles[i].password, sizeof(_wifiProfiles[i].password));
        }
    }
}

void ConfigManager::loadServerProfiles() {
    for (int i = 0; i < MAX_SERVER_PROFILES; i++) {
        char keyIp[16];
        char keyPort[20];
        snprintf(keyIp, sizeof(keyIp), "srv_ip_%d", i);
        snprintf(keyPort, sizeof(keyPort), "srv_port_%d", i);
        
        _serverProfiles[i].active = prefs.getBool(keyIp, false);
        if (_serverProfiles[i].active) {
            prefs.getString(keyIp, _serverProfiles[i].ip, sizeof(_serverProfiles[i].ip));
            _serverProfiles[i].port = prefs.getUShort(keyPort, 8765);
        }
    }
}

void ConfigManager::saveWiFiProfiles() {
    for (int i = 0; i < MAX_WIFI_PROFILES; i++) {
        char keySsid[16];
        char keyPass[20];
        snprintf(keySsid, sizeof(keySsid), "wf_ssid_%d", i);
        snprintf(keyPass, sizeof(keyPass), "wf_pass_%d", i);
        
        prefs.putBool(keySsid, _wifiProfiles[i].active);
        if (_wifiProfiles[i].active) {
            prefs.putString(keySsid, _wifiProfiles[i].ssid);
            prefs.putString(keyPass, _wifiProfiles[i].password);
        }
    }
}

void ConfigManager::saveServerProfiles() {
    for (int i = 0; i < MAX_SERVER_PROFILES; i++) {
        char keyIp[16];
        char keyPort[20];
        snprintf(keyIp, sizeof(keyIp), "srv_ip_%d", i);
        snprintf(keyPort, sizeof(keyPort), "srv_port_%d", i);
        
        prefs.putBool(keyIp, _serverProfiles[i].active);
        if (_serverProfiles[i].active) {
            prefs.putString(keyIp, _serverProfiles[i].ip);
            prefs.putUShort(keyPort, _serverProfiles[i].port);
        }
    }
}
