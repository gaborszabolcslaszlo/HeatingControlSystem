#ifndef PUFFER_H
#define PUFFER_H

#include "HeatingElement.h"
#include "MessageBus.h"
#include <string> // std::string használatához
#include "common.h"

class Puffer : public HeatingElement
{
public:
    Puffer(MessageBus &bus, std::string name); // std::string használata

    // Puffer egyedi vezérlése
    void moveEnergyToRadiators(float radiatorRequestTemp);

    bool hasStoredEnergy();

    bool canSupplyHeat(HeatingElement *element) override; // Override helyes használata
};

#endif
