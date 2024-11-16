#include "Radiator.h"

Radiator::Radiator(MessageBus &bus, std::string name) : HeatingElement(bus, name) {}

// Radiátor egyedi vezérlése
void Radiator::manageHeating(float desiredTemp)
{
}

void Radiator::update()
{
    HeatingElement::update();
    if (getTourTemperature() < 50)
    {
        deactivate();
    }
}
