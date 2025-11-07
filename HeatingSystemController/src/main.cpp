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
#include <Ticker.h>
#include <FS.h>
HeatingSystem *hs;

Ticker timer;

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

// Statikus változó definíciója az osztályon kívüls
std::map<std::string, float> Sensor::SensorsValue;

// A map konvertálása JSON formátumba
String IOMapToJson(const std::map<int, int> &intMap,
                   const std::map<std::string, float> &floatMap,
                   const std::map<std::string, std::map<std::string, std::string>> &stateMap)
{
  // Létrehozunk egy StaticJsonDocument-et
  StaticJsonDocument<1000> jsonDoc;

  // Integer kulcs-érték párok hozzáadása a JSON-hez
  for (const auto &pair : intMap)
  {
    jsonDoc[String(pair.first)] = pair.second; // Kulcsokat stringgé konvertáljuk a JSON-ban
  }

  // String kulcs-érték párok hozzáadása a JSON-hez
  for (const auto &pair : floatMap)
  {
    jsonDoc[pair.first.c_str()] = pair.second; // String kulcsok közvetlenül hozzáadhatók
  }

  // Bejárjuk az ElementsStateMap-et és feltöltjük a JSON-t
  for (const auto &outerPair : stateMap)
  {
    JsonObject element = jsonDoc.createNestedObject(outerPair.first.c_str());
    for (const auto &innerPair : outerPair.second)
    {
      element[innerPair.first.c_str()] = innerPair.second.c_str();
    }
  }

  // Serializáljuk a JSON objektumot egy stringgé
  String jsonString;
  serializeJson(jsonDoc, jsonString);

  return jsonString;
}

void handleFile()
{
  String path = server.uri();
  if (path.endsWith("/"))
    path += "index.html"; // főoldal
  String contentType = getContentType(path);

  if (SPIFFS.exists(path))
  {
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
  }
  else
  {
    // Ha nem létezik: Angular routing -> index.html visszaadása
    File file = SPIFFS.open("/index.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  }
}

void handleWebUserInterfaceRequest()
{
  String path = server.uri(); // pl. "/" vagy "/style.css"

  // Ha "/" érkezik, az index.html legyen
  if (path == "/")
  {
    path = "/index.html";
  }

  String fullPath = "" + path; // a data/browser/ mappa
  String msg = "Serving: " + fullPath + "\n";
  logMessage(msg.c_str());

  if (!SPIFFS.exists(fullPath))
  {
    server.send(404, "text/plain", "File not found");
    return;
  }
  File file = SPIFFS.open(fullPath, "r");

  // MIME típus meghatározása a fájl kiterjesztés alapján
  String contentType = "text/plain";
  if (path.endsWith(".html"))
    contentType = "text/html";
  else if (path.endsWith(".css"))
    contentType = "text/css";
  else if (path.endsWith(".js"))
    contentType = "application/javascript";
  else if (path.endsWith(".png"))
    contentType = "image/png";
  else if (path.endsWith(".jpg") || path.endsWith(".jpeg"))
    contentType = "image/jpeg";

  server.streamFile(file, contentType);
  file.close();
}

// Hőmérséklet API kezelő
void handleHeatingRequest()
{
  // Ellenőrzés, hogy tartalmazza-e az "element" paramétert a kérés
  if (server.hasArg("name") && server.hasArg("setTemp"))
  {
    String name = server.arg("name"); // Az elem neve a "element" paraméterből
    String value = server.arg("setTemp");
    Serial.print("Element received: ");
    Serial.println(name);
    Serial.println(value);

    for (auto &element : hs->mergedList)
    {
      if (element->name == std::string(name.c_str()))
      {
        if (value == "true")
        {
          element->setNeedHeating(true);
          server.send(200, "text/plain", "Heating element: " + name + " request heat, True");
        }
        else if (value == "false")
        {
          element->setNeedHeating(false);
          server.send(200, "text/plain", "Heating element: " + name + " request heat, False");
        }
        server.send(400, "text/plain", "Heating element: " + name + " request heat, unknown value " + value);
        return;
      }
    }
    // Ha a paraméter hiányzik, válaszoljunk 400-as státusszal
    server.send(400, "text/plain", "Error: Unknown element with name" + name);
  }
  server.send(400, "text/plain", "Error: Missing 'name' or 'setTemp' parameter.");
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

void handleSetTemperatureTest()
{
  if (server.method() == HTTP_POST)
  {
    // Ellenőrizzük, hogy a kérés tartalmaz-e JSON adatokat
    if (server.hasArg("plain"))
    {
      String body = server.arg("plain");
      String errorMsg = "";

      // Létrehozzuk a JSON dokumentumot
      DynamicJsonDocument doc(1024);

      // A JSON adat deszerializálása
      DeserializationError error = deserializeJson(doc, body);

      if (error)
      {
        // Ha nem sikerült deszerializálni, hibát küldünk válaszként
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }

      // Az összes szenzor értékeinek beállítása
      for (JsonPair kv : doc.as<JsonObject>())
      {
        float tempValue = 0;

        if (kv.value().is<float>())
        {
          tempValue = kv.value().as<float>();
        }
        else
        {
          // Ha nem float típusú, akkor hibát vagy más értéket kezeljünk
          errorMsg += "Wrong float value:" + kv.value().as<String>() + ", ";
        }

        // Tároljuk a szenzor id-t és az értékét a sensorTemps térképben
        Sensor::SensorsValue[std::string(kv.key().c_str())] = tempValue;
      }

      // Visszajelzés, hogy sikerült beállítani a hőmérsékletet
      String response = errorMsg == "" ? "{\"status\":\"success\"}" : "{\"status\":\"" + errorMsg + "\"}";
      server.send(errorMsg == "" ? 200 : 400, "application/json", response);
    }
    else
    {
      // Ha nincs POST body, akkor hibát küldünk
      server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    }
  }
}

void hadleStateRequest()
{
  // JSON string létrehozása
  String jsonResponse = IOMapToJson(IOMap, Sensor::SensorsValue, HeatingElement::ElementsStateMap);

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

void onTimerISR()
{
  webSocket.loop(); // Az időzítő minden hívásnál váltja az értéket
  hs->update();
}

int val = 0;

void setup()
{
  // Soros port inicializálása
  Serial.begin(115200);
  Serial.flush();
  Wire.begin(D2, D1); // SDA: D2, SCL: D1 (ESP8266)

  pwmController.setupSingleDevice(Wire, 0x41); // Az alapértelmezett I2C cím: 0x40
  pwmController.setToServoFrequency();         // Szervómotorokhoz beállított frekvencia
  pwmController.setAllDevicesOutputsInverted();
  pwmController.setupOutputEnablePin(D0);
  pwmController.setOutputsHighWhenDisabled();
  pwmController.disableOutputs(D0);

  // pwmController.setAllChannelsDutyCycle(0, 0);

  delay(500);
  // gdbstub_init();
  Serial.println();
  logMessage("Start\n");
  // SPIFFS inicializálása és konfiguráció betöltése
  loadConfig();

  hs = new HeatingSystem("/config.json");
  // NTP kliens indítása
  timeClient.begin();
  timeClient.update();

  WiFi.hostname(config.hostname);

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
  server.on("/setTemp", HTTP_POST, handleSetTemperatureTest);

  server.on("/", HTTP_GET, handleWebUserInterfaceRequest);
  server.on("/styles.css", HTTP_GET, handleWebUserInterfaceRequest);
  server.on("/polyfills.js", HTTP_GET, handleWebUserInterfaceRequest);
  server.on("/main.js", HTTP_GET, handleWebUserInterfaceRequest);

  // Alap handler: minden kérés ide jön
  server.onNotFound(handleFile);

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

  timer.attach(1, onTimerISR);

  pwmController.enableOutputs(D0);
  ESP.wdtEnable(5000);

  // ideiglenesen automatikusan elinditja a fureskerelemet
  for (auto &element : hs->mergedList)
  {
    std::string name = "PadloKozos";
    if (element->name == std::string(name.c_str()))
    {
      element->setNeedHeating(true);
    }
  }
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
  // hs->update();

  // 5 másodperc várakozás
  delay(1000);
  // logMessage("Temp: %f", temperature);
  MDNS.update();

  // Ha hitelesítés sikeres, folyamatosan küldhetünk adatokat
  if (isAuthenticated && isDataNew)
  {
    isDataNew = false;
    String data = IOMapToJson(IOMap, Sensor::SensorsValue, HeatingElement::ElementsStateMap);
    webSocket.broadcastTXT(data);
  }
}
