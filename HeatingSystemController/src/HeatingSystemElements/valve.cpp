#include "valve.h"

// Valve class

Valve::Valve() {}

Valve::Valve(String name, int maxControlSig, int minControlSig, String workingMode)
{
    this->name = name;
    this->maxControlSig = maxControlSig;
    this->minControlSig = minControlSig;
    this->workingMode = workingMode;
}

bool Valve::validate()
{
    if (name.isEmpty())
    {
        Serial.println("Error: Valve name is missing.");
        return false;
    }
    return true;
}

void Valve::print()
{
    Serial.print("      Valve:");
    Serial.print("  Name: " + name);
    Serial.print(",  Max Control Signal: " + String(maxControlSig));
    Serial.print(",  Min Control Signal: " + String(minControlSig));
    Serial.println(",  Working Mode: " + workingMode);
}
