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
        logError("Error: Valve name is missing.");
        return false;
    }
    return true;
}

void Valve::print()
{
    logDebug("        Valve:\n");
    logDebug("            Name: %s\n", name.c_str());
    logDebug("            Max Control Signal: %d\n", maxControlSig);
    logDebug("            Min Control Signal: %d\n", minControlSig);
    logDebug("            Working Mode: %s\n", workingMode.c_str());
}
