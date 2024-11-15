#include "Radiator.h"

Radiator::Radiator(MessageBus &bus, String name) : HeatingElement(bus, name.c_str()) {}

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
