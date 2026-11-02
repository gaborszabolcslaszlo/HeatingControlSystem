
#include "Kazan.h"

Kazan::Kazan(MessageBus &bus, std::string name, float retourTempProtValue, float tourTempProtValue, float activationThreshold, FuelType type) : HeatingElement(bus, name), sm(this, COOLD), fuelType(type)
{
    logDebug("Kazán Obiektum!!!!\n");
    this->retourTempProtValue = retourTempProtValue;
    this->tourTempProtValue = tourTempProtValue;
    this->activationThreshold = activationThreshold;

    sm.addState(COOLD, &Kazan::state_COOLD);
    sm.addState(RETOUR_PROTECTION, &Kazan::state_RETOUR_PROTECTION);
    sm.addState(HEATING, &Kazan::state_HEATING);
    sm.addState(OVERHEATED, &Kazan::state_OVERHEATED);
    sm.addState(PREHEAT_FROM_PUFFER, &Kazan::state_PREHEAT_FROM_PUFFER);

    sm.addTransition(COOLD, RETOUR_PROTECTION, &Kazan::trch_COOLD_to_RETOUR_PROTECTION);
    sm.addTransition(RETOUR_PROTECTION, COOLD, &Kazan::trch_RETOUR_PROTECTION_to_COOLD);
    sm.addTransition(RETOUR_PROTECTION, HEATING, &Kazan::trch_RETOUR_PROTECTION_to_HEATING);
    sm.addTransition(HEATING, RETOUR_PROTECTION, &Kazan::trch_HEATING_to_RETOUR_PROTECTION);
    sm.addTransition(OVERHEATED, HEATING, &Kazan::trch_OVERHEATED_to_HEATING);
    sm.addTransition(HEATING, OVERHEATED, &Kazan::trch_HEATING_to_OVERHEATED);

    sm.addTransition(PREHEAT_FROM_PUFFER, RETOUR_PROTECTION, &Kazan::trch_PREHEAT_FROM_PUFFER_to_RETOUR_PROTECTION);
    sm.addTransition(PREHEAT_FROM_PUFFER, COOLD, &Kazan::trch_PREHEAT_FROM_PUFFER_to_COOLD);
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
    logInfo("Kazan state: %s\n", getStateName(sm.getState()));
    HeatingElement::setStateValue(name, "iOHPA", isOverHeatProtectionActive ? "true" : "false");
    HeatingElement::setStateValue(name, "iRPA", isRetourProtectionActive ? "true" : "false");
    HeatingElement::setStateValue(name, "state", getStateName(sm.getState()));
}

void Kazan::run()
{
    HeatingElement::update();
    tourTemp = HeatingElement::getTourTemperature();
    retourTemp = HeatingElement::getReTourTemperature();
    bodyTemp = HeatingElement::getBodyTemperature();

    // checkRetourLowTemperatureProtection();
    // HeatingElement::update();
    sm.run();

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

    if (tourTemp < (tourTempProtValue - 2.0f))
    {
        retVal = true;
    }

    return retVal;
}

bool Kazan::trch_PREHEAT_FROM_PUFFER_to_RETOUR_PROTECTION()
{
    bool retVal = false;
    bool functionActive = false;

    if (&functionActive && retourTemp > (retourTempProtValue - 2.0f))
    {
        retVal = true;
    }

    return retVal;
}

bool Kazan::trch_PREHEAT_FROM_PUFFER_to_COOLD()
{
    bool retVal = false;
    bool functionActive = false;
    int duration;
    int timeLimit;

    if (&functionActive && retourTemp < (retourTempProtValue - 2.0f) && duration > timeLimit)
    {
        retVal = true;
    }

    return retVal;
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
