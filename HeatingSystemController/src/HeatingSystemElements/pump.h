#ifndef PUMP_H
#define PUMP_H

#include <WString.h>
#include <Arduino.h>
#include <string>

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
    String name;
    String model;
    int maxControlSig;
    int minControlSig;
    PumpWorkingMode workingMode;
    int id;

    Pump();

    Pump(int id, String name, String model, int maxControlSig, int minControlSig, String workingMode);

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