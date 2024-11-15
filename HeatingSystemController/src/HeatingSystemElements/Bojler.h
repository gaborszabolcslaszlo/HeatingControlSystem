#ifndef BOJLER_H
#define BOJLER_H

#include "HeatingElement.h"
#include "MessageBus.h"
#include <Arduino.h>

class Bojler : public HeatingElement
{
public:
    Bojler(MessageBus &bus, String name);

    // Bojler vezérlése melegvíz igény esetén
    void manageHotWater(float hotWaterDemandTemp);
};

#endif