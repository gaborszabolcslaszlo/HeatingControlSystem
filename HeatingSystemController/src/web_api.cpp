#include "web_api.h"

#include <ArduinoJson.h>
#include <FS.h>
#include <cstdlib>
#include <cmath>
#include <map>
#include <string>

#include "app_globals.h"
#include "hardware_io.h"
#include "HeatingSystemElements/HeatingElement.h"
#include "HeatingSystemElements/Sensor.h"
#include "comon.h"
#ifdef ESP32
#include <esp_heap_caps.h>
#include <esp_system.h>
#endif

bool allowApiRequest()
{
#if API_MIN_REQUEST_INTERVAL_MS <= 0
  return true;
#else
  static unsigned long lastRequestAt = 0;
  static bool hasPreviousRequest = false;
  const unsigned long now = millis();

  if (hasPreviousRequest && now - lastRequestAt < API_MIN_REQUEST_INTERVAL_MS_VALUE)
  {
    server.sendHeader("Retry-After", "1");
    server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
    return false;
  }

  lastRequestAt = now;
  hasPreviousRequest = true;
  return true;
#endif
}

void handleHeatingRequest()
{
  if (!allowApiRequest())
  {
    return;
  }

  // Ellenőrzés, hogy tartalmazza-e az "element" paramétert a kérés
  if (server.hasArg("name") && server.hasArg("setTemp"))
  {
    String name = server.arg("name"); // Az elem neve a "element" paraméterből
    String value = server.arg("setTemp");
    /*Serial.print("Heating request for: ");
    Serial.print(name);
    Serial.print(" ");
    Serial.println(value);*/

    for (auto &element : hs->mergedList)
    {
      if (element->name == std::string(name.c_str()))
      {
        if (value == "true")
        {
          element->setNeedHeating(true);
          logInfo("[API] setNeedHeating: %s = true\n", name.c_str());
          server.send(200, "text/plain", "Heating element: " + name + " request heat, True");
          return;
        }
        else if (value == "false")
        {
          element->setNeedHeating(false);
          logInfo("[API] setNeedHeating: %s = false\n", name.c_str());
          server.send(200, "text/plain", "Heating element: " + name + " request heat, False");
          return;
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
  if (!allowApiRequest())
  {
    return;
  }

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
  if (!allowApiRequest())
  {
    return;
  }

  if (server.method() == HTTP_POST)
  {
    // Ellenőrizzük, hogy a kérés tartalmaz-e JSON adatokat
    if (server.hasArg("plain"))
    {
      String body = server.arg("plain");
      String errorMsg = "";

      // A kérésenkénti heap-allokációt kerüljük, mert a tesztek sokszor hívják.
      // A dokumentum a handler stackjén él, majd automatikusan felszabadul.
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
      const bool success = errorMsg == "";
      if (success)
      {
        server.send(200, "application/json", "{\"status\":\"success\"}");
      }
      else
      {
        String response = "{\"status\":\"" + errorMsg + "\"}";
        server.send(400, "application/json", response);
      }

      // A választ előbb küldjük el, hogy a részletes soros naplózás ne tartsa
      // nyitva az API-kérést, ha a soros puffer éppen telített.
      if (success)
      {
        /* logInfo("[API] setTemp\n");
         for (JsonPair kv : doc.as<JsonObject>())
         {
           std::string sensorId = kv.key().c_str();
           float temperature = kv.value().as<float>();
           char sensorLocation[64];
           Sensor::describeId(sensorId, sensorLocation, sizeof(sensorLocation));
           logInfo("  Sensor ID: %s\n", sensorId.c_str());
           logInfo("    Hely: %s\n", sensorLocation);
           logInfo("    Érték: %.2f C\n", temperature);
         }*/
      }
    }
    else
    {
      // Ha nincs POST body, akkor hibát küldünk
      server.send(400, "application/json", "{\"error\":\"No data provided\"}");
    }
  }
}

void handleManualMode()
{
  if (!allowApiRequest())
  {
    return;
  }

  if (!server.hasArg("plain"))
  {
    server.send(400, "application/json", "{\"error\":\"No JSON body provided\"}");
    return;
  }

  String body = server.arg("plain");
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, body);

  if (error || !doc.is<JsonObject>())
  {
    server.send(400, "application/json", "{\"error\":\"Expected a JSON object\"}");
    return;
  }

  struct ManualOutput
  {
    int io;
    bool manual;
    int duty;
  } outputs[8];
  size_t outputCount = 0;

  // Először minden mezőt ellenőrzünk, így hibás kérésnél nem történik
  // részleges IO-módosítás.
  for (JsonPair kv : doc.as<JsonObject>())
  {
    const char *key = kv.key().c_str();
    char *end = nullptr;
    long io = std::strtol(key, &end, 10);
    if (key[0] == '\0' || end == key || *end != '\0' ||
        io < 0 || io > 255 || IOMap.find(static_cast<int>(io)) == IOMap.end())
    {
      server.send(400, "application/json", "{\"error\":\"Invalid or unknown IO key\"}");
      return;
    }

    bool manual = true;
    float dutyValue = 0.0f;

    if (kv.value().is<JsonObject>())
    {
      JsonObject output = kv.value().as<JsonObject>();
      if (!output.containsKey("manual") || !output["manual"].is<bool>())
      {
        server.send(400, "application/json", "{\"error\":\"Each IO object needs a boolean manual flag\"}");
        return;
      }

      manual = output["manual"].as<bool>();
      if (manual)
      {
        if (!output.containsKey("value") ||
            (!output["value"].is<int>() && !output["value"].is<float>() &&
             !output["value"].is<double>()))
        {
          server.send(400, "application/json", "{\"error\":\"Manual IO needs a numeric value\"}");
          return;
        }
        dutyValue = output["value"].as<float>();
      }
    }
    else if (kv.value().is<int>() || kv.value().is<float>() ||
             kv.value().is<double>())
    {
      // Kompatibilitás a korábbi {"3": 50} formátummal.
      dutyValue = kv.value().as<float>();
    }
    else
    {
      server.send(400, "application/json", "{\"error\":\"IO value must be numeric\"}");
      return;
    }

    if (outputCount >= 8 ||
        (manual && (!std::isfinite(dutyValue) || dutyValue < 0.0f ||
                    dutyValue > 100.0f)))
    {
      server.send(400, "application/json", "{\"error\":\"PWM value must be between 0 and 100\"}");
      return;
    }

    outputs[outputCount++] = {
        static_cast<int>(io),
        manual,
        static_cast<int>(std::lround(dutyValue))};
  }

  for (size_t i = 0; i < outputCount; ++i)
  {
    if (outputs[i].manual)
    {
      setManualPwmIO(outputs[i].io, outputs[i].duty);
    }
    else
    {
      clearManualIO(outputs[i].io);
    }
  }

  server.send(200, "application/json", "{\"status\":\"success\"}");
}

void handleDeviceInfo()
{
  if (!allowApiRequest())
  {
    return;
  }

#ifdef ESP32
  const size_t filesystemTotalBytes = SPIFFS.totalBytes();
  const size_t filesystemUsedBytes = SPIFFS.usedBytes();
#else
  FSInfo fs;
  SPIFFS.info(fs);
  const size_t filesystemTotalBytes = fs.totalBytes;
  const size_t filesystemUsedBytes = fs.usedBytes;
#endif

  String json = "{";

  // Uptime
  json += "\"uptimeMs\":" + String(millis()) + ",";

  // WiFi
  json += "\"wifi\":{";
  json += "\"ssid\":\"" + WiFi.SSID() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",";
  json += "\"subnet\":\"" + WiFi.subnetMask().toString() + "\",";
  json += "\"dns\":\"" + WiFi.dnsIP().toString() + "\",";
  json += "\"mac\":\"" + WiFi.macAddress() + "\",";
#ifdef ESP32
  json += "\"hostname\":\"" + String(WiFi.getHostname() ? WiFi.getHostname() : "") + "\"";
#else
  json += "\"hostname\":\"" + WiFi.hostname() + "\"";
#endif
  json += "},";

  // Chip info
  json += "\"chip\":{";
#ifdef ESP32
  char chipId[17];
  snprintf(chipId, sizeof(chipId), "%llX", ESP.getEfuseMac());
  json += "\"chipId\":\"" + String(chipId) + "\",";
#else
  json += "\"chipId\":" + String(ESP.getChipId()) + ",";
#endif
  json += "\"cpuFreqMHz\":" + String(ESP.getCpuFreqMHz()) + ",";
#ifdef ESP32
  json += "\"sdkVersion\":\"" + String(ESP.getSdkVersion()) + "\",";
  json += "\"coreVersion\":\"ESP32 Arduino\"";
#else
  json += "\"sdkVersion\":\"" + String(ESP.getSdkVersion()) + "\",";
  json += "\"coreVersion\":\"" + String(ESP.getCoreVersion()) + "\"";
#endif
  json += "},";

  // Flash info
  json += "\"flash\":{";
#ifdef ESP32
  json += "\"chipSize\":" + String(ESP.getFlashChipSize()) + ",";
  json += "\"realChipSize\":" + String(ESP.getFlashChipSize()) + ",";
  json += "\"chipSpeed\":" + String(ESP.getFlashChipSpeed()) + ",";
  json += "\"chipMode\":0,";
#else
  json += "\"chipSize\":" + String(ESP.getFlashChipSize()) + ",";
  json += "\"realChipSize\":" + String(ESP.getFlashChipRealSize()) + ",";
  json += "\"chipSpeed\":" + String(ESP.getFlashChipSpeed()) + ",";
  json += "\"chipMode\":" + String(ESP.getFlashChipMode()) + ",";
#endif
  json += "\"sketchSize\":" + String(ESP.getSketchSize()) + ",";
  json += "\"freeSketchSpace\":" + String(ESP.getFreeSketchSpace());
  json += "},";

  // SPIFFS info
  json += "\"spiffs\":{";
  json += "\"total\":" + String(filesystemTotalBytes) + ",";
  json += "\"used\":" + String(filesystemUsedBytes) + ",";
  json += "\"free\":" + String(filesystemTotalBytes - filesystemUsedBytes);
#ifdef ESP32
  json += ",\"blockSize\":0,\"pageSize\":0,\"maxOpenFiles\":0,\"maxPathLength\":0";
#else
  json += ",\"blockSize\":" + String(fs.blockSize) + ",";
  json += "\"pageSize\":" + String(fs.pageSize) + ",";
  json += "\"maxOpenFiles\":" + String(fs.maxOpenFiles) + ",";
  json += "\"maxPathLength\":" + String(fs.maxPathLength);
#endif
  json += "},";

  // RAM / heap
  json += "\"heap\":{";
#ifdef ESP32
  json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"maxFreeBlock\":" + String(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)) + ",";
  json += "\"fragmentation\":0";
#else
  json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"maxFreeBlock\":" + String(ESP.getMaxFreeBlockSize()) + ",";
  json += "\"fragmentation\":" + String(ESP.getHeapFragmentation());
#endif
  json += "},";

  // Reset info
  json += "\"reset\":{";
#ifdef ESP32
  json += "\"reason\":\"ESP32 reset\",";
  json += "\"info\":\"" + String(esp_reset_reason()) + "\"";
#else
  json += "\"reason\":\"" + String(ESP.getResetReason()) + "\",";
  json += "\"info\":\"" + String(ESP.getResetInfo()) + "\"";
#endif
  json += "}";

  json += "}";

  server.send(200, "application/json", json);
}

void hadleStateRequest()
{
  if (!allowApiRequest())
  {
    return;
  }

  // Ideiglenes diagnosztikai mód: a state választ ugyanúgy építjük fel,
  // mint a deviceInfo választ, String összefűzésekkel.
  String json = "{";
  bool firstEntry = true;

  auto addComma = [&]()
  {
    if (!firstEntry)
    {
      json += ",";
    }
    firstEntry = false;
  };

  for (const auto &pair : IOMap)
  {
    char key[sizeof(int) * 3 + 3];
    const int keyLength = snprintf(key, sizeof(key), "%d", pair.first);
    if (keyLength < 0 || static_cast<size_t>(keyLength) >= sizeof(key))
    {
      server.send(500, "application/json", "{\"error\":\"Invalid integer JSON key\"}");
      return;
    }

    addComma();
    json += "\"";
    json += key;
    json += "\":";
    json += pair.second;
  }

  for (const auto &pair : Sensor::SensorsValue)
  {
    addComma();
    json += "\"";
    json += pair.first.c_str();
    json += "\":";
    json += String(pair.second, 6);
  }

  for (const auto &outerPair : HeatingElement::ElementsStateMap)
  {
    addComma();
    json += "\"";
    json += outerPair.first.c_str();
    json += "\":{";

    bool firstState = true;
    for (const auto &innerPair : outerPair.second)
    {
      if (!firstState)
      {
        json += ",";
      }
      firstState = false;

      json += "\"";
      json += innerPair.first.c_str();
      json += "\":\"";
      json += innerPair.second.data();
      json += "\"";
    }

    json += "}";
  }

  json += "}";
  server.send(200, "application/json", json);
}

