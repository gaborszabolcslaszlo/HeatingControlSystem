
#include "Kazan.h"

Kazan::Kazan(MessageBus &bus, String name, float retourTempProtValue, float tourTempProtValue, float activationThreshold) : HeatingElement(bus, name.c_str())
{
    Serial.println("Kazán Obiektum!!!!");
    this->retourTempProtValue = retourTempProtValue;
    this->tourTempProtValue = tourTempProtValue;
    this->activationThreshold = activationThreshold;
}

void Kazan::checkRetourLowTemperatureProtection()
{
    float tourTemp = HeatingElement::getTourTemperature();
    float retourTemp = HeatingElement::getReTourTemperature();
    if (!isOverHeatProtectionActive && HeatingElement::getBodyTemperature() > (tourTempProtValue + tourTempProtValue * 0.05))
    {
        Serial.println("Kazán tulmelegedes védelem aktiválva!");
        isOverHeatProtectionActive = true;
    }
    else if (isOverHeatProtectionActive && HeatingElement::getBodyTemperature() < (tourTempProtValue - tourTempProtValue * 0.05))
    {
        isOverHeatProtectionActive = false;
    }

    if (!isRetourProtectionActive && retourTemp < (retourTempProtValue - retourTempProtValue * 0.05))
    {
        Serial.println("Kazán retour védelem aktiválva!");
        isRetourProtectionActive = true;
    }
    else if (isRetourProtectionActive && retourTemp > (retourTempProtValue + retourTempProtValue * 0.05))
    {
        isRetourProtectionActive = false;
    }

    if (HeatingElement::getTourTemperature() < HeatingElement::getReTourTemperature() * 1.05)
    {
        HeatingElement::deactivate();
    }
}

void Kazan::checkIsActive()
{
    HeatingElement::getBodyTemperature();
    if (HeatingElement::getBodyTemperature() > activationThreshold)
    {
        HeatingElement::activate();
    }
    else
    {
        HeatingElement::deactivate();
    }
}

void Kazan::onMessageReceived(const std::string &message)
{
    Serial.println(message.c_str());
}

void Kazan::update()
{
    Kazan::checkIsActive();
    checkRetourLowTemperatureProtection();
    HeatingElement::update();
}

bool Kazan::getIsOverHeatProtectionActive() const { return isOverHeatProtectionActive; }
void Kazan::setIsOverHeatProtectionActive(bool isOverHeatProtectionActive_) { isOverHeatProtectionActive = isOverHeatProtectionActive_; }

bool Kazan::getIsRetourProtectionActive() const { return isRetourProtectionActive; }
void Kazan::setIsRetourProtectionActive(bool isRetourProtectionActive_) { isRetourProtectionActive = isRetourProtectionActive_; }
