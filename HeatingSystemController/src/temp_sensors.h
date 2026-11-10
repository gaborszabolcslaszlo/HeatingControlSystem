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

constexpr int MAX_DS18B20_SENSORS = 32;

// Tömb a szenzor konfigurációk tárolására
SensorConfig sensorConfigs[MAX_DS18B20_SENSORS];
int sensorCount = 0;
bool sensorConversionPending = false;
unsigned long sensorConversionStartedAt = 0;
int sensorReadIndex = 0;
constexpr unsigned long DS18B20_CONVERSION_TIME_MS = 100;

#ifdef ESP32
volatile float dsSensorTaskValues[MAX_DS18B20_SENSORS] = {};
volatile bool dsSensorTaskValuesValid[MAX_DS18B20_SENSORS] = {};
TaskHandle_t dsSensorTaskHandle = nullptr;
#endif

// A OneWire adatvonal panelfüggő; ESP32-WROOM esetén a panel D27 felirata.
#ifndef ONE_WIRE_BUS_PIN
#define ONE_WIRE_BUS_PIN D4
#endif
#define ONE_WIRE_BUS ONE_WIRE_BUS_PIN

// OneWire objektum inicializálása
OneWire oneWire(ONE_WIRE_BUS);

// DallasTemperature könyvtár inicializálása a OneWire segítségével
DallasTemperature sensors(&oneWire);

// Legfeljebb 32 szenzort tárolunk, hogy a busz aktuális eszközszáma
// ne írhassa felül a tömbök mögötti memóriát.
DeviceAddress sensorAddresses[MAX_DS18B20_SENSORS];
std::string sensorAddressStrings[MAX_DS18B20_SENSORS];

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
#ifdef ESP32
  // Az OneWire olvasás külön FreeRTOS taskban fut. A loopTask csak a kész,
  // egyszerű float értékeket másolja át, ezért egy lassú szenzor nem tudja
  // megakasztani a webkiszolgálót és a watchdogot.
  for (int i = 0; i < sensorCount; ++i)
  {
    if (dsSensorTaskValuesValid[i])
    {
      Sensor::SensorsValue[sensorAddressStrings[i]] = dsSensorTaskValues[i];
    }
  }
  return;
#else
  if (sensorConversionPending)
  {
    if (millis() - sensorConversionStartedAt < DS18B20_CONVERSION_TIME_MS)
    {
      return;
    }

    if (sensorReadIndex < sensorCount)
    {
      Sensor::SensorsValue[sensorAddressStrings[sensorReadIndex]] =
          sensors.getTempC(sensorAddresses[sensorReadIndex]);
      sensorReadIndex++;
    }

    // Szenzoronként csak egy buszolvasás történik egy loop-ciklusban.
    // A következő ciklus folytatja a kiolvasást, így nem blokkoljuk a loopTask-ot.
    if (sensorReadIndex >= sensorCount)
    {
      sensorConversionPending = false;
      sensorReadIndex = 0;
    }
    return;
  }

  sensors.requestTemperatures();
  sensorConversionStartedAt = millis();
  sensorReadIndex = 0;
  sensorConversionPending = true;
#endif
#endif
  /* for (HeatingElement element : heatingSystemCollection)
   {
       element.update();
   }*/
}

#ifdef ESP32
void ds18b20SensorTask(void *)
{
  for (;;)
  {
    if (sensorCount > 0)
    {
      sensors.requestTemperatures();
      vTaskDelay(pdMS_TO_TICKS(DS18B20_CONVERSION_TIME_MS));

      for (int i = 0; i < sensorCount; ++i)
      {
        const float value = sensors.getTempC(sensorAddresses[i]);
        if (value != DEVICE_DISCONNECTED_C)
        {
          dsSensorTaskValues[i] = value;
          dsSensorTaskValuesValid[i] = true;
        }
        vTaskDelay(1);
      }
    }

    // A szenzorok nem igényelnek folyamatos mintavételt; ez a szünet a WiFi
    // tasknak is biztosít futási időt.
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}
#endif

// Szenzorok inicializálása és ellenőrzése
void setupSensors()
{
#ifndef TESTINGOVER_API
  // DallasTemperature inicializálása
  logInfo("DS18B20 szenzorok keresése...\n");
  sensors.begin();

  // Keressük meg az összes csatlakoztatott DS18B20 szenzort
  const int detectedSensorCount = sensors.getDeviceCount();
  const int sensorsToScan = detectedSensorCount > MAX_DS18B20_SENSORS
                                ? MAX_DS18B20_SENSORS
                                : detectedSensorCount;

  if (detectedSensorCount > MAX_DS18B20_SENSORS)
  {
    logWarn("Tul sok DS18B20 szenzor (%d), csak az elso %d kerul feldolgozasra.\n",
            detectedSensorCount,
            MAX_DS18B20_SENSORS);
  }

  sensors.setResolution(9);
  sensors.setWaitForConversion(false);

  if (sensorsToScan == 0)
  {
    logError("No sensors found on the bus.\n");
  }
  else
  {
    sensorCount = 0;

    // Csak a sikeresen lekért címeket tároljuk. Így sikertelen címlekérés
    // esetén nem kerülhet üres kulcs ("") a Sensor::SensorsValue térképbe.
    for (int deviceIndex = 0; deviceIndex < sensorsToScan; deviceIndex++)
    {
      if (sensors.getAddress(sensorAddresses[sensorCount], deviceIndex))
      {
        // Az azonosító létrehozása csak egyszer történik; a loop nem allokál új stringet.
        sensorAddressStrings[sensorCount] = addressToString(sensorAddresses[sensorCount]);
        logDebug("Szenzor %d címe: %s\n",
                 sensorCount,
                 sensorAddressStrings[sensorCount].c_str());
        sensorCount++;
      }
      else
      {
        logWarn("Nem sikerült lekérdezni a %d. DS18B20 címét.\n", deviceIndex);
      }
    }

    logInfo("Érvényes DS18B20 szenzorok száma: %d\n", sensorCount);

#ifdef ESP32
    if (dsSensorTaskHandle == nullptr)
    {
      xTaskCreatePinnedToCore(
          ds18b20SensorTask,
          "ds18b20",
          4096,
          nullptr,
          1,
          &dsSensorTaskHandle,
          0);
    }
#endif
  }
#endif
}

#endif
