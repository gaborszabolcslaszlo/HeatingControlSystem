// sensors.h

#ifndef TEMP_SENSORS_H
#define TEMP_SENSORS_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include "comon.h"

struct SensorConfig
{
  String id;
  int custom_id;
};

// Tömb a szenzor konfigurációk tárolására
SensorConfig sensorConfigs[10];
int sensorCount = 0;

// A OneWire adatvonal csatlakoztatva a GPIO2-re (D4)
#define ONE_WIRE_BUS 2

// OneWire objektum inicializálása
OneWire oneWire(ONE_WIRE_BUS);

// DallasTemperature könyvtár inicializálása a OneWire segítségével
DallasTemperature sensors(&oneWire);

// Maximum 10 szenzorra számítunk
DeviceAddress sensorAddresses[10];

void configureSensors(StaticJsonDocument<1024> &doc)
{
  logMessage("Start configure temperature sensors");
  // Szenzorok betöltése

  JsonArray sensorsArray = doc["sensors"];
  for (size_t i = 0; i < sensorsArray.size(); i++)
  {
    JsonVariant sensor = sensorsArray[i];
    String sensorId = sensor["id"].as<String>();
    int customId = sensor["custom_id"];
    logMessage("Temperature senzor: %s, -> %d", sensorId.c_str(), customId);
  }
}
// A szenzor címének kiírása (ROM Address)
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16)
      Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
  Serial.println();
}

// Szenzorok inicializálása és ellenőrzése
void setupSensors()
{
  // DallasTemperature inicializálása
  Serial.println("DS18B20 szenzorok keresése...");
  sensors.begin();

  // Keressük meg az összes csatlakoztatott DS18B20 szenzort
  sensorCount = sensors.getDeviceCount();

  if (sensorCount == 0)
  {
    Serial.println("No sensors found on the bus."); // Logoljuk a hibaüzenetet, ha nincs szenzor
  }
  else
  {
    // Kiírjuk a megtalált szenzorok számát
    Serial.print("Talált szenzorok száma: ");
    Serial.println(sensorCount);

    // A szenzorok címeit tároljuk
    for (int i = 0; i < sensorCount; i++)
    {
      if (sensors.getAddress(sensorAddresses[i], i))
      {
        Serial.print("Szenzor ");
        Serial.print(i);
        Serial.print(" címe: ");
        printAddress(sensorAddresses[i]);
      }
      else
      {
        Serial.println("Nem sikerült lekérdezni a szenzor címét.");
      }
    }
  }
}

#endif
