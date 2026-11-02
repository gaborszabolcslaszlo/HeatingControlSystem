#include "Puffer.h"

Puffer::Puffer(MessageBus &bus, std::string name, std::string linkedHeatSourceName, float LinkedHeatSource_ActivationTourTemp) : HeatingElement(bus, name), sm(this, STANDBY)
{
    this->linkedHeatSourceName = linkedHeatSourceName;
    this->LinkedHeatSource_ActivationTourTemp = LinkedHeatSource_ActivationTourTemp;

    sm.addState(CHARGE, &Puffer::state_CHARGE);
    sm.addState(DISCHARGE, &Puffer::state_DISCHARGE);
    sm.addState(FULL_CHARGED, &Puffer::state_FULL_CHARGED);
    sm.addState(FULL_DISCHARGED, &Puffer::state_FULL_DISCHARGED);
    sm.addState(STANDBY, &Puffer::state_STANDBY);
    sm.addState(OVERHEATED, &Puffer::state_OVERHEATED);

    sm.addTransition(STANDBY, CHARGE, &Puffer::trch_STANDBY_to_CHARGE);
    sm.addTransition(STANDBY, DISCHARGE, &Puffer::trch_STANDBY_to_DISCHARGE);
    sm.addTransition(CHARGE, STANDBY, &Puffer::trch_CHARGE_to_STANDBY);
    sm.addTransition(CHARGE, FULL_CHARGED, &Puffer::trch_CHARGE_to_FULL_CHARGED);
    sm.addTransition(DISCHARGE, STANDBY, &Puffer::trch_DISCHARGE_to_STANDBY);
    sm.addTransition(DISCHARGE, FULL_DISCHARGED, &Puffer::trch_DISCHARGE_to_FULL_DISCHARGED);
    sm.addTransition(DISCHARGE, CHARGE, &Puffer::trch_DISCHARGE_to_CHARGE);
    sm.addTransition(CHARGE, DISCHARGE, &Puffer::trch_CHARGE_to_DISCHARGE);
    sm.addTransition(FULL_CHARGED, STANDBY, &Puffer::trch_FULL_CHARGED_to_STANDBY);
    sm.addTransition(FULL_CHARGED, DISCHARGE, &Puffer::trch_FULL_CHARGED_to_DISCHARGE);
    sm.addTransition(FULL_DISCHARGED, STANDBY, &Puffer::trch_FULL_DISCHARGED_to_STANDBY);
    sm.addTransition(FULL_DISCHARGED, CHARGE, &Puffer::trch_FULL_DISCHARGED_to_CHARGE);
}
const char *
Puffer::getStateName(State state) const
{
    switch (state)
    {
    case CHARGE:
        return "CHARGE";
    case DISCHARGE:
        return "DISCHARGE";
    case FULL_CHARGED:
        return "FULL_CHARGED";
    case FULL_DISCHARGED:
        return "FULL_DISCHARGED";
    case STANDBY:
        return "STANDBY";
    case OVERHEATED:
        return "OVERHEATED";
    default:
        return "UNKNOWN";
    }
}

Puffer::State Puffer::getState() const
{
    return sm.getState();
}

// Puffer egyedi vezérlése
void Puffer::moveEnergyToRadiators(float radiatorRequestTemp)
{
}

bool Puffer::hasStoredEnergy()
{
    return getBodyTemperature() > 30; // Van tárolt energia, ha a hőmérséklet magasabb, mint a minimum
}

void Puffer::update()
{
    tourTemp = HeatingElement::getTourTemperature();
    retourTemp = HeatingElement::getReTourTemperature();
    bodyTemp = HeatingElement::getBodyTemperature();

    // Serial.print("Puffer: ");
    HeatingElement::update();
    sm.run();
    // Serial.println(getStateName(sm.getState()));
    HeatingElement::setStateValue(name, "HT", this->heatTransfer.transferValue);
    HeatingElement::setStateValue(name, "state", getStateName(sm.getState()));
}

void Puffer::run()
{
    tourTemp = HeatingElement::getTourTemperature();
    retourTemp = HeatingElement::getReTourTemperature();
    bodyTemp = HeatingElement::getBodyTemperature();

    // Serial.print("Puffer: ");
    HeatingElement::update();
    sm.run();
    // Serial.println(getStateName(sm.getState()));
    HeatingElement::setStateValue(name, "HT", this->heatTransfer.transferValue);
    HeatingElement::setStateValue(name, "state", getStateName(sm.getState()));
}

bool Puffer::canSupplyHeat(HeatingElement *element)
{
    double bodyTemp = this->getBodyTemperature();        // Kazán előremenő hőmérséklete
    double returnTemp = element->getReTourTemperature(); // Elem visszatérő hőmérséklete

    double minTempDifference = 5.0; // Például, minimum 5 °C különbség szükséges

    // Ha a különbség az előremenő és visszatérő hőmérséklet között elég nagy, akkor még tud hőt biztosítani
    if ((bodyTemp - returnTemp) > minTempDifference)
    {
        return true; // Kazán tud még hőt biztosítani
    }

    return false; // A hőmérsékletkülönbség túl kicsi, a fűtési elem már nem tud több hőt felvenni
}

// State functions
void Puffer::state_CHARGE()
{
    activate(false);
};
void Puffer::state_DISCHARGE()
{
    activate(true);
};
void Puffer::state_FULL_CHARGED()
{
    deactivate();
};
void Puffer::state_STANDBY()
{
    deactivate();
};
void Puffer::state_OVERHEATED()
{
    deactivate();
};
void Puffer::state_FULL_DISCHARGED()
{
    deactivate();
};

// Transition Functions
bool Puffer::trch_STANDBY_to_CHARGE()
{
    bool retVal = false;
    if (requestedMode == Request::CHARGE)
    {
        retVal = true;
    }
    return retVal;
};
bool Puffer::trch_STANDBY_to_DISCHARGE()
{
    bool retVal = false;
    if (requestedMode == Request::DISCHARGE)
    {
        retVal = true;
    }
    return retVal;
};
bool Puffer::trch_CHARGE_to_STANDBY()
{
    return requestedMode == Request::STANDBY;
};
bool Puffer::trch_CHARGE_to_FULL_CHARGED() { return false; };
bool Puffer::trch_DISCHARGE_to_STANDBY()
{
    return requestedMode == Request::STANDBY;
};
bool Puffer::trch_DISCHARGE_to_FULL_DISCHARGED() { return false; };
bool Puffer::trch_DISCHARGE_to_CHARGE()
{
    bool retVal = false;
    if (requestedMode == Request::CHARGE)
    {
        retVal = true;
    }
    return retVal;
};
bool Puffer::trch_CHARGE_to_DISCHARGE()
{
    bool retVal = false;
    if (requestedMode == Request::DISCHARGE)
    {
        retVal = true;
    }
    return retVal;
};
bool Puffer::trch_FULL_DISCHARGED_to_CHARGE()
{
    bool retVal = false;
    if (requestedMode == Request::CHARGE)
    {
        retVal = true;
    }
    return retVal;
};
bool Puffer::trch_FULL_DISCHARGED_to_STANDBY()
{
    return requestedMode == Request::STANDBY;
};
bool Puffer::trch_FULL_CHARGED_to_STANDBY()
{
    return requestedMode == Request::STANDBY;
};
bool Puffer::trch_FULL_CHARGED_to_DISCHARGE()
{
    bool retVal = false;
    if (requestedMode == Request::DISCHARGE)
    {
        retVal = true;
    }
    return retVal;
};

void Puffer::requestCharge()
{
    requestedMode = Request::CHARGE;
}

void Puffer::requestDischarge()
{
    requestedMode = Request::DISCHARGE;
}

void Puffer::requestStandby()
{
    requestedMode = Request::STANDBY;
}

Puffer::Request Puffer::getRequest() const
{
    return requestedMode;
}
