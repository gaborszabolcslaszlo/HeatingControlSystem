#ifndef MODEL_H
#define MODEL_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h> // SPIFFS for file handling on ESP8266
#include <string>
#include <map>

#include <temp_sensors.h>
#include "HeatingElement.h"
#include "MessageBus.h"

#include "Kazan.h"
#include "valve.h"
#include "pump.h"
#include "Sensor.h"
#include "Bojler.h"
#include "Puffer.h"
#include "Radiator.h"
#include "HeatingSystem.h"

HeatingSystem hsystem;
MessageBus messageBus;

// Print function for the entire heating system
void printHeatingSystemConfig();

// Load JSON configuration from file
void intiHeatingSystem(const char *filename)
{
    Serial.println("----- int Heating System -----");
    File configFile = SPIFFS.open(filename, "r");
    if (!configFile)
    {
        Serial.println("Failed to open configuration file.");
        return;
    }

    StaticJsonDocument<2048> doc; // Adjust size as necessary
    DeserializationError error = deserializeJson(doc, configFile);

    if (error)
    {
        Serial.print(F("Failed to read configuration. "));
        Serial.println(error.f_str());
        return;
    }

    JsonObject heatingSystem = doc["HeatingSystem"];

    // Check for mandatory components and validate each section
    for (JsonPair keyValue : heatingSystem)
    {
        const char *key = keyValue.key().c_str(); // Get the key
        JsonArray elements = heatingSystem[key].as<JsonArray>();

        for (JsonObject element : elements)
        {
            String name = element["name"];

            if (name.isEmpty())
            {
                Serial.println(F("Error: Name is mandatory for each element."));
                continue; // Skip this element if name is missing
            }

            HeatingElement *heatingElement;

            HeatingElementType type = elementTypeFromString(key);
            if (type == HeatingElementType::UNKNOWN)
            {
                Serial.printf("Unknown Heating System Element type: %s \n", key);
            }

            switch (type)
            {
            case HeatingElementType::KAZAN:
            {
                float retourTempProtValue = element["retourTempProtValue"];
                float tourTempProtValue = element["tourTempProtValue"];
                float activationThreshold = element["activationThreshold"];
                heatingElement = new Kazan(messageBus, name, retourTempProtValue, tourTempProtValue, activationThreshold);
                break;
            }
            case HeatingElementType::RADIATOR:
            {
                heatingElement = new Radiator(messageBus, name);
                break;
            }
            case HeatingElementType::PUFER:
            {
                heatingElement = new Puffer(messageBus, name);
                break;
            }
            default:
                break;
            }

            // Validate and load sensors
            JsonArray sensors = element["sensors"].as<JsonArray>();
            bool isSensorsValid = true;
            for (JsonObject sensor : sensors)
            {
                String model = sensor["model"];
                SensorPosition position = stringToPosition(sensor["position"]);
                String id = sensor["id"];
                float offset;
                if (sensor.containsKey("offset"))
                {
                    offset = sensor["offset"];
                }
                else
                {
                    offset = 0.0;
                }
                Sensor newSensor(model, position, id, offset);
                if (newSensor.validate())
                {
                    heatingElement->addSensor(newSensor);
                }
                else
                {
                    isSensorsValid = false;
                    Serial.println(F("Error: Invalid sensor configuration."));
                }
            }
            if (isSensorsValid)
            {
                heatingElement->classifySensors();
            }

            // Load pumps (name is mandatory)
            JsonArray pumps = element["pumps"].as<JsonArray>();
            for (JsonObject pump : pumps)
            {
                String pumpName = pump["name"];
                if (pumpName.isEmpty())
                {
                    Serial.println(F("Error: Name is mandatory for each pump."));
                    continue; // Skip this pump if name is missing
                }

                String model = pump["model"];
                int maxControlSig = pump["maxControlSig"];
                int minControlSig = pump["minControlSig"];
                String workingMode = pump["workingMode"];
                int IOnumber = pump["IOnumber"];

                Pump newPump(IOnumber, pumpName, model, maxControlSig, minControlSig, workingMode);
                heatingElement->addPump(newPump);
            }

            // Load valves (name is mandatory)
            JsonArray valves = element["valves"].as<JsonArray>();
            for (JsonObject valve : valves)
            {
                String valveName = valve["name"];
                if (valveName.isEmpty())
                {
                    Serial.println(F("Error: Name is mandatory for each valve."));
                    continue; // Skip this valve if name is missing
                }

                int maxControlSig = valve["maxControlSig"];
                int minControlSig = valve["minControlSig"];
                String workingMode = valve["workingMode"];

                Valve newValve(valveName, maxControlSig, minControlSig, workingMode);
                heatingElement->addValve(newValve);
            }

            // Add heatingElement to your heating system collection
            // hsystem.addHeatingElement(heatingElement, keyValue.key().c_str());
            switch (type)
            {
            case HeatingElementType::KAZAN:
                hsystem.addKazan(heatingElement);
                break;
            case HeatingElementType::RADIATOR:
                hsystem.addRadiator(heatingElement);
                break;
            case HeatingElementType::PUFER:
                hsystem.addPuffer(heatingElement);
                break;

            default:
                break;
            }

            // heatingSystemCollection.push_back(heatingElement); // Store the heating element in the collection
            // heatingElement->printHeatingElement();
        }

        hsystem.postInitTasks();
    }
}

void updateHeatingSystem()
{
#ifndef PIO_UNIT_TESTING
    sensors.requestTemperatures();
    Serial.printf(">");
#endif

    /* for (HeatingElement element : heatingSystemCollection)
     {
         element.update();
     }*/
}

#endif