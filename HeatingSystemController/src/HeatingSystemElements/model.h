#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>  // SPIFFS for file handling on ESP8266



// Print function for the entire heating system
void printHeatingSystemConfig();


// Sensor class
class Sensor {
public:
    String model;
    String id;

    Sensor() {}

    Sensor(String model, String id) {
        this->model = model;
        this->id = id;
    }

    bool validate() {
        if (model.isEmpty() || id.isEmpty()) {
            Serial.println("Error: Sensor model or ID is missing.");
            return false;
        }
        return true;
    }

    void print() {
        Serial.println("Sensor:");
        Serial.println("  Model: " + model);
        Serial.println("  ID: " + id);
    }
};

// Pump class
class Pump {
public:
    String name;
    String model;
    int maxControlSig;
    int minControlSig;
    String workingMode;

    Pump() {}

    Pump(String name, String model, int maxControlSig, int minControlSig, String workingMode) {
        this->name = name;
        this->model = model;
        this->maxControlSig = maxControlSig;
        this->minControlSig = minControlSig;
        this->workingMode = workingMode;
    }

    bool validate() {
        if (name.isEmpty() || model.isEmpty() || workingMode.isEmpty()) {
            Serial.println("Error: Pump name, model, or working mode is missing.");
            return false;
        }
        return true;
    }

    void print() {
        Serial.println("Pump:");
        Serial.println("  Name: " + name);
        Serial.println("  Model: " + model);
        Serial.println("  Max Control Signal: " + String(maxControlSig));
        Serial.println("  Min Control Signal: " + String(minControlSig));
        Serial.println("  Working Mode: " + workingMode);
    }
};

// Valve class
class Valve {
public:
    String name;
    int maxControlSig;
    int minControlSig;
    String workingMode;

    Valve() {}

    Valve(String name, int maxControlSig, int minControlSig, String workingMode) {
        this->name = name;
        this->maxControlSig = maxControlSig;
        this->minControlSig = minControlSig;
        this->workingMode = workingMode;
    }

    bool validate() {
        if (name.isEmpty()) {
            Serial.println("Error: Valve name is missing.");
            return false;
        }
        return true;
    }

    void print() {
        Serial.println("Valve:");
        Serial.println("  Name: " + name);
        Serial.println("  Max Control Signal: " + String(maxControlSig));
        Serial.println("  Min Control Signal: " + String(minControlSig));
        Serial.println("  Working Mode: " + workingMode);
    }
};

// HeatingElement class (for Kazan, Radiators, Puffer)
class HeatingElement {
public:
    String name;
    std::vector<Sensor> sensors;
    std::vector<Pump> pumps;
    std::vector<Valve> valves;

    HeatingElement() {}

    HeatingElement(String name) {
        this->name = name;
    }

    void addSensor(Sensor sensor) {
        sensors.push_back(sensor);
    }

    void addPump(Pump pump) {
        pumps.push_back(pump);
    }

    void addValve(Valve valve) {
        valves.push_back(valve);
    }

    bool validate() {
        // Name must be present
        if (name.isEmpty()) {
            Serial.println("Error: Heating element name is missing.");
            return false;
        }

        // Validate sensors if they exist
        for (auto& sensor : sensors) {
            if (!sensor.validate()) {
                Serial.println("Error: Invalid sensor in " + name);
                return false;
            }
        }

        // Validate pumps if they exist
        for (auto& pump : pumps) {
            if (!pump.validate()) {
                Serial.println("Error: Invalid pump in " + name);
                return false;
            }
        }

        // Validate valves if they exist
        for (auto& valve : valves) {
            if (!valve.validate()) {
                Serial.println("Error: Invalid valve in " + name);
                return false;
            }
        }

        return true;
    }

    void print() {
        Serial.println("Heating Element: " + name);

        Serial.println("  Sensors:");
        for (auto& sensor : sensors) {
            sensor.print();
        }

        Serial.println("  Pumps:");
        for (auto& pump : pumps) {
            pump.print();
        }

        Serial.println("  Valves:");
        for (auto& valve : valves) {
            valve.print();
        }
    }
};

// HeatingSystem class (contains the entire system)
class HeatingSystem {
public:
    std::vector<HeatingElement> kazan;
    std::vector<HeatingElement> radiators;
    std::vector<HeatingElement> puffer;

    HeatingSystem() {}

    void addKazan(HeatingElement element) {
        kazan.push_back(element);
    }

    void addRadiator(HeatingElement element) {
        radiators.push_back(element);
    }

    void addPuffer(HeatingElement element) {
        puffer.push_back(element);
    }

    bool validate() {
        // Validate Kazan elements
        if (kazan.empty()) {
            Serial.println("Error: Kazan element is missing in the configuration.");
            return false;
        }

        for (auto& element : kazan) {
            if (!element.validate()) return false;
        }

        // Validate Radiators (can be empty, but must validate if present)
        for (auto& element : radiators) {
            if (!element.validate()) return false;
        }

        // Validate Puffer (can be empty, but must validate if present)
        for (auto& element : puffer) {
            if (!element.validate()) return false;
        }

        return true;
    }

    void print() {
        Serial.println("Heating System:");

        Serial.println("  Kazan Elements:");
        for (auto& element : kazan) {
            element.print();
        }

        Serial.println("  Radiator Elements:");
        for (auto& element : radiators) {
            element.print();
        }

        Serial.println("  Puffer Elements:");
        for (auto& element : puffer) {
            element.print();
        }
    }
};

// Global heating system object
HeatingSystem heatingSystem;

// Load JSON configuration from file
void intiHeatingSystem(const char* filename) {
    File configFile = SPIFFS.open(filename, "r");
    if (!configFile) {
        Serial.println("Failed to open configuration file.");
        return;
    }

    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, configFile);
    if (error) {
        Serial.println("Error reading JSON configuration.");
        return;
    }

    // Read Kazan elements
    JsonArray kazanArray = doc["HeatingSystem"]["Kazan"];
    for (JsonObject element : kazanArray) {
        HeatingElement kazanElement(element["name"].as<String>());

        // Read sensors
        JsonArray sensors = element["sensors"];
        for (JsonObject sensor : sensors) {
            kazanElement.addSensor(Sensor(sensor["model"].as<String>(), sensor["id"].as<String>()));
        }

        // Read pumps
        JsonArray pumps = element["pumps"];
        for (JsonObject pump : pumps) {
            kazanElement.addPump(Pump(pump["name"].as<String>(), pump["model"].as<String>(), pump["maxControlSig"].as<int>(), pump["minControlSig"].as<int>(), pump["workingMode"].as<String>()));
        }

        // Read valves
        JsonArray valves = element["valves"];
        for (JsonObject valve : valves) {
            kazanElement.addValve(Valve(valve["name"].as<String>(), valve["maxControlSig"].as<int>(), valve["minControlSig"].as<int>(), valve["workingMode"].as<String>()));
        }

        heatingSystem.addKazan(kazanElement);
    }

    // Read Radiator elements
    JsonArray radiatorArray = doc["HeatingSystem"]["Radiators"];
    for (JsonObject element : radiatorArray) {
        HeatingElement radiatorElement(element["name"].as<String>());

        // Read sensors
        JsonArray sensors = element["sensors"];
        for (JsonObject sensor : sensors) {
            radiatorElement.addSensor(Sensor(sensor["model"].as<String>(), sensor["id"].as<String>()));
        }

        // Read pumps
        JsonArray pumps = element["pumps"];
        for (JsonObject pump : pumps) {
            radiatorElement.addPump(Pump(pump["name"].as<String>(), pump["model"].as<String>(), pump["maxControlSig"].as<int>(), pump["minControlSig"].as<int>(), pump["workingMode"].as<String>()));
        }

        // Read valves
        JsonArray valves = element["valves"];
        for (JsonObject valve : valves) {
            radiatorElement.addValve(Valve(valve["name"].as<String>(), valve["maxControlSig"].as<int>(), valve["minControlSig"].as<int>(), valve["workingMode"].as<String>()));
        }

        heatingSystem.addRadiator(radiatorElement);
    }

    // Read Puffer elements
    JsonArray pufferArray = doc["HeatingSystem"]["Puffer"];
    for (JsonObject element : pufferArray) {
        HeatingElement pufferElement(element["name"].as<String>());

        // Read sensors
        JsonArray sensors = element["sensors"];
        for (JsonObject sensor : sensors) {
            pufferElement.addSensor(Sensor(sensor["model"].as<String>(), sensor["id"].as<String>()));
        }

        // Read pumps
        JsonArray pumps = element["pumps"];
        for (JsonObject pump : pumps) {
            pufferElement.addPump(Pump(pump["name"].as<String>(), pump["model"].as<String>(), pump["maxControlSig"].as<int>(), pump["minControlSig"].as<int>(), pump["workingMode"].as<String>()));
        }

        // Read valves
        JsonArray valves = element["valves"];
        for (JsonObject valve : valves) {
            pufferElement.addValve(Valve(valve["name"].as<String>(), valve["maxControlSig"].as<int>(), valve["minControlSig"].as<int>(), valve["workingMode"].as<String>()));
        }

        heatingSystem.addPuffer(pufferElement);
    }

    // Validate the entire heating system
    if (!heatingSystem.validate()) {
        Serial.println("Error: Invalid heating system configuration.");
    } else {
        Serial.println("Heating system configuration loaded successfully.");
        printHeatingSystemConfig();
    }
}



// Print function for the entire heating system
void printHeatingSystemConfig() {
    Serial.println("----- Heating System Configuration -----");

    // Print Kazan elements
    if (!heatingSystem.kazan.empty()) {
        Serial.println("Kazan Elements:");
        for (auto& element : heatingSystem.kazan) {
            element.print();
        }
    } else {
        Serial.println("No Kazan elements present.");
    }

    // Print Radiator elements
    if (!heatingSystem.radiators.empty()) {
        Serial.println("Radiator Elements:");
        for (auto& element : heatingSystem.radiators) {
            element.print();
        }
    } else {
        Serial.println("No Radiator elements present.");
    }

    // Print Puffer elements
    if (!heatingSystem.puffer.empty()) {
        Serial.println("Puffer Elements:");
        for (auto& element : heatingSystem.puffer) {
            element.print();
        }
    } else {
        Serial.println("No Puffer elements present.");
    }

    Serial.println("----- End of Heating System Configuration -----");
}