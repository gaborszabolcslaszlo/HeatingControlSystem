#ifndef BOJLER_H
#define BOJLER_H

#include "HeatingElement.h"
#include "MessageBus.h"
#include <string>

class Bojler : public HeatingElement
{
public:
    Bojler(MessageBus &bus, std::string name);

    // Bojler vezérlése melegvíz igény esetén
    void manageHotWater(float hotWaterDemandTemp);
};

#endif