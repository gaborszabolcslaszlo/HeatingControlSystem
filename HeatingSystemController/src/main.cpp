#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <exception>

#include <PCA9685.h>

#include "certificates.h"
#include "config.h"

#include <GDBStub.h>
#include <WebSocketsServer.h>
#include "temp_sensors.h"
#include "HeatingSystemElements/HeatingSystem.h"

HeatingSystem *hs;

// Globális webszerver változó
ESP8266WebServer server(80);
// WebSocket szerver portja
WebSocketsServer webSocket = WebSocketsServer(81);
// Jelszó hitelesítéshez
const char *websocketPassword = "secretpassword";

bool isAuthenticated = false;

const int max_attempts = 1;
int attempt = 0;

bool isDataNew = false;

// I2C címe a PCA9685-nek (alapértelmezett 0x40)
PCA9685 pwmController;

// Létrehozunk egy std::map-et, ahol mind a kulcs, mind az érték int típusú
std::map<int, int> IOMap;

// Statikus változó definíciója az osztályon kívül
std::map<std::string, float> Sensor::SensorsValue;

// A map konvertálása JSON formátumba
String IOMapToJson(const std::map<int, int> &intMap, const std::map<std::string, float> &floatMap)
{
  // Létrehozunk egy StaticJsonDocument-et
  StaticJsonDocument<400> jsonDoc;

  // Integer kulcs-érték párok hozzáadása a JSON-hez
  for (const auto &pair : intMap)
  {
    jsonDoc[String(pair.first)] = pair.second; // Kulcsokat stringgé konvertáljuk a JSON-ban
  }

  // String kulcs-érték párok hozzáadása a JSON-hez
  for (const auto &pair : floatMap)
  {
    jsonDoc[pair.first] = pair.second; // String kulcsok közvetlenül hozzáadhatók
  }

  // Serializáljuk a JSON objektumot egy stringgé
  String jsonString;
  serializeJson(jsonDoc, jsonString);

  return jsonString;
}

// Hőmérséklet API kezelő
void handleHeatingRequest()
{
  String response = "{ \"temperatures\": [";

  // Hőmérséklet olvasása szenzoroktól
  if (sensorCount == 0)
  {
    response = "{ \"temperatures\": [], \"error\": \"No sensors found.\" }";
  }
  else
  {
    sensors.requestTemperatures();
    for (int i = 0; i < sensorCount; i++)
    {
      float tempC = sensors.getTempC(sensorAddresses[i]);
      response += String(tempC);
      if (i < sensorCount - 1)
      {
        response += ", ";
      }
    }
    response += "], \"error\": null }";
  }
  server.send(200, "application/json", response);
}

// Funkció az újraindításhoz
void handleReboot()
{
  // Ellenőrizzük a paramétereket
  if (server.arg("user") == "admin" && server.arg("password") == "password")
  {
    server.send(200, "text/plain", "Rebooting..."); // Válasz a kérésre
    delay(1000);                                    // Várjunk egy kicsit, hogy a válasz eljusson
    ESP.restart();                                  // Újraindítás
  }
  else
  {
    server.send(401, "text/plain", "Unauthorized"); // Jogosulatlan válasz
  }
}

void hadleStateRequest()
{
  // JSON string létrehozása
  String jsonResponse = IOMapToJson(IOMap, Sensor::SensorsValue);

  // Válasz küldése JSON formátumban
  server.send(200, "application/json", jsonResponse);
}

void writePwmIO(int id, int duty)
{
  pwmController.setChannelDutyCycle(id, duty * 4096 / 100);
  IOMap[id] = duty;
  isDataNew = true;
}

void writeIO(int id, int value)
{
  if (value > 0)
  {
    pwmController.setChannelDutyCycle(id, 4096);
    IOMap[id] = 100;
  }
  else
  {
    pwmController.setChannelDutyCycle(id, 0);
    IOMap[id] = 0;
  }
  isDataNew = true;
}

// WebSocket esemény kezelő
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_DISCONNECTED:
    Serial.printf("[%u] Disconnected!\n", num);
    isAuthenticated = false; // Ha a kliens lecsatlakozik, újra kell hitelesíteni
    break;

  case WStype_CONNECTED:
  {
    IPAddress ip = webSocket.remoteIP(num);
    Serial.printf("[%u] Connected from %s\n", num, ip.toString().c_str());
  }
  break;

  case WStype_TEXT:
    Serial.printf("[%u] Text: %s\n", num, payload);

    // Az első üzenet a jelszó hitelesítésére szolgál
    if (!isAuthenticated)
    {
      if (strcmp((const char *)payload, websocketPassword) > 0)
      {
        Serial.println("Hitelesítés sikeres!");
        isAuthenticated = true;
        webSocket.sendTXT(num, "Authentication successful!");
      }
      else
      {
        Serial.println("Hitelesítés sikertelen!");
        webSocket.sendTXT(num, "Authentication failed!");
        webSocket.disconnect(num); // Ha a hitelesítés sikertelen, bontjuk a kapcsolatot
      }
    }
    else
    {
      // Ha már hitelesítettük a klienst, itt kezelhetjük az üzeneteket
      Serial.println("Authenticated data received");
    }
    break;
  }
}

void setup()
{
  // Soros port inicializálása
  Serial.begin(115200);
  Serial.flush();
  delay(500);
  gdbstub_init();
  Serial.println();
  logMessage("Start\n");
  // SPIFFS inicializálása és konfiguráció betöltése
  loadConfig();

  Wire.begin(D2, D1); // SDA: D2, SCL: D1 (ESP8266)

  pwmController.setupSingleDevice(Wire, 0x41); // Az alapértelmezett I2C cím: 0x40
  pwmController.setToServoFrequency();         // Szervómotorokhoz beállított frekvencia

  hs = new HeatingSystem("/config.json");
  // NTP kliens indítása
  timeClient.begin();
  timeClient.update();

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

  if (config.wifiMode == "STA+AP" || config.wifiMode == "STA")
  {
    WiFi.begin(config.wifiSSID, config.wifiPassword);
    while (WiFi.status() != WL_CONNECTED && attempt < max_attempts)
    {
      delay(500);
      logMessage(".\n");
      attempt++;
    }
    if (WiFi.waitForConnectResult() == WL_CONNECTED)
    {
    }
  }

  if (config.wifiMode == "STA+AP" || config.wifiMode == "AP")
  {
    WiFi.softAP(config.APSSID, config.APPassword);
    logMessage("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  }

  server.keepAlive(true);
  server.enableCORS(true);
  // HTTPS beállítása
  // server.getServer()
  // .setRSACert(new X509List(cert), new PrivateKey(private_key));

  // CORS fejléc beállítása
  server.on("/configuration", HTTP_OPTIONS, []()
            {
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(200); });

  // API végpontok
  server.on("/heatingRequest", HTTP_GET, handleHeatingRequest);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/configuration", HTTP_GET, handleConfiguration);
  server.on("/configuration", HTTP_POST, handleConfiguration);
  server.on("/upload", HTTP_POST, []()
            { server.send(200, "text/plain", "File Uploaded Successfully"); }, handleFileUpload);
  // Beállítjuk a reboot végpontot
  server.on("/reboot", HTTP_GET, handleReboot);
  server.on("/state", HTTP_GET, hadleStateRequest);

  const char *headerkeys[] = {"User-Agent", "Content-Type", "Request Headers"};
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);
  // ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize);

  // Webszerver indítása
  // Start the mDNS responder with the domain "myesp.local"
  if (MDNS.begin("kazan"))
  {
    Serial.println("mDNS responder started with domain: http://kazan.local");
  }
  else
  {
    Serial.println("Error setting up mDNS responder!");
  }

  server.begin();
  logMessage("HTTPS szerver indítva.\n");
  // MDNS.addService("http", "tcp", 80);

  hs->printHeatingSystem();
  // Szenzorok inicializálása
  setupSensors();

  // WebSocket szerver indítása
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop()
{
  if (config.wifiMode == "STA+AP" || config.wifiMode == "STA")
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.print("Csatlakozás a WiFi-hez...\n");
      WiFi.begin(config.wifiSSID, config.wifiPassword); // Próbálkozzunk csatlakozni
    }
  }

  try
  {
    // A kód, amely hibát okozhat
    server.handleClient();
  }
  catch (const std::exception &e)
  {
    // Hiba kezelése
    Serial.println("Kivétel történt:");
    Serial.println(e.what());
  }

  timeClient.update();
  updateDsSensors();
  hs->update();

  // 5 másodperc várakozás
  delay(1000);

  // logMessage("Temp: %f", temperature);
  MDNS.update();

  webSocket.loop();
  // Ha hitelesítés sikeres, folyamatosan küldhetünk adatokat
  if (isAuthenticated && isDataNew)
  {
    static unsigned long lastSendTime = 0;

    isDataNew = false;
    String data = IOMapToJson(IOMap, Sensor::SensorsValue);
    webSocket.broadcastTXT(data);
  }
}
