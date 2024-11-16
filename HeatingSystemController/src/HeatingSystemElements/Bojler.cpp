
#include "Bojler.h"

Bojler::Bojler(MessageBus &bus, std::string name) : HeatingElement(bus, name) {}

// Bojler vezérlése melegvíz igény esetén
void Bojler::manageHotWater(float hotWaterDemandTemp)
{
}
