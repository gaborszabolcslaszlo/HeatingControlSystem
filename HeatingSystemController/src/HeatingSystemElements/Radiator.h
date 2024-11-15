#ifndef RADIATOR_H
#define RADIATOR_H

#include "HeatingElement.h"
#include "MessageBus.h"
#include <Arduino.h>

class Radiator : public HeatingElement
{
public:
    Radiator(MessageBus &bus, String name);

    // Radiátor egyedi vezérlése
    void manageHeating(float desiredTemp);

    void update();
};

#endif