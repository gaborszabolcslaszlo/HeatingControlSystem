#include "HeatingElement.h"

#include "../comon.h"
#include <cstdio>
#include <cstring>

HeatingElement::StateMap HeatingElement::ElementsStateMap;

void HeatingElement::setStateValue(const std::string &elementName,
                                   const std::string &stateName,
                                   const char *value)
{
    StateValue &storedValue = ElementsStateMap[elementName][stateName];
    if (value == nullptr)
    {
        storedValue[0] = '\0';
        return;
    }

    std::snprintf(storedValue.data(), storedValue.size(), "%s", value);
}

void HeatingElement::setStateValue(const std::string &elementName,
                                   const std::string &stateName,
                                   float value)
{
    StateValue &storedValue = ElementsStateMap[elementName][stateName];
    std::snprintf(storedValue.data(), storedValue.size(), "%.6f", value);
}

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
    else if (str == "SUNCOLLECTOR")
    {
        return HeatingElementType::PUFER;
    }
    else
    {
        return HeatingElementType::UNKNOWN; // Return UNKNOWN for unrecognized strings
    }
}

HeatingElement::HeatingElement(MessageBus &bus, const std::string name) : messageBus(bus), name(name), isActive(false), needHeating(false)
{
    messageBus.subscribe(name, [this](const std::string &msg)
                         { this->onMessageReceived(msg); });
}

void HeatingElement::activatePump(bool dischargePumpsOnly)
{
    for (Pump &pump : pumps)
    {
        if (dischargePumpsOnly == pump.dischargepump)
        {
            pump.enable();
        }
    }
}

void HeatingElement::deactivatePump(bool skipPid)
{
    for (Pump &pump : pumps)
    {
        // pump.setControlSignal(0); // Hívjuk meg a függvényt minden szenzorra
        pump.disable(skipPid);
    }
}

void HeatingElement::activate(bool dischargepump)
{
    // Serial.printf("Element active, name: %s \n", name);
    activatePump(dischargepump);
    isActive = true; // Aktiválás
}

void HeatingElement::deactivate()
{
    // Serial.printf("Element deactive, name: %s \n", name);
    deactivatePump();
    isActive = false; // Deaktiválás
}

void HeatingElement::cleanState()
{
    deactivatePump(true);
    isActive = false; // Deaktiválás
}

bool HeatingElement::getIsActive() const
{
    return isActive; // Visszaadja, hogy az elem aktív-e
}

void HeatingElement::addSensor(Sensor sensor)
{
    sensors.push_back(sensor);
}

void HeatingElement::addPump(Pump pump)
{
    pumps.push_back(pump);
}

void HeatingElement::addValve(Valve valve)
{
    valves.push_back(valve);
}

float HeatingElement::getTourTemperature()
{
    return calculateAverageTemperature(tourSensors);
}

float HeatingElement::getReTourTemperature()
{
    return calculateAverageTemperature(retourSensors);
}

float HeatingElement::getBodyTemperature()
{
    float f;
    if (bodySensors.size() > 0)
    {
        f = calculateAverageTemperature(bodySensors);
    }
    else
    {
        f = getTourTemperature();
    }
    // Serial.printf("body temp[C], name: %s, value: %f\n", name, f);
    return f;
}
// Function to classify sensors based on their position
void HeatingElement::classifySensors()
{
    // Serial.println("----- classify sensors modul:" + name + "-----");
    logDebug("----- classify sensors modul: %s ----- \n", name.c_str());
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
float HeatingElement::calculateAverageTemperature(const std::vector<Sensor *> &sensors)
{
    float sum = 0.0f;
    float weightedSum = 0.0f;
    int totalWeight = 0;
    size_t count = 0;

    for (const Sensor *sensor : sensors)
    {
        if (sensor)
        {                                                // Check if the sensor is not null
            float temperature = Sensor::getSensorTemperature(sensor);
            sum += temperature;
            if (sensor->level > 0)
            {
                weightedSum += temperature * sensor->level;
                totalWeight += sensor->level;
            }
            ++count;                                     // Increment the count of valid sensors
        }
    }

    if (totalWeight > 0)
    {
        return weightedSum / totalWeight;
    }
    return (count > 0) ? (sum / count) : 0.0f; // Return average temperature or 0.0f if no valid sensors
}

// Function to print sensors using Serial
void HeatingElement::printSensors(const std::vector<Sensor *> &sensors, const std::string &group)
{
    std::string s = "";
    if (sensors.size() > (std::size_t)1)
    {
        s = " sensors are redundant";
    }
    logDebug("        Sensors in %s group %s\n", group.c_str(), s.c_str());
    for (const auto &sensor : sensors)
    {
        logDebug("            Model: %s\n", sensor->model.c_str());

        logDebug("            ID%s\n", sensor->id.c_str());

        // Serial.println();
        logDebug("            Position: %s\n", positionToString(sensor->position).c_str());
    }
}

bool HeatingElement::validate()
{
    // Name must be present
    if (name.empty())
    {
        logError("Error: Heating element name is missing.\n");
        return false;
    }

    // Validate sensors if they exist
    for (auto &sensor : sensors)
    {
        if (!sensor.validate())
        {
            logError("Error: Invalid sensor in %s\n", name.c_str());
            return false;
        }
    }

    // Validate pumps if they exist
    for (auto &pump : pumps)
    {
        if (!pump.validate())
        {
            logError("Error: Invalid pump in %s\n", name.c_str());
            return false;
        }
    }

    // Validate valves if they exist
    for (auto &valve : valves)
    {
        if (!valve.validate())
        {
            logError("Error: Invalid valve in %s\n", name.c_str());
            return false;
        }
    }

    return true;
}

void HeatingElement::printHeatingElement()
{
    logDebug("-----Heating Element: %s\n", name.c_str());

    logDebug("-----Sensors:\n");
    for (auto &sensor : sensors)
    {
        sensor.print();
    }

    printSensors(tourSensors, positionToString(SensorPosition::TOUR));
    printSensors(retourSensors, positionToString(SensorPosition::RETOUR));
    printSensors(bodySensors, positionToString(SensorPosition::BODY));

    logDebug("------Pumps:\n");
    for (auto &pump : pumps)
    {
        pump.print();
    }

    logDebug("------Valves:\n");
    for (auto &valve : valves)
    {
        valve.print();
    }
}

void HeatingElement::update()
{
    for (Sensor &sensor : sensors)
    {
        sensor.update(); // Hívjuk meg a függvényt minden szenzorra
    }

    for (Pump &pump : pumps)
    {
        pump.update(); // Hívjuk meg a függvényt minden szenzorra
    }

    // Perform the calculation.
    heatTransfer.calculateHeatingTransferDirection(tourSensors, retourSensors, 1.0);

    setStateValue(name, "HT", this->heatTransfer.transferValue);
    setStateValue(name, "NH", this->needHeating ? "true" : "false");
    setStateValue(name, "iA", this->getIsActive() ? "true" : "false");
}

bool HeatingElement::canSupplyHeat(HeatingElement *element)
{
    double forwardTemp = getTourTemperature();           // Kazán előremenő hőmérséklete
    double returnTemp = element->getReTourTemperature(); // Elem visszatérő hőmérséklete

    double minTempDifference = 5.0; // Például, minimum 5 °C különbség szükséges

    // Ha a különbség az előremenő és visszatérő hőmérséklet között elég nagy, akkor még tud hőt biztosítani
    if ((forwardTemp - returnTemp) > minTempDifference)
    {
        // logInfo("Source %s can trnasfer heat to %s\n", name.c_str(), element->name.c_str());
        return true; // Kazán tud még hőt biztosítani
    }

    // logInfo("Source %s can't trnasfer heat to %s\n", name.c_str(), element->name.c_str());
    return false; // A hőmérsékletkülönbség túl kicsi, a fűtési elem már nem tud több hőt felvenni
}

bool HeatingElement::getNeedHeating() const { return needHeating; }
void HeatingElement::setNeedHeating(bool needHeating_) { needHeating = needHeating_; }

void HeatingElement::onMessageReceived(const std::string &message)
{
}
