
#include "Kazan.h"

Kazan::Kazan(MessageBus &bus, std::string name, float retourTempProtValue, float tourTempProtValue, float activationThreshold, FuelType type) : HeatingElement(bus, name), sm(this, COOLD), fuelType(type)
{
    logDebug("Kazán Obiektum!!!!\n");
    this->retourTempProtValue = retourTempProtValue;
    this->tourTempProtValue = tourTempProtValue;
    this->activationThreshold = activationThreshold;
    preheatFromPuffer.targetTemp = retourTempProtValue;

    sm.addState(COOLD, &Kazan::state_COOLD);
    sm.addState(RETOUR_PROTECTION, &Kazan::state_RETOUR_PROTECTION);
    sm.addState(HEATING, &Kazan::state_HEATING);
    sm.addState(OVERHEATED, &Kazan::state_OVERHEATED);
    sm.addState(PREHEAT_FROM_PUFFER, &Kazan::state_PREHEAT_FROM_PUFFER);
    sm.addState(COOLING_FROM_PUFFER, &Kazan::state_COOLING_FROM_PUFFER);

    sm.addTransition(COOLD, RETOUR_PROTECTION, &Kazan::trch_COOLD_to_RETOUR_PROTECTION);
    sm.addTransition(RETOUR_PROTECTION, COOLD, &Kazan::trch_RETOUR_PROTECTION_to_COOLD);
    sm.addTransition(RETOUR_PROTECTION, HEATING, &Kazan::trch_RETOUR_PROTECTION_to_HEATING);
    sm.addTransition(HEATING, RETOUR_PROTECTION, &Kazan::trch_HEATING_to_RETOUR_PROTECTION);
    sm.addTransition(OVERHEATED, HEATING, &Kazan::trch_OVERHEATED_to_HEATING);
    sm.addTransition(HEATING, OVERHEATED, &Kazan::trch_HEATING_to_OVERHEATED);

    sm.addTransition(PREHEAT_FROM_PUFFER, RETOUR_PROTECTION, &Kazan::trch_PREHEAT_FROM_PUFFER_to_RETOUR_PROTECTION);
    sm.addTransition(PREHEAT_FROM_PUFFER, COOLD, &Kazan::trch_PREHEAT_FROM_PUFFER_to_COOLD);
    sm.addTransition(COOLING_FROM_PUFFER, COOLD, &Kazan::trch_COOLING_FROM_PUFFER_to_COOLD);
    sm.addTransition(PREHEAT_FROM_PUFFER, OVERHEATED, &Kazan::trch_bufferOperation_to_OVERHEATED);
    sm.addTransition(COOLING_FROM_PUFFER, OVERHEATED, &Kazan::trch_bufferOperation_to_OVERHEATED);
}

const char *Kazan::getStateName(State state) const
{
    switch (state)
    {
    case COOLD:
        return "COOLD";
    case RETOUR_PROTECTION:
        return "RETOUR_PROTECTION";
    case HEATING:
        return "HEATING";
    case OVERHEATED:
        return "OVERHEATED";
    case PREHEAT_FROM_PUFFER:
        return "PREHEAT_FROM_PUFFER";
    case COOLING_FROM_PUFFER:
        return "COOLING_FROM_PUFFER";
    default:
        return "UNKNOWN";
    }
}

void Kazan::checkRetourLowTemperatureProtection()
{

    float histValue = 1.0;
    if (!isOverHeatProtectionActive && bodyTemp > (tourTempProtValue + histValue))
    {
        logInfo("Kazán tulmelegedes védelem aktiválva!\n");
        isOverHeatProtectionActive = true;
    }
    else if (isOverHeatProtectionActive && bodyTemp < (tourTempProtValue - histValue))
    {
        isOverHeatProtectionActive = false;
    }

    if (isKazanActive && !isRetourProtectionActive && retourTemp < (retourTempProtValue - histValue))
    {
        logInfo("Kazán retour védelem aktiválva!\n");
        isRetourProtectionActive = true;
    }
    else if (isKazanActive && isRetourProtectionActive && retourTemp > (retourTempProtValue + histValue))
    {
        isRetourProtectionActive = false;
    }

    if (!isKazanActive && tourTemp > (activationThreshold))
    {
        logInfo("Kazán aktiv!\n");
        isKazanActive = true;
    }
    else if (isKazanActive && tourTemp < (activationThreshold))
    {
        isKazanActive = false;
        isRetourProtectionActive = false;
    }
}

void Kazan::onMessageReceived(const std::string &message)
{
    logInfo("%s\n", message.c_str());
}

void Kazan::update()
{
    // Serial.print("Kazan: ");
    tourTemp = HeatingElement::getTourTemperature();
    retourTemp = HeatingElement::getReTourTemperature();
    bodyTemp = HeatingElement::getBodyTemperature();

    checkRetourLowTemperatureProtection();
    HeatingElement::update();
    sm.run();
    HeatingElement::update();
    logInfo("Kazan state: %s\n", getStateName(sm.getState()));
    HeatingElement::setStateValue(name, "iOHPA", isOverHeatProtectionActive ? "true" : "false");
    HeatingElement::setStateValue(name, "iRPA", isRetourProtectionActive ? "true" : "false");
    HeatingElement::setStateValue(name, "state", getStateName(sm.getState()));
}

void Kazan::run()
{
    for (Sensor &sensor : sensors)
        sensor.update();
    tourTemp = HeatingElement::getTourTemperature();
    retourTemp = HeatingElement::getReTourTemperature();
    bodyTemp = HeatingElement::getBodyTemperature();

    checkRetourLowTemperatureProtection();
    // HeatingElement::update();
    HeatingElement::update();
    sm.run();
    HeatingElement::update();
    HeatingElement::setStateValue(name, "iOHPA", isOverHeatProtectionActive ? "true" : "false");
    HeatingElement::setStateValue(name, "iRPA", isRetourProtectionActive ? "true" : "false");
    HeatingElement::setStateValue(name, "state", getStateName(sm.getState()));
}

Kazan::State Kazan::getState() const
{
    return sm.getState();
}

Kazan::FuelType Kazan::getFuelType() const
{
    return fuelType;
}

bool Kazan::getIsOverHeatProtectionActive() const { return isOverHeatProtectionActive; }

bool Kazan::getIsRetourProtectionActive() const { return isRetourProtectionActive; }

bool Kazan::getIsKazanActive() const { return isKazanActive; }

// State functions
void Kazan::state_COOLD()
{
    deactivate();
}

void Kazan::state_RETOUR_PROTECTION()
{
    activate();
}

void Kazan::state_HEATING()
{
    activate();
}

void Kazan::state_OVERHEATED()
{
    activate();
}

void Kazan::state_PREHEAT_FROM_PUFFER()
{
    activate();
}

void Kazan::state_COOLING_FROM_PUFFER()
{
    activate();
}
// Transition Functions

bool Kazan::trch_COOLD_to_RETOUR_PROTECTION()
{
    bool retVal = false;

    if (tourTemp > (activationThreshold))
    {
        retVal = true;
    }

    return retVal;
}

bool Kazan::trch_RETOUR_PROTECTION_to_COOLD()
{
    bool retVal = false;

    if (tourTemp < (activationThreshold - 2.0f))
    {
        retVal = true;
    }

    return retVal;
}

bool Kazan::trch_RETOUR_PROTECTION_to_HEATING()
{
    bool retVal = false;

    if (retourTemp > (retourTempProtValue + 2.0f))
    {
        retVal = true;
    }

    return retVal;
}

bool Kazan::trch_HEATING_to_RETOUR_PROTECTION()
{
    bool retVal = false;

    if (retourTemp < (retourTempProtValue))
    {
        retVal = true;
    }

    return retVal;
}

bool Kazan::trch_HEATING_to_OVERHEATED()
{
    bool retVal = false;

    if (tourTemp > (tourTempProtValue))
    {
        retVal = true;
    }

    return retVal;
}

bool Kazan::trch_OVERHEATED_to_HEATING()
{
    bool retVal = false;

    if (tourTemp < (tourTempProtValue - 2.0f) && bodyTemp < (tourTempProtValue - 2.0f))
    {
        retVal = true;
    }

    return retVal;
}

bool Kazan::trch_PREHEAT_FROM_PUFFER_to_RETOUR_PROTECTION()
{
    return !trch_bufferOperation_to_OVERHEATED() &&
           retourTemp >= preheatFromPuffer.targetTemp;
}

bool Kazan::trch_PREHEAT_FROM_PUFFER_to_COOLD()
{
    return !trch_bufferOperation_to_OVERHEATED() &&
           millis() - bufferOperationStartedAt >= preheatFromPuffer.maxDurationMs;
}

bool Kazan::trch_COOLING_FROM_PUFFER_to_COOLD()
{
    return !trch_bufferOperation_to_OVERHEATED() &&
           (millis() - bufferOperationStartedAt >= coolingFromPuffer.maxDurationMs ||
            tourTemp <= retourTemp + coolingFromPuffer.stopDeltaTemp);
}

bool Kazan::trch_bufferOperation_to_OVERHEATED()
{
    return tourTemp >= tourTempProtValue || bodyTemp >= tourTempProtValue;
}

bool Kazan::isColdForBufferOperation()
{
    return sm.getState() == COOLD && getTourTemperature() < activationThreshold;
}

bool Kazan::hasUnsafeTemperature()
{
    return getTourTemperature() >= tourTempProtValue || getBodyTemperature() >= tourTempProtValue;
}

bool Kazan::beginBufferOperation(bool cooling)
{
    if (fuelType != FuelType::WOOD || !isColdForBufferOperation())
        return false;
    const BufferOperationConfig &config = cooling ? coolingFromPuffer : preheatFromPuffer;
    if (!config.enabled)
        return false;
    bufferOperationStartedAt = millis();
    sm.setState(cooling ? COOLING_FROM_PUFFER : PREHEAT_FROM_PUFFER);
    return true;
}

bool Kazan::isBufferOperationActive() const
{
    return sm.getState() == PREHEAT_FROM_PUFFER || sm.getState() == COOLING_FROM_PUFFER;
}

void Kazan::cancelBufferOperation()
{
    if (isBufferOperationActive())
    {
        sm.setState(hasUnsafeTemperature() ? OVERHEATED : COOLD);
    }
}

void Kazan::requestStart()
{
    startRequested = true;
}

void Kazan::requestStop()
{
    startRequested = false;
}

bool Kazan::isStartRequested() const
{
    return startRequested;
}
