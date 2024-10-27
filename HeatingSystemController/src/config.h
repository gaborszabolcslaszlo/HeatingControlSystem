// config.h

#ifndef CONFIG_H
#define CONFIG_H

#include <FS.h>
#include <ArduinoJson.h>
#include <ESP8266WebServerSecure.h>
#include "comon.h"

// Globális webszerver változó
extern ESP8266WebServerSecure server;

// Konfiguráció struktúra
struct Configuration {
  String wifiSSID;
  String wifiPassword;
  String wifiMode;
  String APSSID;
  String APPassword;
  String LastLogFileName;
};

// Globális konfigurációs változó
Configuration config;

StaticJsonDocument<1024> configContent;

// Konfiguráció betöltése a fájlból
void loadConfig() {
  // Az SPIFFS inicializálása
  if (!SPIFFS.begin()) {
    logMessage("Hiba a SPIFFS indításakor.");
    return;
  }

  // Ellenőrizzük, hogy létezik-e a konfigurációs fájl
  if (!SPIFFS.exists("/config.json")) {
    logMessage("Nincs konfigurációs fájl.");
    return;
  }

  // Konfigurációs fájl megnyitása
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    logMessage("Nem sikerült megnyitni a konfigurációs fájlt.");
    return;
  }

  // Fájl tartalmának beolvasása
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  // JSON feldolgozása
  DeserializationError error = deserializeJson(configContent, buf.get());

  if (error) {
    logMessage("Hiba a konfiguráció beolvasásakor.");
    return;
  }

  configFile.close();

  // Konfiguráció beállítása
  config.wifiSSID = configContent["wifiSSID"].as<String>();
  config.wifiPassword = configContent["wifiPassword"].as<String>();
  config.APSSID = configContent["AP_SSID"].as<String>();
  config.APPassword = configContent["AP_Password"].as<String>();
  config.wifiMode = configContent["WifiMode"].as<String>();
  

  logMessage("Config betöltve:");
  logMessage("WiFi SSID: %s", config.wifiSSID);
  logMessage("WiFi Password: %s", config.wifiPassword);

}

// Konfiguráció mentése fájlba
void saveConfig() {
  StaticJsonDocument<512> jsonBuffer;
  jsonBuffer["wifiSSID"] = config.wifiSSID;
  jsonBuffer["wifiPassword"] = config.wifiPassword;

  // Fájl megnyitása írásra
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    logMessage("Nem sikerült megnyitni a konfigurációs fájlt írásra.");
    return;
  }

  // JSON mentése a fájlba
  serializeJson(jsonBuffer, configFile);
  configFile.close();
  logMessage("Konfiguráció elmentve.");
}

void handleFileUploadByPlanText() {
  // Ellenőrizzük, hogy van-e adat a POST kérésben
  if (server.hasArg("plain")) {
    String jsonData = server.arg("plain");
    
    // JSON parsolás
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, jsonData);

    if (error) {
      logMessage("Failed to parse JSON");
      server.send(400, "application/json", "{\"status\":\"error\", \"message\":\"Invalid JSON format\"}");
      return;
    }

    // Fájlnév és tartalom olvasása
    const char* filename = doc["filename"];
    String content = "";
    serializeJson(doc["content"], content);


    logMessage("Received filename: %s", filename);
    logMessage("Content: %s",  content);

    // Fájl létrehozása és írása
    File file = SPIFFS.open("/" + String(filename), "w");
    if (file) {
      file.print(content);
      file.close();
      server.send(200, "application/json", "{\"status\":\"success\", \"message\":\"File uploaded successfully\"}");
    } else {
      server.send(500, "application/json", "{\"status\":\"error\", \"message\":\"Failed to write file\"}");
    }
  } else {
    server.send(400, "application/json", "{\"status\":\"error\", \"message\":\"No data received\"}");
  }
}

void handleFileUpload() {

  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/" + upload.filename;
    logMessage("Uploading file: %s", filename);

    File file = SPIFFS.open(filename, "w");
    logMessage("Createing file: %s", filename);

    if (!file) {
      logMessage("Failed to open file for writing");
      return;
    }
    file.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    logMessage("Writing data to file: %s", logMessage);
    File file = SPIFFS.open("/" + upload.filename, "a");
    if (file) {
      file.write(upload.buf, upload.currentSize);
      file.close();
    } else {
      logMessage("Failed to open file for writing during upload %s", upload.filename );
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    logMessage("File successfully uploaded: %s", upload.filename);
  }
}




// Az API kezelő függvény, amely a /configuration végpontot kezeli
void handleConfiguration() {
  if (server.method() == HTTP_GET) {
    // Válasz a jelenlegi konfiguráció JSON formátumban
   /* StaticJsonDocument<512> jsonBuffer;
    jsonBuffer["wifiSSID"] = config.wifiSSID;
    jsonBuffer["wifiPassword"] = config.wifiPassword;

    String response;
    serializeJson(jsonBuffer, response);
    server.send(200, "application/json", response);*/

    if(!sendFile("/config.json"))
    {
      server.send(404, "text/plain", "File Not Found");
    }
  }
  
  else if (server.method() == HTTP_POST) {
      if (server.hasHeader("Content-Type")){
        String contentType = server.header("Content-Type");
        logMessage("Content-Type: %s\n", contentType.c_str());

        if(contentType.indexOf("application/json") >=  0)
        {
          handleFileUploadByPlanText();
        }
        else if(contentType.indexOf("multipart/form-data") >=0 )
        {
          handleFileUpload();
        }
        else
        {
          server.send(500, "text/plain", "Missing header Content-Type");
        }
     }
  }
}

#endif
