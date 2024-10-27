#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h> // SPIFFS for file handling on ESP8266

// Print function for the entire heating system
void printHeatingSystemConfig();

enum class SensorPosition
{
    TOUR,
    RETOUR,
    BODY,
    UNKNOWN // Used for error handling
};

// Utility function to convert SensorPosition enum to string for Serial printing
String positionToString(SensorPosition pos)
{
    switch (pos)
    {
    case SensorPosition::TOUR:
        return "tour";
    case SensorPosition::RETOUR:
        return "retour";
    case SensorPosition::BODY:
        return "body";
    default:
        return "unknown";
    }
}

// Function to convert string to SensorPosition enum
SensorPosition stringToPosition(const String &positionStr)
{
    if (positionStr.equalsIgnoreCase("tour"))
        return SensorPosition::TOUR;
    if (positionStr.equalsIgnoreCase("retour"))
        return SensorPosition::RETOUR;
    if (positionStr.equalsIgnoreCase("body"))
        return SensorPosition::BODY;
    return SensorPosition::UNKNOWN; // Return Invalid for any other string
}

// Sensor class
class Sensor
{
public:
    String model;
    SensorPosition position; // Change position type to enum
    String id;

    Sensor(const String &model, SensorPosition position, const String &id)
        : model(model), position(position), id(id) {}

    void print() const
    {
        Serial.print("      Sensor: ");
        Serial.print("  Model: ");
        Serial.print(model);
        Serial.print(", Position: ");
        Serial.print(positionToString(position));
        Serial.print(", ID: ");
        Serial.println(id);
    }

    bool validate() const
    {
        if (model.isEmpty() || id.isEmpty() || position == SensorPosition::UNKNOWN)
        {
            Serial.println("Error: Sensor model, id, or position is invalid.");
            return false;
        }
        return true;
    }

private:
    String positionToString(SensorPosition pos) const
    {
        switch (pos)
        {
        case SensorPosition::TOUR:
            return "tour";
        case SensorPosition::RETOUR:
            return "retour";
        case SensorPosition::BODY:
            return "body";
        default:
            return "invalid";
        }
    }
};

// Pump class
class Pump
{
public:
    String name;
    String model;
    int maxControlSig;
    int minControlSig;
    String workingMode;

    Pump() {}

    Pump(String name, String model, int maxControlSig, int minControlSig, String workingMode)
    {
        this->name = name;
        this->model = model;
        this->maxControlSig = maxControlSig;
        this->minControlSig = minControlSig;
        this->workingMode = workingMode;
    }

    bool validate()
    {
        if (name.isEmpty() || model.isEmpty() || workingMode.isEmpty())
        {
            Serial.println("Error: Pump name, model, or working mode is missing.");
            return false;
        }
        return true;
    }

    void print()
    {
        Serial.print("      Pump:");
        Serial.print("  Name: " + name);
        Serial.print(", Model: " + model);
        Serial.print(", Max Control Signal: " + String(maxControlSig));
        Serial.print(", Min Control Signal: " + String(minControlSig));
        Serial.println(", Working Mode: " + workingMode);
    }
};

// Valve class
class Valve
{
public:
    String name;
    int maxControlSig;
    int minControlSig;
    String workingMode;

    Valve() {}

    Valve(String name, int maxControlSig, int minControlSig, String workingMode)
    {
        this->name = name;
        this->maxControlSig = maxControlSig;
        this->minControlSig = minControlSig;
        this->workingMode = workingMode;
    }

    bool validate()
    {
        if (name.isEmpty())
        {
            Serial.println("Error: Valve name is missing.");
            return false;
        }
        return true;
    }

    void print()
    {
        Serial.print("      Valve:");
        Serial.print("  Name: " + name);
        Serial.print(",  Max Control Signal: " + String(maxControlSig));
        Serial.print(",  Min Control Signal: " + String(minControlSig));
        Serial.println(",  Working Mode: " + workingMode);
    }
};

// HeatingElement class (for Kazan, Radiators, Puffer)
class HeatingElement
{
public:
    String name;
    std::vector<Sensor> sensors;
    std::vector<Pump> pumps;
    std::vector<Valve> valves;

    std::vector<Sensor *> tourSensors;
    std::vector<Sensor *> retourSensors;
    std::vector<Sensor *> bodySensors;

    int directionOfHeatTransfer = 0;

    HeatingElement() {}

    HeatingElement(String name)
    {
        this->name = name;
    }

    void addSensor(Sensor sensor)
    {
        sensors.push_back(sensor);
    }

    void addPump(Pump pump)
    {
        pumps.push_back(pump);
    }

    void addValve(Valve valve)
    {
        valves.push_back(valve);
    }

    // Function to classify sensors based on their position
    void classifySensors()
    {
        Serial.println("----- classify sensors modul:" + name + "-----");
        for (const auto &sensor : sensors)
        {
            if (sensor.position == SensorPosition::TOUR)
            {
                tourSensors.push_back(const_cast<Sensor *>(&sensor)); // Store reference to the original object
            }
            else if (sensor.position == SensorPosition::RETOUR)
            {
                retourSensors.push_back(const_cast<Sensor *>(&sensor)); // Store reference to the original object
            }
            else if (sensor.position == SensorPosition::BODY)
            {
                bodySensors.push_back(const_cast<Sensor *>(&sensor)); // Store reference to the original object
            }
        }
    }

    // Function to print sensors using Serial
    void printSensors(const std::vector<Sensor *> &sensors, const String &group)
    {   String s = "";
        if(sensors.size() > (std::size_t)1)
        {
            s = " sensors are redundant";
        }
        Serial.println("    Sensors in " + group + " group " + s);
        for (const auto &sensor : sensors)
        {
            Serial.print("          Model: ");
            Serial.print(sensor->model);
            Serial.print(", ID: ");
            Serial.print(sensor->id);
            Serial.print(", Position: ");
            Serial.println(positionToString(sensor->position));
        }
    }

    bool validate()
    {
        // Name must be present
        if (name.isEmpty())
        {
            Serial.println("Error: Heating element name is missing.");
            return false;
        }

        // Validate sensors if they exist
        for (auto &sensor : sensors)
        {
            if (!sensor.validate())
            {
                Serial.println("Error: Invalid sensor in " + name);
                return false;
            }
        }

        // Validate pumps if they exist
        for (auto &pump : pumps)
        {
            if (!pump.validate())
            {
                Serial.println("Error: Invalid pump in " + name);
                return false;
            }
        }

        // Validate valves if they exist
        for (auto &valve : valves)
        {
            if (!valve.validate())
            {
                Serial.println("Error: Invalid valve in " + name);
                return false;
            }
        }

        return true;
    }

    void printHeatingElement()
    {
        Serial.println("Heating Element: " + name);

        Serial.println("  Sensors:");
        for (auto &sensor : sensors)
        {
            sensor.print();
        }

        printSensors(tourSensors, positionToString(SensorPosition::TOUR));
        printSensors(retourSensors, positionToString(SensorPosition::RETOUR));
        printSensors(bodySensors, positionToString(SensorPosition::BODY));

        Serial.println("  Pumps:");
        for (auto &pump : pumps)
        {
            pump.print();
        }

        Serial.println("  Valves:");
        for (auto &valve : valves)
        {
            valve.print();
        }
    }
};

// HeatingSystem class (contains the entire system)
class HeatingSystem
{
public:
    std::vector<HeatingElement> kazan;
    std::vector<HeatingElement> radiators;
    std::vector<HeatingElement> puffer;

    HeatingSystem() {}

    void addKazan(HeatingElement element)
    {
        kazan.push_back(element);
    }

    void addRadiator(HeatingElement element)
    {
        radiators.push_back(element);
    }

    void addPuffer(HeatingElement element)
    {
        puffer.push_back(element);
    }

    bool validate()
    {
        // Validate Kazan elements
        if (kazan.empty())
        {
            Serial.println("Error: Kazan element is missing in the configuration.");
            return false;
        }

        for (auto &element : kazan)
        {
            if (!element.validate())
                return false;
        }

        // Validate Radiators (can be empty, but must validate if present)
        for (auto &element : radiators)
        {
            if (!element.validate())
                return false;
        }

        // Validate Puffer (can be empty, but must validate if present)
        for (auto &element : puffer)
        {
            if (!element.validate())
                return false;
        }

        return true;
    }

    void printHeatingSystem()
    {
        Serial.println("Heating System:");

        Serial.println("  Kazan Elements:");
        for (auto &element : kazan)
        {
            element.printHeatingElement();
        }

        Serial.println("  Radiator Elements:");
        for (auto &element : radiators)
        {
            element.printHeatingElement();
        }

        Serial.println("  Puffer Elements:");
        for (auto &element : puffer)
        {
            element.printHeatingElement();
        }
    }
};

// Global collection for heating elements
std::vector<HeatingElement> heatingSystemCollection;

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

            HeatingElement heatingElement(name);

            // Validate and load sensors
            JsonArray sensors = element["sensors"].as<JsonArray>();
            bool isSensorsValid = true;
            for (JsonObject sensor : sensors)
            {
                String model = sensor["model"];
                SensorPosition position = stringToPosition(sensor["position"]);
                String id = sensor["id"];

                Sensor newSensor(model, position, id);
                if (newSensor.validate())
                {
                    heatingElement.addSensor(newSensor);
                }
                else
                {
                    isSensorsValid = false;
                    Serial.println(F("Error: Invalid sensor configuration."));
                }
            }
            if (isSensorsValid)
            {
                heatingElement.classifySensors();
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

                Pump newPump(pumpName, model, maxControlSig, minControlSig, workingMode);
                heatingElement.addPump(newPump);
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
                heatingElement.addValve(newValve);
            }

            // Add heatingElement to your heating system collection
            heatingSystemCollection.push_back(heatingElement); // Store the heating element in the collection
            heatingElement.printHeatingElement();
        }
    }
}