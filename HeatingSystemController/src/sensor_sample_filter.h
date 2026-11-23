#pragma once

#include <cmath>

// Temperatures are raw sensor readings, before the configured sensor offset.
struct SensorSampleFilter
{
    enum Result { ACCEPTED, INVALID, OUT_OF_RANGE, SPIKE };

    bool hasMinimum = false;
    bool hasMaximum = false;
    float minimum = 0;
    float maximum = 0;
    // Disabled by default: a universal limit could hide a real rapid rise.
    float maxStep = 0;
    bool hasPrevious = false;
    float previous = 0;

    Result evaluate(float value)
    {
        if (!std::isfinite(value) || value == -127.0f || value < -55.0f || value > 125.0f)
            return INVALID;
        if ((hasMinimum && value < minimum) || (hasMaximum && value > maximum))
            return OUT_OF_RANGE;
        if (hasPrevious && maxStep > 0 && std::fabs(value - previous) > maxStep)
            return SPIKE;
        previous = value;
        hasPrevious = true;
        return ACCEPTED;
    }
};
