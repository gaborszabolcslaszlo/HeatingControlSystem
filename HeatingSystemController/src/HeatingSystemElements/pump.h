#ifndef PUMP_H
#define PUMP_H

#include <string>
#include "common.h"
#include "../comon.h"
#include "Controllers/pid.h"
#include "Sensor.h"

enum class PumpWorkingMode
{
    ONOFF,
    PWM,
    UNKNOWN
};

// "to string" függvény
std::string PumpWorkingModeToString(PumpWorkingMode mode);

// "from string" függvény
PumpWorkingMode PumpWorkingModeFromString(const std::string &modeStr);

class Pump
{
public:
    std::string name;
    std::string model;
    bool dischargepump;
    int maxControlSig;
    int minControlSig;
    int SaftyIOnumberForAnalog;

    PumpWorkingMode workingMode;
    int id;
    bool active = false;

    Pump();

    Pump(int id, std::string name, std::string model, int maxControlSig, int minControlSig, std::string workingMode, int SaftyIOnumberForAnalog, bool dischargepump, PID pid);

    int getControlSignal() const;

    // Setter metódus
    void setControlSignal(int value);

    bool validate();

    void print();

    void update();

    void run();

    void enable();

    void disable(bool skipPidPump = true);

private:
    int controlSignal = 0;
    PID pid_c;
};
#endif
