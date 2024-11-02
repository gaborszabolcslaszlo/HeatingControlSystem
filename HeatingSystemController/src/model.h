#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h> // SPIFFS for file handling on ESP8266
#include <string>

extern void writePwmIO(int id, float duty);
extern void writeIO(int id, bool value);

// Message Bus class for broadcasting messages
class MessageBus
{
public:
    // Function to register a listener
    void subscribe(const String &listenerName, std::function<void(const std::string &)> callback)
    {
        listeners[listenerName.c_str()] = callback;
    }

    // Function to broadcast a message to all listeners
    void broadcast(const std::string &senderName, std::string message)
    {
        for (const auto &listener : listeners)
        {
            if (listener.first != senderName)
            { // Avoid sending the message back to the sender
                listener.second(message);
            }
        }
    }

private:
    std::unordered_map<std::string, std::function<void(const std::string &)>> listeners;
};

// Print function for the entire heating system
void printHeatingSystemConfig();

enum class PumpWorkingMode
{
    ONOFF,
    PWM,
    UNKNOWN
};

// "to string" függvény
std::string PumpWorkingModeToString(PumpWorkingMode mode)
{
    switch (mode)
    {
    case PumpWorkingMode::ONOFF:
        return "ONOFF";
    case PumpWorkingMode::PWM:
        return "PWM";
    case PumpWorkingMode::UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

// "from string" függvény
PumpWorkingMode PumpWorkingModeFromString(const std::string &modeStr)
{
    if (modeStr == "ON/OFF")
    {
        return PumpWorkingMode::ONOFF;
    }
    else if (modeStr == "PWM")
    {
        return PumpWorkingMode::PWM;
    }
    else
    {
        return PumpWorkingMode::UNKNOWN;
    }
}

enum class HeatingElementType
{
    KAZAN,
    RADIATOR,
    BOJLER,
    PUFER, // Used for error handling
    UNKNOWN
};
std::string elementTypeToString(HeatingElementType type)
{
    switch (type)
    {
    case HeatingElementType::KAZAN:
        return "KAZAN";
    case HeatingElementType::RADIATOR:
        return "RADIATOR";
    case HeatingElementType::BOJLER:
        return "BOJLER";
    case HeatingElementType::PUFER:
        return "PUFER";
    default:
        return "UNKNOWN";
    }
}

HeatingElementType elementTypeFromString(const std::string &str)
{
    if (str == "KAZAN")
    {
        return HeatingElementType::KAZAN;
    }
    else if (str == "RADIATOR")
    {
        return HeatingElementType::RADIATOR;
    }
    else if (str == "BOJLER")
    {
        return HeatingElementType::BOJLER;
    }
    else if (str == "PUFER")
    {
        return HeatingElementType::PUFER;
    }
    else
    {
        return HeatingElementType::UNKNOWN; // Return UNKNOWN for unrecognized strings
    }
}

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

// New class that stores the flow rate value.
class FlowRate
{
public:
    float value;

    FlowRate(float v) : value(v) {}
};

// Sensor class
class Sensor
{
public:
    String model;
    SensorPosition position; // Change position type to enum
    String id;
    float temperature;

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

    // Getter for temperature.
    float getTemperature() const
    {
        return temperature;
    }

    // Static method to get temperature from a sensor
    static float getSensorTemperature(const Sensor *sensor)
    {
        return sensor->getTemperature(); // Use getter to retrieve temperature
    }

    // Setter for temperature.
    void setTemperature(float temp)
    {
        temperature = temp;
    }

    void update()
    {
        setTemperature(sensors.getTempC(reinterpret_cast<const uint8_t *>(id.c_str())));
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
    PumpWorkingMode workingMode;
    int id;

    Pump() {}

    Pump(int id, String name, String model, int maxControlSig, int minControlSig, String workingMode)
    {
        this->id = id;
        this->name = name;
        this->model = model;
        this->maxControlSig = maxControlSig;
        this->minControlSig = minControlSig;
        this->workingMode = PumpWorkingModeFromString(workingMode.c_str());
    }

    int getControlSignal() const
    {
        return controlSignal;
    }

    // Setter metódus
    void setControlSignal(int value)
    {
        controlSignal = value;
    }

    bool validate()
    {
        if (name.isEmpty() || model.isEmpty() || workingMode != PumpWorkingMode::UNKNOWN)
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
        Serial.printf(", Working Mode: %s \n", PumpWorkingModeToString(workingMode).c_str());
    }

    void update()
    {
        if (workingMode == PumpWorkingMode::ONOFF)
        {
            writeIO(id, controlSignal);
        }
        if (workingMode == PumpWorkingMode::PWM)
        {
            writePwmIO(id, controlSignal);
        }
    }

private:
    int controlSignal = 0;
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

class HeatingTransfer
{
public:
    HeatingTransfer() {}

    float transferValue = 0;

    // Calculate heating transfer based on the average temperature difference
    void calculateHeatingTransferDirection(
        const std::vector<Sensor *> &sensorsA,
        const std::vector<Sensor *> &sensorsB,
        const FlowRate &flowRate // FlowRate is a separate class
    )
    {
        float avgTempA = calculateAverageTemperature(sensorsA);
        float avgTempB = calculateAverageTemperature(sensorsB);

        // Calculate the heating transfer based on the temperature difference and flow rate
        transferValue = (avgTempA - avgTempB) * flowRate.value;
    }

private:
    // Helper function to calculate average temperature from a list of sensors
    float calculateAverageTemperature(const std::vector<Sensor *> &sensors)
    {
        float sum = 0.0f;
        size_t count = 0;

        for (const Sensor *sensor : sensors)
        {
            if (sensor)
            {                                                // Check if the sensor is not null
                sum += Sensor::getSensorTemperature(sensor); // Use static method to get temperature
                ++count;                                     // Increment the count of valid sensors
            }
        }

        return (count > 0) ? (sum / count) : 0.0f; // Return average temperature or 0.0f if no valid sensors
    }
};

// HeatingElement class (for Kazan, Radiators, Puffer)
class HeatingElement
{
public:
    String name;
    bool isActive; // Állapotjelző, hogy az elem éppen aktív-e
    std::vector<Sensor> sensors;
    std::vector<Pump> pumps;
    std::vector<Valve> valves;

    std::vector<Sensor *> tourSensors;
    std::vector<Sensor *> retourSensors;
    std::vector<Sensor *> bodySensors;

    HeatingTransfer heatTransfer;

    HeatingElement(MessageBus &bus, const String name) : messageBus(bus), name(name), isActive(false), needHeating(false)
    {
        messageBus.subscribe(name, [this](const std::string &msg)
                             { this->onMessageReceived(msg); });
    }

    void activatePump()
    {
    }

    void deactivatePump()
    {
    }

    void activate()
    {
        isActive = true; // Aktiválás
    }

    void deactivate()
    {
        isActive = false; // Deaktiválás
    }

    bool getIsActive() const
    {
        return isActive; // Visszaadja, hogy az elem aktív-e
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

    float getTourTemperature()
    {
        return calculateAverageTemperature(tourSensors);
    }

    float getReTourTemperature()
    {
        return calculateAverageTemperature(retourSensors);
    }

    float getBodyTemperature()
    {
        if (bodySensors.size() > 0)
        {
            return calculateAverageTemperature(bodySensors);
        }
        else
        {
            return getTourTemperature();
        }
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

    // Helper function to calculate average temperature from a list of sensors
    float calculateAverageTemperature(const std::vector<Sensor *> &sensors)
    {
        float sum = 0.0f;
        size_t count = 0;

        for (const Sensor *sensor : sensors)
        {
            if (sensor)
            {                                                // Check if the sensor is not null
                sum += Sensor::getSensorTemperature(sensor); // Use static method to get temperature
                ++count;                                     // Increment the count of valid sensors
            }
        }

        return (count > 0) ? (sum / count) : 0.0f; // Return average temperature or 0.0f if no valid sensors
    }

    // Function to print sensors using Serial
    void printSensors(const std::vector<Sensor *> &sensors, const String &group)
    {
        String s = "";
        if (sensors.size() > (std::size_t)1)
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

    void update()
    {
        for (Sensor &sensor : sensors)
        {
            sensor.update(); // Hívjuk meg a függvényt minden szenzorra
        }

        // Perform the calculation.
        heatTransfer.calculateHeatingTransferDirection(tourSensors, retourSensors, 1.0);
    }

    virtual bool canSupplyHeat(HeatingElement *element)
    {
        double forwardTemp = getTourTemperature();     // Kazán előremenő hőmérséklete
        double returnTemp = element->getReTourTemperature(); // Elem visszatérő hőmérséklete

        double minTempDifference = 5.0; // Például, minimum 5 °C különbség szükséges

        // Ha a különbség az előremenő és visszatérő hőmérséklet között elég nagy, akkor még tud hőt biztosítani
        if ((forwardTemp - returnTemp) > minTempDifference)
        {
            return true; // Kazán tud még hőt biztosítani
        }

        return false; // A hőmérsékletkülönbség túl kicsi, a fűtési elem már nem tud több hőt felvenni
    }

    bool needsHeating()
    {
        return needHeating;
    }

private:
    bool needHeating;
    MessageBus &messageBus;
    virtual void onMessageReceived(const std::string &message)
    {
    }
};

class Kazan : public HeatingElement
{
public:
    Kazan(MessageBus &bus, String name, float retourTempProtValue, float tourTempProtValue) : HeatingElement(bus, name.c_str())
    {
        Serial.println("Kazán Obiektum!!!!");
    }

    void checkRetourLowTemperatureProtection()
    {
        float tourTemp = getTourTemperature();
        float retourTemp = getReTourTemperature();
        if (tourTemp > retourTempProtValue)
        {
            Serial.println("Kazán tulmelegedes védelem aktiválva!");
        }
        if (retourTemp < retourTempProtValue)
        {
            Serial.println("Kazán retour védelem aktiválva!");
        }
    }

    void onMessageReceived(const std::string &message) override
    {
        Serial.println(message.c_str());
    }

private:
    float retourTempProtValue;
    float tourTempProtValue;
};

class Puffer : public HeatingElement
{
public:
    Puffer(MessageBus &bus, String name) : HeatingElement(bus, name.c_str()) {}

    // Puffer egyedi vezérlése
    void moveEnergyToRadiators(float radiatorRequestTemp)
    {
    }

    bool hasStoredEnergy()
    {
        return getBodyTemperature() > 30; // Van tárolt energia, ha a hőmérséklet magasabb, mint a minimum
    }

    bool canSupplyHeat(HeatingElement *element) override
    {
        double bodyTemp = this->getBodyTemperature();        // Kazán előremenő hőmérséklete
        double returnTemp = element->getReTourTemperature(); // Elem visszatérő hőmérséklete

        double minTempDifference = 5.0; // Például, minimum 5 °C különbség szükséges

        // Ha a különbség az előremenő és visszatérő hőmérséklet között elég nagy, akkor még tud hőt biztosítani
        if ((bodyTemp - returnTemp) > minTempDifference)
        {
            return true; // Kazán tud még hőt biztosítani
        }

        return false; // A hőmérsékletkülönbség túl kicsi, a fűtési elem már nem tud több hőt felvenni
    }
};

class Bojler : public HeatingElement
{
public:
    Bojler(MessageBus &bus, String name) : HeatingElement(bus, name.c_str()) {}

    // Bojler vezérlése melegvíz igény esetén
    void manageHotWater(float hotWaterDemandTemp)
    {
    }
};

class Radiator : public HeatingElement
{
public:
    Radiator(MessageBus &bus, String name) : HeatingElement(bus, name.c_str()) {}

    // Radiátor egyedi vezérlése
    void manageHeating(float desiredTemp)
    {
    }
};

// HeatingSystem class (contains the entire system)
class HeatingSystem
{
public:
    std::vector<HeatingElement *> kazan;
    std::vector<HeatingElement *> radiators;
    std::vector<HeatingElement *> puffer;
    std::vector<HeatingElement *> mergedList;

    std::vector<HeatingElement *> heatingPriorityList;

    HeatingSystem() {}

    void postInitTasks()
    {
    }

    void addKazan(HeatingElement *element)
    {
        kazan.push_back(element);
        mergedList.push_back(element);
    }

    void addRadiator(HeatingElement *element)
    {
        radiators.push_back(element);
        mergedList.push_back(element);
        heatingPriorityList.push_back(element);
    }

    void addPuffer(HeatingElement *element)
    {
        puffer.push_back(element);
        mergedList.push_back(element);
        heatingPriorityList.push_back(element);
    }

    void addHeatingElement(HeatingElement *element, std::string typeString)
    {
        HeatingElementType type = elementTypeFromString(typeString);
        if (type == HeatingElementType::UNKNOWN)
        {
            Serial.printf("Unknown Heating System Element type: %s \n", typeString.c_str());
        }
        switch (type)
        {
        case HeatingElementType::KAZAN:
            addKazan(element);
            break;
        case HeatingElementType::RADIATOR:
            addRadiator(element);
            break;
        case HeatingElementType::PUFER:
            addPuffer(element);
            break;

        default:
            break;
        }
    }

    bool validate()
    {
        // Validate Kazan elements
        if (kazan.empty())
        {
            Serial.println("Error: Kazan element is missing in the configuration.");
            return false;
        }

        for (auto &element : mergedList)
        {
            if (!element->validate())
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
            element->printHeatingElement();
        }

        Serial.println("  Radiator Elements:");
        for (auto &element : radiators)
        {
            element->printHeatingElement();
        }

        Serial.println("  Puffer Elements:");
        for (auto &element : puffer)
        {
            element->printHeatingElement();
        }
    }

    void update()
    {
        for (HeatingElement *element : mergedList)
        {
            element->update();
        }
    }

    void controlHeatingSystem(HeatingElement *kazan)
    {
        for (HeatingElement *elem : heatingPriorityList)
        {
            if (kazan->getIsActive())
            {
                if (kazan->canSupplyHeat(elem) && elem->needsHeating())
                {
                    elem->activatePump();
                }
                else
                {
                    continue; // Tovább lép a következő elemre
                }
            }
            else
            {
                for (HeatingElement *actPuffer : puffer)
                {
                    Puffer *pufferPtr = static_cast<Puffer *>(actPuffer);
                    if (pufferPtr->hasStoredEnergy() && elem->needsHeating() && pufferPtr->canSupplyHeat(elem) )
                    {
                        elem->activatePump();
                        pufferPtr->activatePump();
                    }
                }
            }
        }
    }
};

// Global collection for heating elements
// std::vector<HeatingElement> heatingSystemCollection;

HeatingSystem hsystem;
MessageBus messageBus;

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
                heatingElement = new Kazan(messageBus, name, retourTempProtValue, tourTempProtValue);
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

                Sensor newSensor(model, position, id);
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
            heatingElement->printHeatingElement();
        }

        hsystem.postInitTasks();
    }
}

void updateHeatingSystem()
{
    sensors.requestTemperatures();

    /* for (HeatingElement element : heatingSystemCollection)
     {
         element.update();
     }*/
}