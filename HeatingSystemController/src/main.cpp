#include <ESP8266WiFi.h>
#include <ESP8266WebServerSecure.h>
#include <ESP8266mDNS.h>

#include "certificates.h"
#include "config.h"
#include "temp_sensors.h"
#include "comon.h"
#include "HeatingSystemElements/model.h"

// Globális webszerver változó
ESP8266WebServerSecure server(443);
const int max_attempts = 10;
int attempt = 0;

float temperature;

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
void handleReboot() {
    // Ellenőrizzük a paramétereket
    if (server.arg("user") == "admin" && server.arg("password") == "password") {
        server.send(200, "text/plain", "Rebooting..."); // Válasz a kérésre
        delay(1000); // Várjunk egy kicsit, hogy a válasz eljusson
        ESP.restart(); // Újraindítás
    } else {
        server.send(401, "text/plain", "Unauthorized"); // Jogosulatlan válasz
    }
}

void setup()
{
  // Soros port inicializálása
  Serial.begin(115200);
  delay(500);
  Serial.println();
  logMessage("Start");
  // SPIFFS inicializálása és konfiguráció betöltése
  loadConfig();

  configureSensors(configContent);
  intiHeatingSystem("/config.json"); 
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
      logMessage(".");
      attempt++;
    }
    if (WiFi.waitForConnectResult() == WL_CONNECTED)
    {
    }
  }

  if (config.wifiMode == "STA+AP" || config.wifiMode == "AP")
  {
    WiFi.softAP(config.APSSID, config.APPassword);
    logMessage("AP IP: %s", WiFi.softAPIP());
  }

  // HTTPS beállítása
  server.getServer().setRSACert(new X509List(cert), new PrivateKey(private_key));

  // API végpontok
  server.on("/heatingRequest", handleHeatingRequest);
  server.on("/logs", HTTP_GET, handleLogs);
  server.on("/configuration", HTTP_GET, handleConfiguration);
  server.on("/configuration", HTTP_POST, handleConfiguration);
  server.on("/upload", HTTP_POST, []()
            { server.send(200, "text/plain", "File Uploaded Successfully"); }, handleFileUpload);
  // Beállítjuk a reboot végpontot
  server.on("/reboot", HTTP_GET, handleReboot);
  const char *headerkeys[] = {"User-Agent", "Content-Type", "Request Headers"};
  size_t headerkeyssize = sizeof(headerkeys) / sizeof(char *);
  // ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize);

  // Webszerver indítása
  // MDNS.begin("kazan");
  server.begin();
  logMessage("HTTPS szerver indítva.");
  // MDNS.addService("https", "tcp", 443);

  // Szenzorok inicializálása
  setupSensors();
}

void loop()
{
  server.handleClient();
  timeClient.update();

  // 5 másodperc várakozás
  delay(1000);
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);
  logMessage("Temp: %f", temperature);
  // MDNS.update();
}
