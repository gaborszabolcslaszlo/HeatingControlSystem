#include "valve.h"

// Valve class

Valve::Valve() {}

Valve::Valve(std::string name, int maxControlSig, int minControlSig, std::string workingMode)
{
    this->name = name;
    this->maxControlSig = maxControlSig;
    this->minControlSig = minControlSig;
    this->workingMode = workingMode;
}

bool Valve::validate()
{
    if (name.empty())
    {
        logMessage("Error: Valve name is missing.");
        return false;
    }
    return true;
}

void Valve::print()
{
    logMessage("        Valve:\n");
    logMessage("            Name: %s\n", name.c_str());
    logMessage("            Max Control Signal: %s\n", std::to_string(maxControlSig).c_str());
    logMessage("            Min Control Signal: %s\n", std::to_string(minControlSig).c_str());
    logMessage("            Working Mode: %s\n", workingMode.c_str());
}
