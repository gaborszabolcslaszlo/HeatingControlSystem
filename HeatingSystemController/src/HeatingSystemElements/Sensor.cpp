#include "Sensor.h"

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
};

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
};

Sensor::Sensor() {}

Sensor::Sensor(const String &model, SensorPosition position, const String &id, float offset)
    : model(model), position(position), id(id), offset(offset)
{
}

void Sensor::print() const
{
    Serial.print("      Sensor: ");
    Serial.print("  Model: ");
    Serial.print(model);
    Serial.print(", Position: ");
    Serial.print(positionToString(position));
    Serial.print(", ID: ");
    Serial.println(id);
    if (offset != 0.0)
    {
        Serial.print(", Offset: ");
        Serial.println(offset);
    }
}

bool Sensor::validate() const
{
    if (model.isEmpty() || id.isEmpty() || position == SensorPosition::UNKNOWN)
    {
        Serial.println("Error: Sensor model, id, or position is invalid.");
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
    temperature = temp + offset;
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
    DeviceAddress addr;
    stringToDeviceAddress(id, addr);
    setTemperature(sensors.getTempC(addr));
    SensorsValue[id.c_str()] = getTemperature();
#endif
#endif
}

String Sensor::positionToString(SensorPosition pos) const
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
