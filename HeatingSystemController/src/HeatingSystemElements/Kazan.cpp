
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
    float histValue = 1.5;
    if (!isOverHeatProtectionActive && bodyTemp > (tourTempProtValue + histValue))
    {
        logMessage("Kazán tulmelegedes védelem aktiválva!\n");
        isOverHeatProtectionActive = true;
    }
    else if (isOverHeatProtectionActive && bodyTemp < (tourTempProtValue - histValue))
    {
        isOverHeatProtectionActive = false;
    }

    if (!isRetourProtectionActive && retourTemp < (retourTempProtValue - histValue))
    {
        logMessage("Kazán retour védelem aktiválva!\n");
        isRetourProtectionActive = true;
    }
    else if (isRetourProtectionActive && retourTemp > (retourTempProtValue + histValue))
    {
        isRetourProtectionActive = false;
    }

    if (!isKazanActive && bodyTemp > (activationThreshold - histValue))
    {
        logMessage("Kazán aktiv!\n");
        isKazanActive = true;
    }
    else if (isKazanActive && bodyTemp < (activationThreshold + histValue))
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
    HeatingElement::ElementsStateMap[name]["iOHPA"] = isOverHeatProtectionActive ? "true" : "false";
    HeatingElement::ElementsStateMap[name]["iRPA"] = isRetourProtectionActive ? "true" : "false";
    HeatingElement::ElementsStateMap[name]["iKA"] = isKazanActive ? "true" : "false";
}

bool Kazan::getIsOverHeatProtectionActive() const { return isOverHeatProtectionActive; }

bool Kazan::getIsRetourProtectionActive() const { return isRetourProtectionActive; }

bool Kazan::getIsKazanActive() const { return isKazanActive; }
