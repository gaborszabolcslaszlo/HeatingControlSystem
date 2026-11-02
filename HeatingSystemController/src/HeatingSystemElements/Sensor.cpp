#include "Sensor.h"
#include <cstdio>

std::map<std::string, Sensor::Location> Sensor::LocationsById;

// Utility function to convert SensorPosition enum to string for Serial printing
std::string positionToString(SensorPosition pos)
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
};

// Function to convert string to SensorPosition enum
SensorPosition stringToPosition(const std::string &positionStr)
{
    if (positionStr == "tour")
        return SensorPosition::TOUR;
    if (positionStr == "retour")
        return SensorPosition::RETOUR;
    if (positionStr == "body")
        return SensorPosition::BODY;
    return SensorPosition::UNKNOWN; // Return Invalid for any other string
};

Sensor::Sensor()
    : temperature(0.0f), offset(0.0f), level(0)
{
}

Sensor::Sensor(const std::string &model, SensorPosition position, const std::string &id, float offset, int level)
    : model(model), position(position), id(id), offset(offset), level(level)
{
}

std::string Sensor::describeId(const std::string &id)
{
    auto location = LocationsById.find(id);
    if (location == LocationsById.end())
    {
        return "unknown sensor";
    }

    std::string description = location->second.elementName + "." +
                              ::positionToString(location->second.position);
    if (location->second.level != 0)
    {
        description += "(level=" + std::to_string(location->second.level) + ")";
    }
    return description;
}

void Sensor::describeId(const std::string &id, char *buffer, size_t bufferSize)
{
    if (buffer == nullptr || bufferSize == 0)
    {
        return;
    }

    auto location = LocationsById.find(id);
    if (location == LocationsById.end())
    {
        snprintf(buffer, bufferSize, "unknown sensor");
        return;
    }

    const char *position = "unknown";
    switch (location->second.position)
    {
    case SensorPosition::TOUR:
        position = "tour";
        break;
    case SensorPosition::RETOUR:
        position = "retour";
        break;
    case SensorPosition::BODY:
        position = "body";
        break;
    default:
        break;
    }
    if (location->second.level != 0)
    {
        snprintf(buffer, bufferSize, "%s.%s(level=%d)",
                 location->second.elementName.c_str(), position, location->second.level);
    }
    else
    {
        snprintf(buffer, bufferSize, "%s.%s",
                 location->second.elementName.c_str(), position);
    }
}

void Sensor::print() const
{
    logDebug("        Sensor: \n");
    logDebug("            Model: %s\n", model.c_str());
    logDebug("            Position: %s\n", positionToString(position).c_str());
    logDebug("            ID: %s\n", id.c_str());
    if (offset != 0.0)
    {
        logDebug("            Offset: %f\n", offset);
    }
    if (level != 0)
    {
        logDebug("            Level: %d\n", level);
    }
}

bool Sensor::validate() const
{
    if (model.empty() || id.empty() || position == SensorPosition::UNKNOWN)
    {
        logError("Error: Sensor model, id, or position is invalid.");
        return false;
    }
    return true;
}

// Getter for temperature.
float Sensor::getTemperature() const
{
    return temperature;
}

// Static method to get temperature from a sensor
float Sensor::getSensorTemperature(const Sensor *sensor)
{
    return sensor->getTemperature(); // Use getter to retrieve temperature
}

// Setter for temperature.
void Sensor::setTemperature(float temp)
{
    if (temp > -100.0) // noise filtering
    {
        temperature = temp + offset;
    }
}

#ifdef RNADOM_DATA
float Sensor::generateSineWave()
{
    static float angle = 0;

    // A szinuszos hullám kiszámítása
    float sineValue = sin(angle) * 100;

    // A következő szög kiszámítása
    angle += (2 * PI * 1) / 100;
    if (angle > 2 * PI)
    {
        angle -= 2 * PI; // Biztosítjuk, hogy az érték a 0 és 2π között maradjon
    }

    return sineValue;
}
#endif

void Sensor::update()
{
#ifdef PIO_UNIT_TESTING
    // If SENSORS_MOCK is defined, use the mock list

    float value = mockSensorValues[id.c_str()]; // Use the mock value
    // Serial.println("Mock sensor value: " + String(value));
    setTemperature(value);
    SensorsValue[id.c_str()] = getTemperature();
#else
#ifdef RNADOM_DATA
    float value = generateSineWave();
    setTemperature(value);
    SensorsValue[id.c_str()] = getTemperature();
#else
    if (SensorsValue.find(id) != SensorsValue.end())
    {
        setTemperature(SensorsValue[id]);
    }
    // SensorsValue[id.c_str()] = getTemperature();
#endif
#endif
}

std::string Sensor::positionToString(SensorPosition pos) const
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
