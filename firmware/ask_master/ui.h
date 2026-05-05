#ifndef UI_H
#define UI_H

#include <Arduino.h>

void drawIdleScreen(const char* version, const char* ip, bool showSetupHint);
void drawSetupScreen(const char* label, const char* context, const char* inputBuffer);
void drawSetupSummaryScreen(const char* ssid, const char* serverIP, uint16_t port);
void drawAskScreen(const char* question, const char* context, const char* inputBuffer);
void drawEscalateScreen(const char* question, const char* context, const char* inputBuffer);
void drawConfirmScreen(const char* statement, const char* consequence);
void drawChooseScreen(const char* question, const char* context, const String options[], int optionCount);

#endif // UI_H
