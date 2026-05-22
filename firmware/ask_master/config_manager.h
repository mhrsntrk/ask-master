#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Preferences.h>

#define MAX_WIFI_PROFILES 3
#define MAX_SERVER_PROFILES 3

struct WiFiProfile {
    char ssid[33];
    char password[65];
    bool active;
};

struct ServerProfile {
    char ip[16];
    uint16_t port;
    bool active;
};

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

    int getWiFiProfileCount() const;
    int getServerProfileCount() const;
    bool getWiFiProfile(int index, WiFiProfile& profile) const;
    bool getServerProfile(int index, ServerProfile& profile) const;
    bool addWiFiProfile(const char* ssid, const char* password);
    bool addServerProfile(const char* ip, uint16_t port);
    bool removeWiFiProfile(int index);
    bool removeServerProfile(int index);
    void setActiveWiFiProfile(int index);
    void setActiveServerProfile(int index);
    int getActiveWiFiProfile() const;
    int getActiveServerProfile() const;
    bool hasWiFiProfile(const char* ssid) const;
    bool hasServerProfile(const char* ip, uint16_t port) const;

private:
    Preferences prefs;
    bool _configured;
    char _ssid[33];
    char _password[65];
    char _serverIP[16];
    uint16_t _serverPort;
    
    WiFiProfile _wifiProfiles[MAX_WIFI_PROFILES];
    ServerProfile _serverProfiles[MAX_SERVER_PROFILES];
    int _activeWiFiProfile;
    int _activeServerProfile;
    
    void loadWiFiProfiles();
    void loadServerProfiles();
    void saveWiFiProfiles();
    void saveServerProfiles();
};

#endif // CONFIG_MANAGER_H
