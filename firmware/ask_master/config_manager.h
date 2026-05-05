#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Preferences.h>

class ConfigManager {
public:
    ConfigManager();
    bool begin();
    bool load();
    void save();
    bool isConfigured() const;
    void clear();

    void setWiFiSSID(const char* ssid);
    void setWiFiPassword(const char* password);
    void setServerIP(const char* ip);
    void setServerPort(uint16_t port);

    const char* getWiFiSSID() const;
    const char* getWiFiPassword() const;
    const char* getServerIP() const;
    uint16_t getServerPort() const;

private:
    Preferences prefs;
    bool _configured;
    char _ssid[33];
    char _password[65];
    char _serverIP[16];
    uint16_t _serverPort;
};

#endif // CONFIG_MANAGER_H
