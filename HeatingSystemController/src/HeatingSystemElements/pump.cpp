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

Pump::Pump(int id, std::string name, std::string model, int maxControlSig, int minControlSig, std::string workingMode, int SaftyIOnumberForAnalog, bool dischargepump, PID pid)
{
    this->id = id;
    this->name = name;
    this->model = model;
    this->maxControlSig = maxControlSig;
    this->minControlSig = minControlSig;
    this->SaftyIOnumberForAnalog = SaftyIOnumberForAnalog;
    this->workingMode = PumpWorkingModeFromString(workingMode.c_str());
    this->dischargepump = dischargepump;
    this->pid_c = pid;
}

int Pump::getControlSignal() const
{
    return controlSignal;
}

// Setter metódus
void Pump::setControlSignal(int value)
{
    if (!pid_c.enabled && !pid_c.initialized)
    {
        controlSignal = value;
    }
}

bool Pump::validate()
{
    if (name.empty() || model.empty() || workingMode != PumpWorkingMode::UNKNOWN)
    {
        logError("Error: Pump name, model, or working mode is missing.");
        return false;
    }
    return true;
}

void Pump::print()
{
    logDebug("        Pump:\n");
    logDebug("            Name: %s\n", name.c_str());
    logDebug("            Model: %s\n", model.c_str());
    logDebug("            Max Control Signal: %d\n", maxControlSig);
    logDebug("            Min Control Signal: %d\n", minControlSig);
    logDebug("            Working Mode: %s \n", PumpWorkingModeToString(workingMode).c_str());
    logDebug("            IOid: %d\n", id);
}

void Pump::update()
{
    if (!active)
    {
        controlSignal = 0;
        if (workingMode == PumpWorkingMode::ONOFF)
        {
            writeIO(id, 0);
        }
        else if (workingMode == PumpWorkingMode::PWM)
        {
            writePwmIO(id, 0);
        }
        return;
    }

    if (workingMode == PumpWorkingMode::ONOFF)
    {
        writeIO(id, controlSignal);
    }
    if (workingMode == PumpWorkingMode::PWM)
    {
        if (pid_c.allparamok && pid_c.enabled)
        {
            float tempValue = Sensor::SensorsValue[std::string(pid_c.tempSensorId.c_str())];
            controlSignal = pid_c.compute(pid_c.setPointConfig, tempValue);
        }
        else
        {
            // Letiltott PID esetén a korábbi PWM érték nem maradhat a kimeneten.
            controlSignal = 0;
        }
        writePwmIO(id, controlSignal);
    }
}

void Pump::enable()
{
    active = true;

    if (workingMode == PumpWorkingMode::ONOFF)
    {
        setControlSignal(100);
    }
    if (workingMode == PumpWorkingMode::PWM)
    {
        float tempValue = Sensor::SensorsValue[std::string(pid_c.tempSensorId.c_str())];
        pid_c.setEnabled(true, tempValue);
    }
}
void Pump::disable(bool skipPidPump)
{
    active = false;

    if (workingMode == PumpWorkingMode::ONOFF)
    {
        setControlSignal(0);
    }
    if (!skipPidPump && workingMode == PumpWorkingMode::PWM)
    {
        float tempValue = Sensor::SensorsValue[std::string(pid_c.tempSensorId.c_str())];
        pid_c.setEnabled(false, tempValue);
        controlSignal = 0;
    }
}
