#include "HeatingElement.h"

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

HeatingElement::HeatingElement(MessageBus &bus, const String name) : messageBus(bus), name(name), isActive(false), needHeating(false)
{
    messageBus.subscribe(name, [this](const std::string &msg)
                         { this->onMessageReceived(msg); });
}

void HeatingElement::activatePump()
{
    for (Pump &pump : pumps)
    {
        pump.setControlSignal(100); // Hívjuk meg a függvényt minden szenzorra
    }
}

void HeatingElement::deactivatePump()
{
    for (Pump &pump : pumps)
    {
        pump.setControlSignal(0); // Hívjuk meg a függvényt minden szenzorra
    }
}

void HeatingElement::activate()
{
    // Serial.printf("Element active, name: %s \n", name);
    activatePump();
    isActive = true; // Aktiválás
}

void HeatingElement::deactivate()
{
    // Serial.printf("Element deactive, name: %s \n", name);
    deactivatePump();
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
float HeatingElement::calculateAverageTemperature(const std::vector<Sensor *> &sensors)
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
void HeatingElement::printSensors(const std::vector<Sensor *> &sensors, const String &group)
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

bool HeatingElement::validate()
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

void HeatingElement::printHeatingElement()
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
}

bool HeatingElement::canSupplyHeat(HeatingElement *element)
{
    double forwardTemp = getTourTemperature();           // Kazán előremenő hőmérséklete
    double returnTemp = element->getReTourTemperature(); // Elem visszatérő hőmérséklete

    double minTempDifference = 5.0; // Például, minimum 5 °C különbség szükséges

    // Ha a különbség az előremenő és visszatérő hőmérséklet között elég nagy, akkor még tud hőt biztosítani
    if ((forwardTemp - returnTemp) > minTempDifference)
    {
        Serial.printf("Source %s can trnasfer heat to %s", name, element->name);
        return true; // Kazán tud még hőt biztosítani
    }

    Serial.printf("Source %s can't trnasfer heat to %s", name, element->name);
    return false; // A hőmérsékletkülönbség túl kicsi, a fűtési elem már nem tud több hőt felvenni
}

bool HeatingElement::getNeedHeating() const { return needHeating; }
void HeatingElement::setNeedHeating(bool needHeating_) { needHeating = needHeating_; }

void HeatingElement::onMessageReceived(const std::string &message)
{
}
