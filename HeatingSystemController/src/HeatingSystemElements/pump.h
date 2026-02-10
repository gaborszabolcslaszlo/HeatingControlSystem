#ifndef PUMP_H
#define PUMP_H

#include <string>
#include "common.h"
#include "../comon.h"

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
    int maxControlSig;
    int minControlSig;
    int SaftyIOnumberForAnalog;
    bool discargepump;

    PumpWorkingMode workingMode;
    int id;

    Pump();

    Pump(int id, std::string name, std::string model, int maxControlSig, int minControlSig, std::string workingMode, int SaftyIOnumberForAnalog, bool discargepump);

    int getControlSignal() const;

    // Setter metódus
    void setControlSignal(int value);

    bool validate();

    void print();

    void update();

private:
    int controlSignal;
};
#endif