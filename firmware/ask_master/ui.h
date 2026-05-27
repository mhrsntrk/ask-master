#ifndef UI_H
#define UI_H

#include <Arduino.h>

void drawIdleScreen(const char* version, const char* ip, bool showSetupHint);
void drawSetupScreen(const char* label, const char* context, const char* inputBuffer);
void drawSetupSummaryScreen(const char* ssid, const char* serverIP, uint16_t port);
void drawNetworkListScreen(const String networks[], int networkCount, int8_t rssi[]);
int measureWordWrappedHeight(const char* text, int x, int maxWidth);
// Returns max valid scrollY for the given screen type so callers don't
// scroll past the last visible line.
int computeMaxScroll(const char* type, const char* question, const char* context,
                      const String options[], int optionCount);
void drawSettingsMenuScreen(bool hasConfig, const char* currentSSID, const char* currentServer);
void drawWiFiSelectScreen(const String networks[], int networkCount, int8_t rssi[], bool showSaved);
void drawServerSelectScreen(const char* ips[], int ports[], int count);
void drawAskScreen(const char* question, const char* context, const char* inputBuffer, int scrollY);
void drawEscalateScreen(const char* question, const char* context, const char* inputBuffer, int scrollY);
void drawConfirmScreen(const char* statement, const char* consequence, int scrollY);
void drawChooseScreen(const char* question, const char* context, const String options[], int optionCount, int scrollY);
void drawSleepScreen();

#endif // UI_H
