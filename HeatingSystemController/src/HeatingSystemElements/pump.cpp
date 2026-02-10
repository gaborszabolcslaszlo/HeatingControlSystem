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

Pump::Pump(int id, std::string name, std::string model, int maxControlSig, int minControlSig, std::string workingMode, int SaftyIOnumberForAnalog, bool discargepump)
{
    this->id = id;
    this->name = name;
    this->model = model;
    this->maxControlSig = maxControlSig;
    this->minControlSig = minControlSig;
    this->SaftyIOnumberForAnalog = SaftyIOnumberForAnalog;
    this->workingMode = PumpWorkingModeFromString(workingMode.c_str());
    this->discargepump = discargepump;
}

int Pump::getControlSignal() const
{
    return controlSignal;
}

// Setter metódus
void Pump::setControlSignal(int value)
{
    if (value > 100)
    {
        controlSignal = 100;
    }
    else
    {
        controlSignal = value;
    }
}

bool Pump::validate()
{
    if (name.empty() || model.empty() || workingMode != PumpWorkingMode::UNKNOWN)
    {
        logMessage("Error: Pump name, model, or working mode is missing.");
        return false;
    }
    return true;
}

void Pump::print()
{
    logMessage("        Pump:\n");
    logMessage("            Name: %s\n", name.c_str());
    logMessage("            Model: %s\n", model.c_str());
    logMessage("            Max Control Signal: %s\n", std::to_string(maxControlSig).c_str());
    logMessage("            Min Control Signal: %s\n", std::to_string(minControlSig).c_str());
    logMessage("            Working Mode: %s \n", PumpWorkingModeToString(workingMode).c_str());
    logMessage("            IOid: %s\n", std::to_string(id).c_str());
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
