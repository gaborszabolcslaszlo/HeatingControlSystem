#include "suncollector.h"

SunCollector::SunCollector(MessageBus &bus, std::string name) : HeatingElement(bus, name) {}

bool SunCollector::hasStoredEnergy()
{
    return getBodyTemperature() > 60 ? true : false;
}