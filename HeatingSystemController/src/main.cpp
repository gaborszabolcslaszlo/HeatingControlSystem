#include "platform_compat.h"
#include <exception>

#include <PCA9685.h>

#include "certificates.h"
#include "app_globals.h"
#include "config.h"
#include "web_api.h"
#include "web_ui.h"
#include "hardware_io.h"

#ifndef ESP32
#include <GDBStub.h>
#endif
#include <WebSocketsServer.h>
#include "temp_sensors.h"
#include "HeatingSystemElements/HeatingSystem.h"
#include <Ticker.h>
#include <FS.h>
#ifdef ESP32
#include <esp_heap_caps.h>
#endif
#include "esp_routes.h"

const int max_attempts = 20;
int attempt = 0;

// WebSocket esemény kezelő
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  (void)length;
  switch (type)
  {
  case WStype_DISCONNECTED:
    logDebug("[%u] Disconnected!\n", num);
    isAuthenticated = false;
    break;

  case WStype_CONNECTED:
  {
    IPAddress ip = webSocket.remoteIP(num);
    logDebug("[%u] Connected from %s\n", num, ip.toString().c_str());
  }
  break;

  case WStype_TEXT:
    logDebug("[%u] Text: %s\n", num, payload);

    // Az első üzenet a jelszó hitelesítésére szolgál
    if (!isAuthenticated)
    {
      if (strcmp((const char *)payload, websocketPassword) > 0)
      {
        logInfo("Hitelesítés sikeres!\n");
        isAuthenticated = true;
        webSocket.sendTXT(num, "Authentication successful!");
      }
      else
      {
        logWarn("Hitelesítés sikertelen!\n");
        webSocket.sendTXT(num, "Authentication failed!");
        webSocket.disconnect(num); // Ha a hitelesítés sikertelen, bontjuk a kapcsolatot
      }
    }
    else
    {
      // Ha már hitelesítettük a klienst, itt kezelhetjük az üzeneteket
      logDebug("Authenticated data received\n");
    }
    break;
  }
}

void onTimerISR()
{
  // ISR-ben nincs Serial, logMessage vagy állapotgép-frissítés.
  // Ezek a műveletek a loop()-ban, az updatePending ellenőrzése után futnak.
  heatingSystemUpdatePending = true;

  hs->update();
}

void printMemoryDiagnostics(const char *source)
{
#ifdef ESP32
  const size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  const size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  const unsigned int fragmentation =
      freeHeap == 0 || largestBlock >= freeHeap
          ? 0
          : static_cast<unsigned int>(100U - (largestBlock * 100U / freeHeap));
  logInfo("[MEM] source=%s free=%uB fragmentation=%u%% maxBlock=%uB uptime=%lus\n",
          source,
          static_cast<unsigned int>(freeHeap),
          fragmentation,
          static_cast<unsigned int>(largestBlock),
          millis() / 1000UL);
#else
  logInfo("[MEM] source=%s free=%uB fragmentation=%u%% maxBlock=%uB uptime=%lus\n",
          source,
          static_cast<unsigned int>(ESP.getFreeHeap()),
          static_cast<unsigned int>(ESP.getHeapFragmentation()),
          static_cast<unsigned int>(ESP.getMaxFreeBlockSize()),
          millis() / 1000UL);
#endif
}

int val = 0;

void initializeSerialAndHardware()
{
  Serial.begin(115200);
  Serial.flush();
#ifdef ESP32
  logInfo("[BOOT] ESP32 indulás\n");
#else
  logInfo("[BOOT] reset=%s\n", ESP.getResetInfo().c_str());
#endif
  printMemoryDiagnostics("setup");

#ifndef TESTINGOVER_API
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  pwmController.setupSingleDevice(Wire, 0x41);
  pwmController.setToServoFrequency();
  pwmController.setAllDevicesOutputsInverted();
  pwmController.setupOutputEnablePin(D0);
  pwmController.setOutputsHighWhenDisabled();
  pwmController.disableOutputs(D0);
#else
  logInfo("[TEST] PCA9685 inicializalas kihagyva; API teszt mod\n");
#endif

  for (int i = 1; i <= 8; i++)
  {
    IOMap[i] = 0;
  }

  delay(500);
  logInfo("Start\n");
}

void initializeHeatingSystem()
{
  loadConfig();
  hs = new HeatingSystem("/config.json");
}

void initializeHeatingSensors()
{
  hs->printHeatingSystem();
  setupSensors();
}

void configureWifiMode()
{
#ifdef ESP32
  WiFi.setHostname(config.hostname.c_str());
#else
  WiFi.hostname(config.hostname);
#endif

  if (config.wifiMode == "STA+AP")
  {
    WiFi.mode(WIFI_AP_STA);
  }
  else if (config.wifiMode == "STA")
  {
    WiFi.mode(WIFI_STA);
  }
  else if (config.wifiMode == "AP")
  {
    WiFi.mode(WIFI_AP);
  }
}

void connectToWifiStation()
{
  if (config.wifiMode != "STA+AP" && config.wifiMode != "STA")
  {
    return;
  }

  WiFi.begin(config.wifiSSID, config.wifiPassword);
  logInfo("WiFi csatlakozás: %s\n", config.wifiSSID.c_str());
  while (WiFi.status() != WL_CONNECTED && attempt < max_attempts)
  {
    delay(500);
    logInfo("WiFi állapot: %d (%d/%d)\n",
            static_cast<int>(WiFi.status()), attempt + 1, max_attempts);
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    logInfo("WiFi csatlakozva, IP: %s\n", WiFi.localIP().toString().c_str());
  }
  else
  {
    logError("WiFi csatlakozás sikertelen.\n");
#ifdef ESP32
    WiFi.mode(WIFI_AP_STA);
    if (WiFi.softAP(config.APSSID, config.APPassword))
    {
      logInfo("AP fallback indítva, IP: %s\n", WiFi.softAPIP().toString().c_str());
    }
    else
    {
      logError("AP fallback indítása sikertelen.\n");
    }
#endif
  }
}

void startWifiAccessPoint()
{
  if (config.wifiMode != "STA+AP" && config.wifiMode != "AP")
  {
    return;
  }

  WiFi.softAP(config.APSSID, config.APPassword);
  logInfo("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void initializeNetwork()
{
  configureWifiMode();
  connectToWifiStation();
  startWifiAccessPoint();

  // Az NTP kliens UDP kapcsolatot használ. Az ESP32-n csak akkor indítjuk,
  // amikor a WiFi hálózati réteg már inicializálva van és van STA kapcsolat.
  if (WiFi.status() == WL_CONNECTED)
  {
    timeClient.begin();
    timeClient.update();
  }
}

void registerHttpRoutes()
{
#ifndef ESP32
  server.keepAlive(true);
#endif
  server.enableCORS(true);

  server.on("/configuration", HTTP_OPTIONS, []()
            {
              server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
              server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
              server.send(200); });

  server.on("/heatingRequest", HTTP_GET, handleHeatingRequest);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/configuration", HTTP_GET, handleConfiguration);
  server.on("/configuration", HTTP_POST, handleConfigFileUpload, handleFileUpload);
  server.on("/reboot", HTTP_GET, handleReboot);
  server.on("/state", HTTP_GET, hadleStateRequest);
  server.on("/setTemp", HTTP_POST, handleSetTemperatureTest);
  server.on("/deviceInfo", HTTP_GET, handleDeviceInfo);
  server.on("/manualControlPumps", HTTP_POST, handleManualMode);

  server.on("/", HTTP_GET, handleWebUserInterfaceRequest);
  server.on("/styles.css", HTTP_GET, handleWebUserInterfaceRequest);
  server.on("/polyfills.js", HTTP_GET, handleWebUserInterfaceRequest);
  server.on("/main.js", HTTP_GET, handleWebUserInterfaceRequest);
  setupWebRoutes(server);
  server.onNotFound(handleFile);
}

void initializeWebServer()
{
  registerHttpRoutes();
  server.begin();
  logInfo("HTTP szerver indítva.\n");
}

void initializeWebSocket()
{
  if (config.communicationTypeWebApp != "WS")
  {
    return;
  }

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void initializeRuntime()
{
  timer.attach(1, onTimerISR);
  pwmController.enableOutputs(D0);
#ifdef ESP32
  enableLoopWDT();
#else
  ESP.wdtEnable(5000);
#endif
}

void enableInitialHeatingDemand()
{
  for (auto &element : hs->mergedList)
  {
    if (element->name == "PadloKozos")
    {
      element->setNeedHeating(true);
    }
  }
}

void setup()
{
  initializeSerialAndHardware();
  initializeHeatingSystem();
  initializeNetwork();
  initializeWebServer();
  initializeHeatingSensors();
  initializeWebSocket();
  initializeRuntime();
  enableInitialHeatingDemand();
}

void reconnectWifiIfNeeded()
{
  if (config.wifiMode != "STA+AP" && config.wifiMode != "STA")
  {
    return;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    return;
  }

  if (cntrWifiTry == 60)
  {
    logInfo("Csatlakozás a WiFi-hez...\n");
    WiFi.begin(config.wifiSSID, config.wifiPassword);
  }
  cntrWifiTry++;
}

void handleHttpRequests()
{
  try
  {
    server.handleClient();
  }
  catch (const std::exception &e)
  {
    logError("Kivétel történt: %s\n", e.what());
  }
}

void updateSensorsAndReportDuration()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    timeClient.update();
  }
  unsigned long sensorUpdateStarted = millis();
  updateDsSensors();
  unsigned long sensorUpdateDuration = millis() - sensorUpdateStarted;

  if (sensorUpdateDuration > 500UL)
  {
    logWarn("updateDsSensors took %lums\n", sensorUpdateDuration);
  }
}

void consumeHeatingSystemUpdateSignal()
{
  bool updatePending = false;
  noInterrupts();
  if (heatingSystemUpdatePending)
  {
    heatingSystemUpdatePending = false;
    updatePending = true;
  }
  interrupts();

  if (updatePending)
  {
    // Az állapotgép frissítését az onTimerISR végzi, a meglévő működés szerint.
    unsigned long heatingUpdateStarted = millis();
    unsigned long heatingUpdateDuration = millis() - heatingUpdateStarted;
    if (heatingUpdateDuration > 500UL)
    {
      logWarn("hs->update took %lums\n", heatingUpdateDuration);
    }
  }
}

void processWebSocket()
{
  if (config.communicationTypeWebApp == "WS")
  {
    webSocket.loop();
  }
}

void reportPeriodicMemoryUsage()
{
  if (millis() - lastMemoryDiagnosticMs >= 10000UL)
  {
    lastMemoryDiagnosticMs = millis();
    printMemoryDiagnostics("loop");
  }
}

void broadcastStateIfChanged()
{
  if (config.communicationTypeWebApp != "WS" || !isAuthenticated || !isDataNew)
  {
    return;
  }

  isDataNew = false;
  stateJsonDocument.clear();
  stateJsonResponse.reserve(STATE_JSON_RESPONSE_SIZE);
  stateJsonResponse = "";

  for (const auto &pair : IOMap)
  {
    char key[sizeof(int) * 3 + 3];
    const int keyLength = snprintf(key, sizeof(key), "%d", pair.first);
    if (keyLength < 0 || static_cast<size_t>(keyLength) >= sizeof(key))
    {
      stateJsonResponse = "{\"error\":\"Invalid integer JSON key\"}";
      break;
    }
    stateJsonDocument[key] = pair.second;
  }

  for (const auto &pair : Sensor::SensorsValue)
  {
    stateJsonDocument[pair.first.c_str()] = pair.second;
  }

  for (const auto &outerPair : HeatingElement::ElementsStateMap)
  {
    JsonObject element = stateJsonDocument.createNestedObject(outerPair.first.c_str());
    for (const auto &innerPair : outerPair.second)
    {
      element[innerPair.first.c_str()] = innerPair.second.data();
    }
  }

  if (!stateJsonDocument.overflowed() && stateJsonResponse.isEmpty())
  {
    serializeJson(stateJsonDocument, stateJsonResponse);
    webSocket.broadcastTXT(stateJsonResponse);
  }
}

void loop()
{
  reconnectWifiIfNeeded();
  handleHttpRequests();
  updateSensorsAndReportDuration();
  consumeHeatingSystemUpdateSignal();
  processWebSocket();
  reportPeriodicMemoryUsage();
  broadcastStateIfChanged();
  delay(10);
}
