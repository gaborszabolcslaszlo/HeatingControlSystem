// sensors.h

#ifndef TEMP_SENSORS_H
#define TEMP_SENSORS_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include "comon.h"
#include "HeatingSystemElements/Sensor.h"
#include <iomanip> // Számformázáshoz szükséges
#include <sstream> // Stringstream használatas

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

std::string addressToString(DeviceAddress deviceAddress)
{
  std::stringstream result;
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16)
      result << "0"; // Ha kisebb mint 16, előtte nullát írunk
    result << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)deviceAddress[i];
  }
  return result.str();
}

// String cím konvertálása DeviceAddress típusú címre
bool stringToDeviceAddress(String address, DeviceAddress &deviceAddress)
{
  int i = 0;
  // A címben szereplő kötőjeleket eltávolítjuk
  address.replace("-", "");

  // Hexadecimális karakterek átalakítása byte-okba
  for (i = 0; i < 8; i++)
  {
    String byteString = address.substring(i * 2, i * 2 + 2);       // Két karakter egy byte-nak
    deviceAddress[i] = (byte)strtol(byteString.c_str(), NULL, 16); // Hex -> byte
  }
  return true;
}

void updateDsSensors()
{
#ifndef TESTINGOVER_API
  sensors.requestTemperatures();
  for (int i = 0; i < sensorCount; i++)
  {
    Sensor::SensorsValue[addressToString(sensorAddresses[i]).c_str()] = sensors.getTempC(sensorAddresses[i]);
  }
#endif
  /* for (HeatingElement element : heatingSystemCollection)
   {
       element.update();
   }*/
}

// Szenzorok inicializálása és ellenőrzése
void setupSensors()
{
  // DallasTemperature inicializálása
  Serial.println("DS18B20 szenzorok keresése...");
  sensors.begin();

  // Keressük meg az összes csatlakoztatott DS18B20 szenzort
  sensorCount = sensors.getDeviceCount();

  sensors.setResolution(9);

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
