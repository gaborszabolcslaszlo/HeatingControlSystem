#include "pump.h"

extern void writePwmIO(int id, int duty);
extern void writeIO(int id, int value);

// "to string" függvény
std::string PumpWorkingModeToString(PumpWorkingMode mode)
{
    switch (mode)
    {
    case PumpWorkingMode::ONOFF:
        return "ONOFF";
    case PumpWorkingMode::PWM:
        return "PWM";
    case PumpWorkingMode::UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

// "from string" függvény
PumpWorkingMode PumpWorkingModeFromString(const std::string &modeStr)
{
    if (modeStr == "ON/OFF")
    {
        return PumpWorkingMode::ONOFF;
    }
    else if (modeStr == "PWM")
    {
        return PumpWorkingMode::PWM;
    }
    else
    {
        return PumpWorkingMode::UNKNOWN;
    }
}

Pump::Pump() {}

Pump::Pump(int id, String name, String model, int maxControlSig, int minControlSig, String workingMode)
{
    this->id = id;
    this->name = name;
    this->model = model;
    this->maxControlSig = maxControlSig;
    this->minControlSig = minControlSig;
    this->workingMode = PumpWorkingModeFromString(workingMode.c_str());
}

int Pump::getControlSignal() const
{
    return controlSignal;
}

// Setter metódus
void Pump::setControlSignal(int value)
{
    controlSignal = value;
}

bool Pump::validate()
{
    if (name.isEmpty() || model.isEmpty() || workingMode != PumpWorkingMode::UNKNOWN)
    {
        Serial.println("Error: Pump name, model, or working mode is missing.");
        return false;
    }
    return true;
}

void Pump::print()
{
    Serial.print("      Pump:");
    Serial.print("  Name: " + name);
    Serial.print(", Model: " + model);
    Serial.print(", Max Control Signal: " + String(maxControlSig));
    Serial.print(", Min Control Signal: " + String(minControlSig));
    Serial.printf(", Working Mode: %s \n", PumpWorkingModeToString(workingMode).c_str());
}

void Pump::update()
{
    if (workingMode == PumpWorkingMode::ONOFF)
    {
        writeIO(id, controlSignal);
    }
    if (workingMode == PumpWorkingMode::PWM)
    {
        writePwmIO(id, controlSignal);
    }
}
