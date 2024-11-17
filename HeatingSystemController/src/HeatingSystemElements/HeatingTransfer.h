#ifndef HEATINGTRANSFER_H
#define HEATINGTRANSFER_H

#include "HeatingTransfer.h"
#include "Sensor.h"
#include <vector>
#include "common.h"
#include "../comon.h"

// New class that stores the flow rate value.
class FlowRate
{
public:
    float value;

    FlowRate(float v) : value(v) {}
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
    );

private:
    // Helper function to calculate average temperature from a list of sensors
    float calculateAverageTemperature(const std::vector<Sensor *> &sensors);
};

#endif