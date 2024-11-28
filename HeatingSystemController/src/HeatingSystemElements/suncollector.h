#ifndef SUNCOLLECTOR_H
#define SUNCOLLECTOR_H

#include "HeatingElement.h"
#include "MessageBus.h"
#include "common.h"
#include "../comon.h"

class SunCollector : public HeatingElement
{
public:
    SunCollector(MessageBus &bus, std::string name);
    bool hasStoredEnergy();
};

#endif