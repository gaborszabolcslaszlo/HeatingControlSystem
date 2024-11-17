#ifndef KAZAN_H
#define KAZAN_H

#include "HeatingElement.h"
#include "MessageBus.h"
#include <string>
#include "common.h"
#include "../comon.h"

class Kazan : public HeatingElement
{
public:
    Kazan(MessageBus &bus, std::string name, float retourTempProtValue, float tourTempProtValue, float activationThreshold);

    void checkRetourLowTemperatureProtection();

    void onMessageReceived(const std::string &message) override;
    void update();

    bool getIsOverHeatProtectionActive() const;

    bool getIsRetourProtectionActive() const;

    bool getIsKazanActive() const;

    

private:
    float retourTempProtValue;
    float tourTempProtValue;
    float activationThreshold;
    bool isRetourProtectionActive;
    bool isOverHeatProtectionActive;
    bool isKazanActive;
};

#endif