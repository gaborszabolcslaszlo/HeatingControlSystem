#ifndef RADIATOR_H
#define RADIATOR_H

#include "HeatingElement.h"
#include "MessageBus.h"
#include "common.h"
#include "../comon.h"

class Radiator : public HeatingElement
{
public:
    Radiator(MessageBus &bus, std::string name);

    // Radiátor egyedi vezérlése
    void manageHeating(float desiredTemp);

    void update();
};

#endif