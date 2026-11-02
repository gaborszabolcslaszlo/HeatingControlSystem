
#include "pid.h"
#include <Arduino.h>

PID::PID() {}

PID::PID(float kp, float ki, float kd)
    : Kp(kp),
      Ki(ki),
      Kd(kd),
      outMin(0.0f),
      outMax(100.0f),
      integral(0.0f),
      prevInput(0.0f),
      enabled(true),
      initialized(false),
      allparamok(false),
      tempSensorId(""),
      setPointConfig(0.0f)
{
}

float PID::limit(float value, float minVal, float maxVal)
{
    if (value < minVal)
        return minVal;

    if (value > maxVal)
        return maxVal;

    return value;
}

// PID felfuggesztese vagy folytatasa
void PID::setEnabled(bool state, float currentInput)
{
    if (state == enabled)
        return;

    enabled = state;

    if (enabled)
    {
        prevInput = currentInput;
        initialized = true;
    }
}

// PID szamitas valtozo idovel
float PID::compute(float setpoint, float input)
{
    unsigned long now = millis();
    unsigned long dt_ms = now - lastComputeTime;

    if (!enabled)
    {
        // A szunet ideje ne szamitson bele
        lastComputeTime = now;
        return 0.0f;
    }

    if (!initialized)
    {
        prevInput = input;
        lastComputeTime = now;
        initialized = true;

        return limit(
            Kp * (setpoint - input) + integral,
            outMin,
            outMax);
    }

    lastComputeTime = now;

    if (dt_ms == 0 || dt_ms > 60000UL)
    {
        prevInput = input;
        return limit(
            Kp * (setpoint - input) + integral,
            outMin,
            outMax);
    }

    float dt = dt_ms * 0.001f;
    float error = setpoint - input;

    float P = Kp * error;

    integral += Ki * error * dt;
    integral = limit(integral, outMin, outMax);

    float D = -Kd * (input - prevInput) / dt;

    prevInput = input;

    return limit(P + integral + D, outMin, outMax);
}