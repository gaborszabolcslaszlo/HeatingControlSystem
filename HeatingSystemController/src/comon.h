// comon.h

#ifndef COMON_H
#define COMON_H
#include <WiFiUdp.h>
#include <NTPClient.h>

extern ESP8266WebServerSecure server;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 600000); // 1 órás időeltolás (3600 másodperc), 600 másodpercenként frissít

// Log fájl neve
const char* logFileName = "/log.txt";

// Tartalom típus meghatározása fájlnév alapján
String getContentType(String filename) {
  if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/pdf";
  else if (filename.endsWith(".zip")) return "application/zip";
  return "text/plain";  // Alapértelmezett típus
}

void createLogFile()
{
  File logFile = SPIFFS.open(logFileName, "w");
}

// Logger funkció, ami hasonló a printf()-hez
void logMessage(const char* format, ...) {
  // Variadic argumentek kezelése
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  // Időbélyeg lekérése
  String timestamp = timeClient.getFormattedTime();

  // Összeállított üzenet
  String logEntry = String("[") + timestamp + String("] ") + String(buffer) + String("\n");

  // 1. Kiíratás Serial-ra
  Serial.print(logEntry);

  // 2. Kiíratás fájlba (SPIFFS)
  File logFile = SPIFFS.open(logFileName, "a");  // "a" mód: hozzáfűzés
  if (logFile) {
    logFile.print(logEntry);
    logFile.close();
  } else {
    Serial.println("Nem sikerült megnyitni a log fájlt.");
  }
}

boolean sendFile(String path) {
  File file = SPIFFS.open(path, "r");
  if (!file) {    
    return false;
  }

  // Fájl méretének ellenőrzése és tartalom küldése
  String contentType = getContentType(path);  // Tartalom típus meghatározása
  server.streamFile(file, contentType);
  file.close();
  return true;
}

void handleLogs() {
  if (server.method() == HTTP_GET) {
    // Válasz a jelenlegi konfiguráció JSON formátumban
   /* StaticJsonDocument<512> jsonBuffer;
    jsonBuffer["wifiSSID"] = config.wifiSSID;
    jsonBuffer["wifiPassword"] = config.wifiPassword;

    String response;
    serializeJson(jsonBuffer, response);
    server.send(200, "application/json", response);*/

    sendFile("/log.txt");
  }}

#endif
