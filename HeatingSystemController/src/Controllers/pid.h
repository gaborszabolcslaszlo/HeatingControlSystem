
#ifndef PID_H
#define PID_H

#include <string>

class PID
{
public:
    float Kp;
    float Ki;
    float Kd;

    float outMin;
    float outMax;

    // Ha deadZoneMax > deadZoneMin, a pozitív PID-igényhez deadZoneMax
    // indulási eltolás adódik. Nulla vagy negatív igénynél 0 PWM a kimenet.
    float deadZoneMin;
    float deadZoneMax;

    float integral;
    float prevInput;

    bool enabled;
    bool initialized;

    bool allparamok;

    std::string tempSensorId;

    float setPointConfig;

    unsigned long lastComputeTime = 0;

    PID();

    PID(float kp, float ki, float kd);

    static float limit(float value,
                       float minVal,
                       float maxVal);

    void setEnabled(bool state, float currentInput);

    void reset(float currentInput);

    float compute(float setpoint,
                  float input);

};

#endif
