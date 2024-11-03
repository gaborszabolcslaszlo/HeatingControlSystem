#include <ESP8266WiFi.h>
#include <ESP8266WebServerSecure.h>
#include <ESP8266mDNS.h>

#include <PCA9685.h>

#include "certificates.h"
#include "config.h"

#include "temp_sensors.h"
#include "HeatingSystemElements/model.h"

#include <GDBStub.h>

// Globális webszerver változó
ESP8266WebServerSecure server(443);
const int max_attempts = 10;
int attempt = 0;

// I2C címe a PCA9685-nek (alapértelmezett 0x40)
PCA9685 pwmController;

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

void writePwmIO(int id, int duty)
{
  pwmController.setChannelDutyCycle(id, duty * 4096 / 100);
}

void writeIO(int id, int value)
{
  if (value > 0)
  {
    pwmController.setChannelDutyCycle(id, 4096);
  }
  else
  {
    pwmController.setChannelDutyCycle(id, 0);
  }
}

void setup()
{
  // Soros port inicializálása
  Serial.begin(115200);
  delay(500);
  gdbstub_init();
  Serial.println();
  logMessage("Start");
  // SPIFFS inicializálása és konfiguráció betöltése
  loadConfig();

  Wire.begin(D2, D1); // SDA: D2, SCL: D1 (ESP8266)

  pwmController.setupSingleDevice(Wire, 0x40); // Az alapértelmezett I2C cím: 0x40
  pwmController.setToServoFrequency();         // Szervómotorokhoz beállított frekvencia

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

  hsystem.printHeatingSystem();
  // Szenzorok inicializálása
  setupSensors();
}

void loop()
{
  server.handleClient();
  timeClient.update();
  updateHeatingSystem();
  hsystem.update();

  // 5 másodperc várakozás
  delay(1000);

  // logMessage("Temp: %f", temperature);
  //  MDNS.update();
}
