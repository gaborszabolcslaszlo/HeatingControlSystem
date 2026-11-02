// config.h

#ifndef CONFIG_H
#define CONFIG_H

#include <FS.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <map>
#include <string>
#include "comon.h"

// Globális webszerver változó
extern ESP8266WebServer server;

// Konfiguráció struktúra
struct Configuration
{
  String wifiSSID;
  String wifiPassword;
  String wifiMode;
  String APSSID;
  String APPassword;
  String LastLogFileName;
  String hostname;
  String communicationTypeWebApp; // WS - web socket, API - rest api
};

// Globális konfigurációs változó
Configuration config;
int configurationUploadResult = 0;
String configurationUploadError;

DynamicJsonDocument configContent(8192);

// Konfiguráció betöltése a fájlból
void loadConfig()
{
  // Az SPIFFS inicializálása
  if (!SPIFFS.begin())
  {
    logError("Hiba a SPIFFS indításakor.\n");
    return;
  }

  // Ellenőrizzük, hogy létezik-e a konfigurációs fájl
  if (!SPIFFS.exists("/config.json"))
  {
    logError("Nincs konfigurációs fájl.\n");
    return;
  }

  // Konfigurációs fájl megnyitása
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile)
  {
    logError("Nem sikerült megnyitni a konfigurációs fájlt.\n");
    return;
  }

  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size + 1]);
  size_t bytesRead = configFile.readBytes(buf.get(), size);
  buf[bytesRead] = '\0';

  DeserializationError error = deserializeJson(configContent, buf.get(), bytesRead);

  if (error)
  {
    logError("Hiba a konfiguráció beolvasásakor.\n");
    return;
  }

  configFile.close();

  // Konfiguráció beállítása
  config.wifiSSID = configContent["wifiSSID"].as<String>();
  config.wifiPassword = configContent["wifiPassword"].as<String>();
  config.APSSID = configContent["AP_SSID"].as<String>();
  config.APPassword = configContent["AP_Password"].as<String>();
  config.wifiMode = configContent["WifiMode"].as<String>();
  config.hostname = configContent["hostname"].as<String>();
  config.communicationTypeWebApp = configContent["comType"].as<String>();
  logInfo("Config betöltve:\n");
  logInfo("  WiFi SSID: %s\n", config.wifiSSID.c_str());
  logInfo("  WiFi hostname: %s\n", config.hostname.c_str());
}

// Konfiguráció mentése fájlba
void saveConfig()
{
  DynamicJsonDocument jsonBuffer(8192);

  File existingConfig = SPIFFS.open("/config.json", "r");
  if (existingConfig)
  {
    DeserializationError error = deserializeJson(jsonBuffer, existingConfig);
    existingConfig.close();
    if (error)
    {
      logError("Nem sikerült a meglévő konfiguráció beolvasása mentés előtt.\n");
      return;
    }
  }

  jsonBuffer["wifiSSID"] = config.wifiSSID;
  jsonBuffer["wifiPassword"] = config.wifiPassword;

  // Fájl megnyitása írásra
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    logError("Nem sikerült megnyitni a konfigurációs fájlt írásra.");
    return;
  }

  // JSON mentése a fájlba
  serializeJson(jsonBuffer, configFile);
  configFile.close();
  logInfo("Konfiguráció elmentve.");
}

bool validateConfigurationJson(const String &jsonData, String &errorMessage)
{
  DynamicJsonDocument doc(8192);
  auto isNumber = [](JsonVariant value)
  {
    return value.is<int>() || value.is<float>() || value.is<double>();
  };
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error)
  {
    errorMessage = "Érvénytelen JSON: " + String(error.c_str());
    return false;
  }

  JsonObject root = doc.as<JsonObject>();
  if (root.isNull())
  {
    errorMessage = "A konfiguráció gyökéreleme nem JSON objektum.";
    return false;
  }

  const char *requiredRootKeys[] = {
      "wifiSSID", "wifiPassword", "AP_SSID", "AP_Password",
      "WifiMode", "hostname", "comType", "HeatingSystem"};
  for (const char *key : requiredRootKeys)
  {
    if (!root.containsKey(key))
    {
      errorMessage = String("Hiányzó fő konfigurációs mező: ") + key;
      return false;
    }
  }

  String wifiMode = root["WifiMode"].as<String>();
  if (wifiMode != "STA" && wifiMode != "AP" && wifiMode != "STA+AP")
  {
    errorMessage = "A WifiMode csak STA, AP vagy STA+AP lehet.";
    return false;
  }

  String communicationType = root["comType"].as<String>();
  if (communicationType != "API" && communicationType != "WS")
  {
    errorMessage = "A comType csak API vagy WS lehet.";
    return false;
  }

  JsonObject heatingSystem = root["HeatingSystem"].as<JsonObject>();
  if (heatingSystem.isNull())
  {
    errorMessage = "A HeatingSystem mezőnek JSON objektumnak kell lennie.";
    return false;
  }
  if (!heatingSystem.containsKey("KAZAN"))
  {
    errorMessage = "A HeatingSystem.KAZAN kötelező mező.";
    return false;
  }

  std::map<std::string, bool> sensorIds;
  bool hasBoiler = false;
  const char *elementTypes[] = {"KAZAN", "RADIATOR", "PUFER"};
  for (const char *elementType : elementTypes)
  {
    if (!heatingSystem.containsKey(elementType))
    {
      continue;
    }

    JsonArray elements = heatingSystem[elementType].as<JsonArray>();
    if (elements.isNull())
    {
      errorMessage = String("A HeatingSystem.") + elementType + " mezőnek tömbnek kell lennie.";
      return false;
    }
    if (String(elementType) == "KAZAN" && elements.size() == 0)
    {
      errorMessage = "Legalább egy kazán konfigurációja szükséges.";
      return false;
    }

    for (JsonObject element : elements)
    {
      String elementName = element["name"].as<String>();
      if (elementName.length() == 0)
      {
        errorMessage = String("Hiányzó elemnév a ") + elementType + " listában.";
        return false;
      }
      if (String(elementType) == "KAZAN")
      {
        hasBoiler = true;
        const char *boilerFields[] = {
            "retourTempProtValue", "tourTempProtValue", "activationThreshold"};
        for (const char *field : boilerFields)
        {
          if (!element.containsKey(field) || !isNumber(element[field]))
          {
            errorMessage = String("Hiányzó kazánbeállítás: ") + field;
            return false;
          }
        }
      }

      if (!element.containsKey("sensors") || !element["sensors"].is<JsonArray>())
      {
        errorMessage = String("Hiányzó vagy hibás sensors tömb: ") + elementName;
        return false;
      }
      for (JsonObject sensor : element["sensors"].as<JsonArray>())
      {
        String sensorId = sensor["id"].as<String>();
        String position = sensor["position"].as<String>();
        if (sensor["model"].as<String>().length() == 0 || sensorId.length() == 0)
        {
          errorMessage = String("Hiányos szenzorkonfiguráció: ") + elementName;
          return false;
        }
        if (position != "body" && position != "tour" && position != "retour")
        {
          errorMessage = String("Érvénytelen szenzorpozíció: ") + position;
          return false;
        }
        std::string sensorIdValue = sensorId.c_str();
        if (sensorIds.count(sensorIdValue) != 0)
        {
          errorMessage = String("Duplikált szenzorazonosító: ") + sensorId;
          return false;
        }
        sensorIds[sensorIdValue] = true;

        if (sensor.containsKey("level"))
        {
          int level = sensor["level"] | -1;
          if (level < 0 || level > 100)
          {
            errorMessage = String("A szenzorszintnek 0 és 100 között kell lennie: ") + sensorId;
            return false;
          }
        }
      }

      if (!element.containsKey("pumps") || !element["pumps"].is<JsonArray>())
      {
        errorMessage = String("Hiányzó vagy hibás pumps tömb: ") + elementName;
        return false;
      }
      for (JsonObject pump : element["pumps"].as<JsonArray>())
      {
        String pumpName = pump["name"].as<String>();
        String workingMode = pump["workingMode"].as<String>();
        if (pumpName.length() == 0 || pump["model"].as<String>().length() == 0)
        {
          errorMessage = String("Hiányos szivattyúkonfiguráció: ") + elementName;
          return false;
        }
        if (workingMode != "ON/OFF" && workingMode != "PWM")
        {
          errorMessage = String("Érvénytelen szivattyú üzemmód: ") + workingMode;
          return false;
        }
        int minSignal = pump["minControlSig"] | -1;
        int maxSignal = pump["maxControlSig"] | -1;
        int ioNumber = pump["IOnumber"] | -1;
        if (!pump.containsKey("IOnumber") || ioNumber < 0 || ioNumber > 15 ||
            minSignal < 0 || maxSignal > 100 || minSignal > maxSignal)
        {
          errorMessage = String("Hibás szivattyú vezérlési tartomány: ") + pumpName;
          return false;
        }

        if (pump.containsKey("pid"))
        {
          JsonObject pid = pump["pid"].as<JsonObject>();
          String tempSensorId = pid["tempSensorId"].as<String>();
          if (pid.isNull() || tempSensorId.length() == 0 ||
              !pid.containsKey("setPoint") || !isNumber(pid["setPoint"]) ||
              !pid.containsKey("enabled") || !pid["enabled"].is<bool>() ||
              !pid.containsKey("p") || !isNumber(pid["p"]) ||
              !pid.containsKey("i") || !isNumber(pid["i"]) ||
              !pid.containsKey("d") || !isNumber(pid["d"]))
          {
            errorMessage = String("Hiányos PID-konfiguráció: ") + pumpName;
            return false;
          }
          if (sensorIds.count(std::string(tempSensorId.c_str())) == 0)
          {
            errorMessage = String("A PID ismeretlen szenzorra hivatkozik: ") + tempSensorId;
            return false;
          }
        }
      }
    }
  }

  if (!hasBoiler)
  {
    errorMessage = "Nem található érvényes kazánkonfiguráció.";
    return false;
  }

  return true;
}

void handleFileUploadByPlanText()
{
  // Ellenőrizzük, hogy van-e adat a POST kérésben
  if (server.hasArg("plain"))
  {
    String jsonData = server.arg("plain");

    // JSON parsolás
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, jsonData);

    if (error)
    {
      logError("Failed to parse JSON");
      server.send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid JSON format\"}");
      return;
    }

    // Fájlnév és tartalom olvasása
    const char *filename = doc["filename"];
    String content = "";
    serializeJson(doc["content"], content);

    if (String(filename) == "config.json")
    {
      String validationError;
      if (!validateConfigurationJson(content, validationError))
      {
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"" + validationError + "\"}");
        return;
      }
    }

    logInfo("Received filename: %s", filename);
    logInfo("Content: %s", content.c_str());

    // Fájl létrehozása és írása
    File file = SPIFFS.open("/" + String(filename), "w");
    if (file)
    {
      file.print(content);
      file.close();
      server.send(200, "application/json", "{\"status\":\"success\", \"message\":\"File uploaded successfully\"}");
    }
    else
    {
      server.send(500, "application/json", "{\"status\":\"error\", \"message\":\"Failed to write file\"}");
    }
  }
  else
  {
    server.send(400, "application/json", "{\"status\":\"error\", \"message\":\"No data received\"}");
  }
}

int handleFileUpload()
{

  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START)
  {
    String filename = "/" + upload.filename;
    String writePath = filename == "/config.json" ? "/config.json.upload" : filename;
    configurationUploadResult = 0;
    configurationUploadError = "";
    logInfo("Uploading file: %s\n", filename.c_str());

    File file = SPIFFS.open(writePath, "w");
    logInfo("Creating file: %s\n", writePath.c_str());

    if (!file)
    {
      logError("Failed to open file for writing\n");
      return -1;
    }
    file.close();
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    String filename = "/" + upload.filename;
    String writePath = filename == "/config.json" ? "/config.json.upload" : filename;
    File file = SPIFFS.open(writePath, "a");
    if (file)
    {
      file.write(upload.buf, upload.currentSize);
      file.close();
    }
    else
    {
      logError("Failed to open file for writing during upload %s\n", upload.filename.c_str());
      return -1;
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    logInfo("File successfully uploaded: %s\n", upload.filename.c_str());
    if (upload.filename == "config.json")
    {
      File uploadedConfig = SPIFFS.open("/config.json.upload", "r");
      String content = uploadedConfig ? uploadedConfig.readString() : "";
      if (uploadedConfig)
      {
        uploadedConfig.close();
      }

      String validationError;
      if (!validateConfigurationJson(content, validationError))
      {
        SPIFFS.remove("/config.json.upload");
        configurationUploadResult = 400;
        configurationUploadError = validationError;
        logError("Hibás konfiguráció, a jelenlegi fájl megmaradt: %s\n", validationError.c_str());
        return -2;
      }

      bool hadPreviousConfig = SPIFFS.exists("/config.json");
      SPIFFS.remove("/config.json.backup");
      if (hadPreviousConfig && !SPIFFS.rename("/config.json", "/config.json.backup"))
      {
        SPIFFS.remove("/config.json.upload");
        configurationUploadResult = 500;
        configurationUploadError = "Nem sikerült a jelenlegi konfiguráció biztonsági mentése.";
        return -1;
      }

      if (!SPIFFS.rename("/config.json.upload", "/config.json"))
      {
        if (hadPreviousConfig)
        {
          SPIFFS.rename("/config.json.backup", "/config.json");
        }
        configurationUploadResult = 500;
        configurationUploadError = "Nem sikerült az új konfiguráció aktiválása.";
        return -1;
      }
      SPIFFS.remove("/config.json.backup");
    }
  }

  return 0;
}

void handleConfigFileUpload()
{
  if (configurationUploadResult != 0)
  {
    int statusCode = configurationUploadResult;
    String message = configurationUploadError.length() > 0
                         ? configurationUploadError
                         : "A konfiguráció feltöltése sikertelen.";
    server.send(statusCode, "application/json", "{\"status\":\"error\",\"message\":\"" + message + "\"}");
  }
  else
  {
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"A konfiguráció feltöltve és ellenőrizve. Újraindítás szükséges.\"}");
  }
}

// Az API kezelő függvény, amely a /configuration végpontot kezeli
void handleConfiguration()
{
  // server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");

  logInfo("HTTP config request:\n");

  if (server.method() == HTTP_OPTIONS)
  {
    // Preflight kérelemre nincs body, csak fejlécek
    server.send(204); // No Content
    return;
  }

  if (server.method() == HTTP_GET)
  {
    logDebug("Configuration GET request\n");
    // Válasz a jelenlegi konfiguráció JSON formátumban
    /* StaticJsonDocument<512> jsonBuffer;
     jsonBuffer["wifiSSID"] = config.wifiSSID;
     jsonBuffer["wifiPassword"] = config.wifiPassword;

     String response;
     serializeJson(jsonBuffer, response);
     server.send(200, "application/json", response);*/

    if (!sendFile("/config.json"))
    {
      server.send(404, "text/plain", "File Not Found");
    }
  }

  else if (server.method() == HTTP_POST)
  {
    handleFileUpload();
  }

  server.client().flush();
}

#endif
