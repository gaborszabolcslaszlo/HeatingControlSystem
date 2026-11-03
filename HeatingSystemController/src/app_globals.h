#pragma once

#include <Arduino.h>
#include <ESP8266WebServer.h>
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
constexpr size_t MAX_IO_ID = 8;

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
