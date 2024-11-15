#ifndef KAZAN_H
#define KAZAN_H

#include "HeatingElement.h"
#include "MessageBus.h"
#include <string>
#include <WString.h>

class Kazan : public HeatingElement
{
public:
    Kazan(MessageBus &bus, String name, float retourTempProtValue, float tourTempProtValue, float activationThreshold);

    void checkRetourLowTemperatureProtection();

    void onMessageReceived(const std::string &message) override;
    void update();
    void checkIsActive();

    bool getIsOverHeatProtectionActive() const;
    void setIsOverHeatProtectionActive(bool isOverHeatProtectionActive_);

    bool getIsRetourProtectionActive() const;
    void setIsRetourProtectionActive(bool isRetourProtectionActive_);

private:
    float retourTempProtValue;
    float tourTempProtValue;
    float activationThreshold;
    bool isRetourProtectionActive;
    bool isOverHeatProtectionActive;
};

#endif