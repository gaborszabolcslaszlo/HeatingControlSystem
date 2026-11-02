#include "app_globals.h"
#include "HeatingSystemElements/Sensor.h"

HeatingSystem *hs = nullptr;
Ticker timer;
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);
const char *websocketPassword = "secretpassword";
bool isAuthenticated = false;
bool isDataNew = false;
int cntrWifiTry = 0;
unsigned long lastMemoryDiagnosticMs = 0;
volatile bool heatingSystemUpdatePending = false;
PCA9685 pwmController;
std::map<int, int> IOMap;
std::map<std::string, float> Sensor::SensorsValue;
StaticJsonDocument<STATE_JSON_DOCUMENT_SIZE> stateJsonDocument;
String stateJsonResponse;
