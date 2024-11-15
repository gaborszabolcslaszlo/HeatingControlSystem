#ifndef SENSOR_H
#define SENSOR_H

#include <string>
#include <Arduino.h>
#include <map>

#ifdef PIO_UNIT_TESTING
extern std::map<std::string, float> mockSensorValues;
#endif

enum class SensorPosition
{
    TOUR,
    RETOUR,
    BODY,
    UNKNOWN // Used for error handling
};

// Utility function to convert SensorPosition enum to string for Serial printing
String positionToString(SensorPosition pos);

// Function to convert string to SensorPosition enum
SensorPosition stringToPosition(const String &positionStr);

// Sensor class
class Sensor
{
public:
    String model;
    SensorPosition position; // Change position type to enum
    String id;
    float temperature;
    static std::map<std::string, float> SensorsValue;
    float offset;

    Sensor();

    Sensor(const String &model, SensorPosition position, const String &id, float offset);

    void print() const;

    bool validate() const;

    // Getter for temperature.
    float getTemperature() const;

    // Static method to get temperature from a sensor
    static float getSensorTemperature(const Sensor *sensor);

    // Setter for temperature.
    void setTemperature(float temp);
#ifdef RNADOM_DATA
    float generateSineWave();
#endif

    void update();

private:
    String positionToString(SensorPosition pos) const;
};

#endif