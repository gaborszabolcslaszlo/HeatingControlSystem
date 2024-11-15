
#include "Bojler.h"

Bojler::Bojler(MessageBus &bus, String name) : HeatingElement(bus, name.c_str()) {}

// Bojler vezérlése melegvíz igény esetén
void Bojler::manageHotWater(float hotWaterDemandTemp)
{
}
