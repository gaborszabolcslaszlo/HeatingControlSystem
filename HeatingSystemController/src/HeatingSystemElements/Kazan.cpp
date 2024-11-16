
#include "Kazan.h"

Kazan::Kazan(MessageBus &bus, std::string name, float retourTempProtValue, float tourTempProtValue, float activationThreshold) : HeatingElement(bus, name)
{
    logMessage("Kazán Obiektum!!!!\n");
    this->retourTempProtValue = retourTempProtValue;
    this->tourTempProtValue = tourTempProtValue;
    this->activationThreshold = activationThreshold;
}

void Kazan::checkRetourLowTemperatureProtection()
{
    float tourTemp = HeatingElement::getTourTemperature();
    float retourTemp = HeatingElement::getReTourTemperature();
    float bodyTemp = HeatingElement::getBodyTemperature();
    if (!isOverHeatProtectionActive && bodyTemp > (tourTempProtValue + tourTempProtValue * 0.05))
    {
        logMessage("Kazán tulmelegedes védelem aktiválva!\n");
        isOverHeatProtectionActive = true;
    }
    else if (isOverHeatProtectionActive && bodyTemp < (tourTempProtValue - tourTempProtValue * 0.05))
    {
        isOverHeatProtectionActive = false;
    }

    if (!isRetourProtectionActive && retourTemp < (retourTempProtValue - retourTempProtValue * 0.05))
    {
        logMessage("Kazán retour védelem aktiválva!\n");
        isRetourProtectionActive = true;
    }
    else if (isRetourProtectionActive && retourTemp > (retourTempProtValue + retourTempProtValue * 0.05))
    {
        isRetourProtectionActive = false;
    }

    if (!isKazanActive && bodyTemp > (activationThreshold - activationThreshold * 0.05))
    {
        logMessage("Kazán aktiv!\n");
        isKazanActive = true;
    }
    else if (isKazanActive && bodyTemp < (activationThreshold + activationThreshold * 0.05))
    {
        isKazanActive = false;
    }
}

void Kazan::onMessageReceived(const std::string &message)
{
    logMessage("%s\n", message.c_str());
}

void Kazan::update()
{
    checkRetourLowTemperatureProtection();
    HeatingElement::update();
}

bool Kazan::getIsOverHeatProtectionActive() const { return isOverHeatProtectionActive; }

bool Kazan::getIsRetourProtectionActive() const { return isRetourProtectionActive; }

bool Kazan::getIsKazanActive() const
{
    return isKazanActive;
}
