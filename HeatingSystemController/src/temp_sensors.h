// sensors.h

#ifndef TEMP_SENSORS_H
#define TEMP_SENSORS_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include "comon.h"
#include "HeatingSystemElements/Sensor.h"
#include "sensor_sample_filter.h"
#include "system_health.h"
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
int configuredSensorCount = 0;
volatile bool sensorRescanRequested = false;
bool sensorConversionPending = false;
unsigned long sensorConversionStartedAt = 0;
int sensorReadIndex = 0;
constexpr unsigned long DS18B20_CONVERSION_TIME_MS = 100;
constexpr unsigned long DS18B20_STARTUP_TIMEOUT_MS = 20000UL;
constexpr unsigned long DS18B20_STARTUP_RETRY_MS = 250UL;

#ifdef ESP32
volatile float dsSensorTaskValues[MAX_DS18B20_SENSORS] = {};
volatile bool dsSensorTaskValuesValid[MAX_DS18B20_SENSORS] = {};
SensorSampleFilter dsSensorFilters[MAX_DS18B20_SENSORS];
volatile int dsSensorResults[MAX_DS18B20_SENSORS] = {};
int dsSensorReportedResults[MAX_DS18B20_SENSORS] = {};
TaskHandle_t dsSensorTaskHandle = nullptr;
SemaphoreHandle_t dsSensorBusMutex = nullptr;
portMUX_TYPE dsSensorMux = portMUX_INITIALIZER_UNLOCKED;
constexpr unsigned long DS18B20_RESCAN_INTERVAL_MS = 30000UL;
#endif

std::string lastDsSensorScanIds[MAX_DS18B20_SENSORS];
int lastDsSensorScanCount = 0;
bool hasPreviousDsSensorScan = false;

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
    // A setw(2) + setfill('0') már pontosan két hexadecimális karakterre
    // tölti ki a bájtot; külön nulla hozzáadása három karakteres bájtokat
    // (pl. 0x00 -> "000") eredményezne.
    result << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
           << (int)deviceAddress[i];
  }
  return result.str();
}

bool isConfiguredSensorId(const std::string &id)
{
  for (JsonPair group : configContent["HeatingSystem"].as<JsonObject>())
    for (JsonObject element : group.value().as<JsonArray>())
      for (JsonObject configured : element["sensors"].as<JsonArray>())
        if (configured["id"].as<String>() == id.c_str())
          return true;
  return false;
}

bool containsDsSensorId(const std::string *ids, int count, const std::string &id)
{
  for (int i = 0; i < count; ++i)
    if (ids[i] == id)
      return true;
  return false;
}

void configureDsSensorFilter(int slot, const std::string &id)
{
#ifdef ESP32
  SensorSampleFilter &filter = dsSensorFilters[slot];
  filter = SensorSampleFilter();
  for (JsonPair group : configContent["HeatingSystem"].as<JsonObject>())
    for (JsonObject element : group.value().as<JsonArray>())
      for (JsonObject configured : element["sensors"].as<JsonArray>())
        if (configured["id"].as<String>() == id.c_str())
        {
          filter.hasMinimum = !configured["minTemp"].isNull();
          filter.hasMaximum = !configured["maxTemp"].isNull();
          if (filter.hasMinimum)
            filter.minimum = configured["minTemp"].as<float>();
          if (filter.hasMaximum)
            filter.maximum = configured["maxTemp"].as<float>();
          filter.maxStep = configured["maxStep"].isNull() ? 0 : configured["maxStep"].as<float>();
          return;
        }
#else
  (void)slot;
  (void)id;
#endif
}

void updatePreviousDsSensorScan()
{
  lastDsSensorScanCount = sensorCount;
  for (int i = 0; i < sensorCount; ++i)
    lastDsSensorScanIds[i] = sensorAddressStrings[i];
  hasPreviousDsSensorScan = true;
}

void handleDsSensorRescan()
{
#ifndef ESP32
  server.send(501, "application/json", "{\"error\":\"DS18B20 bus rescan is only available on ESP32\"}");
  return;
#else
  if (dsSensorBusMutex != nullptr)
    xSemaphoreTake(dsSensorBusMutex, portMAX_DELAY);

  std::string previousIds[MAX_DS18B20_SENSORS];
  const int previousCount = lastDsSensorScanCount;
  for (int i = 0; i < previousCount; ++i)
    previousIds[i] = lastDsSensorScanIds[i];
  const bool hadPreviousScan = hasPreviousDsSensorScan;

  sensors.begin();
  sensors.setResolution(9);
  sensors.setWaitForConversion(false);
  const int detected = sensors.getDeviceCount();
  const int scanCount = detected > MAX_DS18B20_SENSORS ? MAX_DS18B20_SENSORS : detected;

  DeviceAddress discoveredAddresses[MAX_DS18B20_SENSORS];
  std::string discoveredIds[MAX_DS18B20_SENSORS];
  int discoveredCount = 0;
  for (int deviceIndex = 0; deviceIndex < scanCount; ++deviceIndex)
  {
    DeviceAddress address;
    if (!sensors.getAddress(address, deviceIndex))
      continue;
    memcpy(discoveredAddresses[discoveredCount], address, sizeof(DeviceAddress));
    discoveredIds[discoveredCount] = addressToString(address);
    ++discoveredCount;
  }

  portENTER_CRITICAL(&dsSensorMux);
  sensorCount = discoveredCount;
  for (int i = 0; i < discoveredCount; ++i)
  {
    memcpy(sensorAddresses[i], discoveredAddresses[i], sizeof(DeviceAddress));
    sensorAddressStrings[i] = discoveredIds[i];
    configureDsSensorFilter(i, discoveredIds[i]);
    dsSensorTaskValuesValid[i] = false;
    dsSensorResults[i] = SensorSampleFilter::INVALID;
  }
  portEXIT_CRITICAL(&dsSensorMux);
  sensorRescanRequested = false;

  DynamicJsonDocument document(8192);
  JsonArray found = document.createNestedArray("found");
  JsonArray missingConfigured = document.createNestedArray("missingConfigured");
  JsonArray foundNotConfigured = document.createNestedArray("foundNotConfigured");
  JsonObject changed = document["changedSinceLastScan"].to<JsonObject>();
  JsonArray added = changed.createNestedArray("added");
  JsonArray removed = changed.createNestedArray("removed");

  std::string configuredIds[MAX_DS18B20_SENSORS];
  int configuredCount = 0;
  for (JsonPair group : configContent["HeatingSystem"].as<JsonObject>())
    for (JsonObject element : group.value().as<JsonArray>())
      for (JsonObject configured : element["sensors"].as<JsonArray>())
      {
        const std::string id = configured["id"].as<String>().c_str();
        if (configuredCount < MAX_DS18B20_SENSORS &&
            !containsDsSensorId(configuredIds, configuredCount, id))
          configuredIds[configuredCount++] = id;
      }

  for (int i = 0; i < discoveredCount; ++i)
  {
    found.add(discoveredIds[i].c_str());
    if (!containsDsSensorId(configuredIds, configuredCount, discoveredIds[i]))
      foundNotConfigured.add(discoveredIds[i].c_str());
    if (!hadPreviousScan || !containsDsSensorId(previousIds, previousCount, discoveredIds[i]))
      added.add(discoveredIds[i].c_str());
  }
  for (int i = 0; i < configuredCount; ++i)
    if (!containsDsSensorId(discoveredIds, discoveredCount, configuredIds[i]))
      missingConfigured.add(configuredIds[i].c_str());
  for (int i = 0; i < previousCount; ++i)
    if (!containsDsSensorId(discoveredIds, discoveredCount, previousIds[i]))
      removed.add(previousIds[i].c_str());

  changed["hasChanges"] = !hadPreviousScan || added.size() > 0 || removed.size() > 0;
  changed["firstScan"] = !hadPreviousScan;
  document["detectedCount"] = discoveredCount;
  document["configuredCount"] = configuredCount;
  document["timestampMs"] = millis();

  updatePreviousDsSensorScan();
  if (dsSensorBusMutex != nullptr)
    xSemaphoreGive(dsSensorBusMutex);

  String response;
  serializeJson(document, response);
  server.send(200, "application/json", response);
#endif
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
  portENTER_CRITICAL(&dsSensorMux);
  const int count = sensorCount;
  portEXIT_CRITICAL(&dsSensorMux);
  bool unavailable = dsSensorTaskHandle == nullptr;
  bool outOfRange = false;
  bool spike = false;
  for (int i = 0; i < count; ++i)
  {
    portENTER_CRITICAL(&dsSensorMux);
    const int result = dsSensorResults[i];
    const bool valid = dsSensorTaskValuesValid[i];
    const float value = dsSensorTaskValues[i];
    portEXIT_CRITICAL(&dsSensorMux);
    unavailable |= !valid || result == SensorSampleFilter::INVALID;
    outOfRange |= result == SensorSampleFilter::OUT_OF_RANGE;
    spike |= result == SensorSampleFilter::SPIKE;
    if (result != dsSensorReportedResults[i])
    {
      if (result == SensorSampleFilter::ACCEPTED)
        logInfo("DS18B20 %s: meres helyreallt.\n", sensorAddressStrings[i].c_str());
      else
        logWarn("DS18B20 %s: hibas meres (kod=%d: 1=olvasas, 2=tartomany, 3=kiugras).\n",
                sensorAddressStrings[i].c_str(), result);
      dsSensorReportedResults[i] = result;
    }
    if (valid)
    {
      Sensor::SensorsValue[sensorAddressStrings[i]] = value;
    }
  }
  for (const auto &configured : Sensor::LocationsById)
  {
    bool found = false;
    for (int i = 0; i < count; ++i)
      if (sensorAddressStrings[i] == configured.first)
      {
        found = true;
        break;
      }
    unavailable |= !found;
  }
  setHealthCondition(HEALTH_SENSOR_UNAVAILABLE, unavailable);
  setHealthCondition(HEALTH_SENSOR_RANGE, outOfRange);
  setHealthCondition(HEALTH_SENSOR_SPIKE, spike);
  static unsigned long lastMissingReport = 0;
  if (millis() - lastMissingReport >= DS18B20_RESCAN_INTERVAL_MS)
  {
    lastMissingReport = millis();
    for (const auto &configured : Sensor::LocationsById)
    {
      bool found = false;
      for (int i = 0; i < count; ++i)
        if (sensorAddressStrings[i] == configured.first)
        {
          found = true;
          break;
        }
      if (!found)
        logWarn("Konfiguralt DS18B20 nem talalhato: %s\n", configured.first.c_str());
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
  // Az induláskori setupSensors() már végzett egy keresést. Ha ott még nem
  // volt elérhető minden eszköz, a task azonnal próbálkozzon újra.
  unsigned long lastScan = millis() - DS18B20_RESCAN_INTERVAL_MS;
  for (;;)
  {
    const bool sensorsMissing = sensorCount < configuredSensorCount;
    if ((sensorRescanRequested || sensorsMissing) &&
        millis() - lastScan >= DS18B20_RESCAN_INTERVAL_MS)
    {
      if (dsSensorBusMutex != nullptr)
        xSemaphoreTake(dsSensorBusMutex, portMAX_DELAY);
      lastScan = millis();
      sensorRescanRequested = false;
      sensors.begin();
      sensors.setResolution(9);
      sensors.setWaitForConversion(false);
      const int detected = sensors.getDeviceCount();
      for (int deviceIndex = 0; deviceIndex < detected; ++deviceIndex)
      {
        DeviceAddress address;
        if (!sensors.getAddress(address, deviceIndex))
        {
          logWarn("DS18B20 cim nem olvashato, index=%d\n", deviceIndex);
          continue;
        }
        const std::string id = addressToString(address);
        bool known = false;
        for (int i = 0; i < sensorCount; ++i)
          if (sensorAddressStrings[i] == id)
          {
            known = true;
            break;
          }
        if (known)
          continue;
        if (sensorCount >= MAX_DS18B20_SENSORS)
        {
          logWarn("DS18B20 lista megtelt, nem adhato hozza: %s\n", id.c_str());
          break;
        }
        const int slot = sensorCount;
        memcpy(sensorAddresses[slot], address, sizeof(DeviceAddress));
        sensorAddressStrings[slot] = id;
        for (JsonPair group : configContent["HeatingSystem"].as<JsonObject>())
          for (JsonObject element : group.value().as<JsonArray>())
            for (JsonObject configured : element["sensors"].as<JsonArray>())
              if (configured["id"].as<String>() == id.c_str())
              {
                SensorSampleFilter &filter = dsSensorFilters[slot];
                filter.hasMinimum = !configured["minTemp"].isNull();
                filter.hasMaximum = !configured["maxTemp"].isNull();
                if (filter.hasMinimum)
                  filter.minimum = configured["minTemp"].as<float>();
                if (filter.hasMaximum)
                  filter.maximum = configured["maxTemp"].as<float>();
                filter.maxStep = configured["maxStep"].isNull() ? 0 : configured["maxStep"].as<float>();
              }
        portENTER_CRITICAL(&dsSensorMux);
        ++sensorCount;
        portEXIT_CRITICAL(&dsSensorMux);
        if (isConfiguredSensorId(id))
          logGreen("DS18B20 megtalalva: %s\n", id.c_str());
        else
          logWarn("DS18B20 megtalalva, de nincs a konfiguracioban: %s\n", id.c_str());
      }
      if (dsSensorBusMutex != nullptr)
        xSemaphoreGive(dsSensorBusMutex);
    }
    if (sensorCount > 0)
    {
      if (dsSensorBusMutex != nullptr)
        xSemaphoreTake(dsSensorBusMutex, portMAX_DELAY);
      sensors.requestTemperatures();
      vTaskDelay(pdMS_TO_TICKS(DS18B20_CONVERSION_TIME_MS));

      for (int i = 0; i < sensorCount; ++i)
      {
        const float value = sensors.getTempC(sensorAddresses[i]);
        const SensorSampleFilter::Result result = dsSensorFilters[i].evaluate(value);
        if (result != SensorSampleFilter::ACCEPTED)
          sensorRescanRequested = true;
        portENTER_CRITICAL(&dsSensorMux);
        dsSensorResults[i] = result;
        if (result == SensorSampleFilter::ACCEPTED)
        {
          dsSensorTaskValues[i] = value;
          dsSensorTaskValuesValid[i] = true;
        }
        portEXIT_CRITICAL(&dsSensorMux);
        vTaskDelay(1);
      }
      if (dsSensorBusMutex != nullptr)
        xSemaphoreGive(dsSensorBusMutex);
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

  // A szenzorok tápellátása és a OneWire busz felállása után kellhet néhány
  // próbálkozás, mire minden eszköz válaszol. A konfigurált darabszámig
  // várunk, de korlátozott ideig, hogy hibás bekötésnél ne álljon meg az
  // indulás végtelen időre.
  configuredSensorCount = 0;
  for (JsonPair group : configContent["HeatingSystem"].as<JsonObject>())
    for (JsonObject element : group.value().as<JsonArray>())
      configuredSensorCount += element["sensors"].as<JsonArray>().size();

  int detectedSensorCount = 0;
  const unsigned long startupDeadline = millis() + DS18B20_STARTUP_TIMEOUT_MS;
  do
  {
    sensors.begin();
    detectedSensorCount = sensors.getDeviceCount();
    if (detectedSensorCount >= configuredSensorCount ||
        millis() >= startupDeadline)
      break;
    delay(DS18B20_STARTUP_RETRY_MS);
  } while (true);

  const int sensorsToScan = detectedSensorCount > MAX_DS18B20_SENSORS
                                ? MAX_DS18B20_SENSORS
                                : detectedSensorCount;

  if (detectedSensorCount < configuredSensorCount)
  {
    logWarn("Indulaskor csak %d/%d DS18B20 szenzor erheto el.\n",
            detectedSensorCount,
            configuredSensorCount);
  }

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
#ifdef ESP32
        // Optional raw-temperature bounds; existing configurations remain valid.
        for (JsonPair group : configContent["HeatingSystem"].as<JsonObject>())
          for (JsonObject element : group.value().as<JsonArray>())
            for (JsonObject configured : element["sensors"].as<JsonArray>())
              if (configured["id"].as<String>() == sensorAddressStrings[sensorCount].c_str())
              {
                SensorSampleFilter &filter = dsSensorFilters[sensorCount];
                filter.hasMinimum = !configured["minTemp"].isNull();
                filter.hasMaximum = !configured["maxTemp"].isNull();
                if (filter.hasMinimum)
                  filter.minimum = configured["minTemp"].as<float>();
                if (filter.hasMaximum)
                  filter.maximum = configured["maxTemp"].as<float>();
                filter.maxStep = configured["maxStep"].isNull() ? 0 : configured["maxStep"].as<float>();
              }
#endif
        if (isConfiguredSensorId(sensorAddressStrings[sensorCount]))
          logGreen("Szenzor %d címe: %s\n",
                   sensorCount,
                   sensorAddressStrings[sensorCount].c_str());
        else
          logWarn("Szenzor megtalalva, de nincs a konfiguracioban: %s\n",
                  sensorAddressStrings[sensorCount].c_str());
        sensorCount++;
      }
      else
      {
        logWarn("Nem sikerült lekérdezni a %d. DS18B20 címét.\n", deviceIndex);
      }
    }

    logInfo("Érvényes DS18B20 szenzorok száma: %d\n", sensorCount);
  }
#ifdef ESP32
  // Ne indítsunk fölösleges új buszkeresést, ha az induláskori keresés
  // minden konfigurált szenzort megtalált. Hiányos keresés esetén a task
  // azonnal újrapróbálkozik.
  sensorRescanRequested = sensorCount < configuredSensorCount;
  if (dsSensorBusMutex == nullptr)
    dsSensorBusMutex = xSemaphoreCreateMutex();
  updatePreviousDsSensorScan();
  if (dsSensorTaskHandle == nullptr &&
      xTaskCreatePinnedToCore(ds18b20SensorTask, "ds18b20", 4096, nullptr, 1,
                              &dsSensorTaskHandle, 0) != pdPASS)
    logError("Nem sikerult elinditani a DS18B20 taskot.\n");
#endif
#endif
}

#endif
