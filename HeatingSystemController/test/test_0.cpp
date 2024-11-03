#include <Arduino.h>
#include <unity.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServerSecure.h>
#include <ESP8266mDNS.h>

#include <PCA9685.h>

#include "certificates.h"
#include "config.h"
#include "comon.h"
#include "HeatingSystemElements/model.h"

#define SENSORS_MOCK

std::map<int, float> mockOutIO = {};
std::map<std::string, float> mockSensorValues = {};

void printOutIOValues(const std::map<int, float> &outIOValues)
{
    String output = ""; // Ide gyűjtjük a kimenetet egy stringbe

    for (const auto &pair : outIOValues)
    {
        output += String(pair.first);     // IO Port ID
        output += " -> ";                 // Elválasztó
        output += String(pair.second, 2); // Value (2 tizedesjegy)
        output += "; ";                   // Elválasztó a portok között
    }

    // Végső kimenet egyszerre
    Serial.println(output);
}

void printMockSensorValues(const std::map<std::string, float> &sensorValues)
{
    String output = ""; // Ide gyűjtjük a kimenetet egy stringbe

    for (const auto &pair : sensorValues)
    {
        output += pair.first.c_str();     // Sensor ID
        output += " -> ";                 // Separator
        output += String(pair.second, 2); // Temperature (2 decimal places)
        output += "; ";                   // Separator between sensors
    }

    // Végső kimenet egyszerre
    Serial.println(output);
}

void printInputOutputState()
{
    printMockSensorValues(mockSensorValues);
    printOutIOValues(mockOutIO);
}

void writePwmIO(int id, int duty)
{
    // Serial.printf("writePwmIO id: %d \n", id);
    if (duty > 0)
    {
        mockOutIO[id] = true;
    }
    else
    {
        mockOutIO[id] = false;
    }
}

void writeIO(int id, int value)
{
    // Serial.printf("writeIO id: %d value %d \n", id, value);
    mockOutIO[id] = value > 0 ? true : false;
}

// Tesztelés előtti beállítások
void setUp()
{
    // Inicializációs kód a tesztek előtt
}

// Tesztelés utáni takarítás
void tearDown()
{
    // Takarítás a tesztek után
}

// Egy egyszerű teszt
void test_led_builtin_pin_number()
{
    TEST_ASSERT_EQUAL(2, LED_BUILTIN); // Ellenőrzi, hogy az LED_BUILTIN a helyes érték-e
}

// Teszt, amely ellenőrzi, hogy a szivattyú bekapcsol-e, ha a kazán hőmérséklete eléri a küszöbértéket
void pump_starts_when_heatsource_temp_exceeds_threshold()
{

    /*std::vector<Sensor> bodySensors = std::find_if(hsystem.kazan[0]->sensors.begin(),hsystem.kazan[0]->sensors.end(), [](const Sensor& sensor) {
            return sensor.position == SensorPosition::BODY; // Szűrés feltétele
        });*/
    mockSensorValues = {
        {"K0Body0", 20}, {"K0Tour0", 20}, {"K0Retour0", 20}, {"R0Tour0", 20}, {"R0Retour0", 20}, {"P0Body0", 20}, {"P0Tour0", 20}, {"R0Retour0", 20}};

    // SPIFFS inicializálása és konfiguráció betöltése
    loadConfig();
    intiHeatingSystem("/test_config.json");

    TEST_MESSAGE("---------------Step 0--------------");
    hsystem.update();
    printInputOutputState();
    TEST_MESSAGE("Check if the heat source pump is off");
    TEST_ASSERT_FALSE(mockOutIO[1]);

    TEST_MESSAGE("---------------Step 1--------------");
    TEST_MESSAGE("Raising temperature body 1");
    mockSensorValues["K0Body0"] = 40.1;
    hsystem.update();
    printInputOutputState();
    TEST_MESSAGE("Check if the heat source pump is on");
    TEST_ASSERT_TRUE(mockOutIO[1]);

    TEST_MESSAGE("---------------Step 2--------------");
    TEST_MESSAGE("Decrese temperature body 1");
    mockSensorValues["K0Body0"] = 20.1;
    hsystem.update();
    printInputOutputState();
    TEST_MESSAGE("Check if the heat source pump is off");
    TEST_ASSERT_FALSE(mockOutIO[1]);
}

void heatsource_retour_protection()
{

    /*std::vector<Sensor> bodySensors = std::find_if(hsystem.kazan[0]->sensors.begin(),hsystem.kazan[0]->sensors.end(), [](const Sensor& sensor) {
            return sensor.position == SensorPosition::BODY; // Szűrés feltétele
        });*/
    mockSensorValues = {
        {"K0Body0", 20}, {"K0Tour0", 20}, {"K0Retour0", 20}, {"R0Tour0", 20}, {"R0Retour0", 20}, {"P0Body0", 30}, {"P0Tour0", 20}, {"R0Retour0", 20}};

    // SPIFFS inicializálása és konfiguráció betöltése
    loadConfig();
    intiHeatingSystem("/test_config.json");

    hsystem.radiators[0]->setNeedHeating(true);
    hsystem.puffer[0]->setNeedHeating(true);

    TEST_MESSAGE("---------------Step 0--------------");
    hsystem.update();
    printInputOutputState();
    TEST_MESSAGE("Check if the heat source pump is off");
    TEST_ASSERT_FALSE(mockOutIO[1]);
    TEST_ASSERT_FALSE(mockOutIO[3]);
    TEST_ASSERT_FALSE(mockOutIO[5]);

    TEST_MESSAGE("---------------Step 1--------------");
    TEST_MESSAGE("Raising temperature body 1");
    mockSensorValues["K0Body0"] = 50.1;
    mockSensorValues["K0Tour0"] = 55.1;
    mockSensorValues["K0Retour0"] = 25.1;
    hsystem.update();
    printInputOutputState();
    TEST_MESSAGE("Check if the heat source pump is on, and other pump is off retour protection mode");
    TEST_ASSERT_TRUE(mockOutIO[1]);
    TEST_ASSERT_FALSE(mockOutIO[3]);
    TEST_ASSERT_FALSE(mockOutIO[5]);

    TEST_MESSAGE("---------------Step 2--------------");
    TEST_MESSAGE("Increse temperature retour of heat source, heating mode ");
    mockSensorValues["K0Retour0"] = 45.3;
    hsystem.update();
    printInputOutputState();
    TEST_MESSAGE("Check if the radiator and puffer pumps is on");
    TEST_ASSERT_TRUE(mockOutIO[1]);
    TEST_ASSERT_TRUE(mockOutIO[3]);
    TEST_ASSERT_TRUE(mockOutIO[5]);

    TEST_MESSAGE("---------------Step 3--------------");
    TEST_MESSAGE("Decrese temperature retour of heat source, retur protecting mode ");
    mockSensorValues["K0Retour0"] = 25.3;
    hsystem.update();
    printInputOutputState();
    TEST_MESSAGE("Check if the radiator and puffer pumps is off");
    TEST_ASSERT_TRUE(mockOutIO[1]);
    TEST_ASSERT_FALSE(mockOutIO[3]);
    TEST_ASSERT_FALSE(mockOutIO[5]);

    TEST_MESSAGE("---------------Step 4--------------");
    TEST_MESSAGE("Deactivate heat source, puffer heatin mode ");
    mockSensorValues["K0Retour0"] = 40;
    mockSensorValues["K0Tour0"] = 30;
    mockSensorValues["P0Body0"] = 70;
    mockSensorValues["P0Tour0"] = 70;
    mockSensorValues["P0Retour0"] = 65;
    hsystem.update();
    printInputOutputState();
    TEST_MESSAGE("Check if the radiator and puffer pumps is off");
    TEST_ASSERT_FALSE(mockOutIO[1]);
    TEST_ASSERT_TRUE(mockOutIO[3]);
    TEST_ASSERT_TRUE(mockOutIO[5]);

    TEST_MESSAGE("---------------Step 5--------------");
    TEST_MESSAGE("puffer cant supply heat ");
    mockSensorValues["R0Retour0"] = 40;
    mockSensorValues["R0Retour0"] = 40;
    mockSensorValues["P0Body0"] = 29;
    mockSensorValues["P0Tour0"] = 29;
    mockSensorValues["P0Retour0"] = 48;
    hsystem.update();
    printInputOutputState();
    TEST_MESSAGE("Check if the radiator and puffer pumps is off");
    TEST_ASSERT_FALSE(mockOutIO[1]);
    TEST_ASSERT_FALSE(mockOutIO[3]);
    TEST_ASSERT_FALSE(mockOutIO[5]);

    TEST_MESSAGE("---------------Step 6--------------");
    TEST_MESSAGE("kazan rocket test");
    mockSensorValues["K0Body0"] = 99;
    mockSensorValues["K0Tour0"] = 96;
    hsystem.update();
    printInputOutputState();
    TEST_MESSAGE("Check if all pumps is on");
    TEST_ASSERT_TRUE(mockOutIO[1]);
    TEST_ASSERT_TRUE(mockOutIO[3]);
    TEST_ASSERT_TRUE(mockOutIO[5]);
}

void setup()
{
    // Tesztelés indítása
    UNITY_BEGIN();

    // Hívjuk meg a teszt funkciókat
    RUN_TEST(test_led_builtin_pin_number);

    // RUN_TEST(pump_starts_when_heatsource_temp_exceeds_threshold);
    RUN_TEST(heatsource_retour_protection);
    // Tesztek lezárása
    UNITY_END();
}

void loop()
{
    // Az ESP8266 folyamatosan futtatja a loop-ot, de a tesztek csak egyszer futnak le
}
