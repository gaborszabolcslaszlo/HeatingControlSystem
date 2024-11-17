#ifndef SENSOR_H
#define SENSOR_H

#include <string>
#include <map>
#include "common.h"
#include "../comon.h"

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
std::string positionToString(SensorPosition pos);

// Function to convert string to SensorPosition enum
SensorPosition stringToPosition(const std::string &positionStr);

// Sensor class
class Sensor
{
public:
    std::string model;
    SensorPosition position; // Change position type to enum
    std::string id;
    float temperature;
    static std::map<std::string, float> SensorsValue;
    float offset;

    Sensor();

    Sensor(const std::string &model, SensorPosition position, const std::string &id, float offset);

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
    std::string positionToString(SensorPosition pos) const;
};

#endif