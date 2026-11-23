#pragma once

#include <Arduino.h>
#include "platform_compat.h"
#include <PCA9685.h>
#include <Ticker.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <array>
#include <map>
#include <string>

#include "HeatingSystemElements/HeatingSystem.h"

constexpr size_t STATE_JSON_DOCUMENT_SIZE = 2048;
constexpr size_t STATE_JSON_RESPONSE_SIZE = 1024;
// A PCA9685 16 csatornát kezel: 0..15. A konfigurációban használt
// 1..15 tartományt ezért az alkalmazásnak is teljesen támogatnia kell.
constexpr size_t MAX_IO_ID = 15;

extern HeatingSystem *hs;
extern Ticker timer;
extern ESP8266WebServer server;
extern WebSocketsServer webSocket;
extern const char *websocketPassword;
extern bool isAuthenticated;
extern bool isDataNew;
extern int cntrWifiTry;
extern unsigned long lastMemoryDiagnosticMs;
extern volatile bool heatingSystemUpdatePending;
extern PCA9685 pwmController;
extern std::map<int, int> IOMap;
extern std::array<bool, MAX_IO_ID + 1> manualIOOverrides;
extern StaticJsonDocument<STATE_JSON_DOCUMENT_SIZE> stateJsonDocument;
extern String stateJsonResponse;
