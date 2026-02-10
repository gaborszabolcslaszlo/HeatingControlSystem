#ifndef PUFFER_H
#define PUFFER_H

#include "HeatingElement.h"
#include "MessageBus.h"
#include <string> // std::string használatához
#include "common.h"
#include "../comon.h"
#include "Kazan.h"

class Puffer : public HeatingElement
{
public:
    Puffer(MessageBus &bus, std::string name, std::string linkedHeatSourceName, float LinkedHeatSource_ActivationTourTemp); // std::string használata
    std::vector<Pump> DischargePumps;
    // Puffer egyedi vezérlése
    void moveEnergyToRadiators(float radiatorRequestTemp);

    bool hasStoredEnergy();

    Kazan *getActiveKazan();

    void update();

    bool canSupplyHeat(HeatingElement *element) override; // Override helyes használata

    std::string linkedHeatSourceName;
    float LinkedHeatSource_ActivationTourTemp;
    void chargeFromSource(HeatingElement *element);
    bool isChargeActivated;
    bool isDeschargeActivated;
    float getBodyTemperatureFromLevel(int level = 100);
    void discharge(bool active);
};

#endif
