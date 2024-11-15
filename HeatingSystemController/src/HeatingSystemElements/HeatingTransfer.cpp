#include "HeatingTransfer.h"
#include <strings.h>

// Calculate heating transfer based on the average temperature difference
void HeatingTransfer::calculateHeatingTransferDirection(
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

// Helper function to calculate average temperature from a list of sensors
float HeatingTransfer::calculateAverageTemperature(const std::vector<Sensor *> &sensors)
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
