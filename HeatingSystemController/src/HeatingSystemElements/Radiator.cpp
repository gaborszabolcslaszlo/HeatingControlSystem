#include "Radiator.h"

Radiator::Radiator(MessageBus &bus, std::string name) : HeatingElement(bus, name), sm(this, STATE_STANDBY),
                                                        heatingAvailable(false),
                                                        overheated(false),
                                                        fault(false)
{
    // Állapotfüggvények
    sm.addState(STATE_STANDBY, &Radiator::state_STANDBY);

    sm.addState(STATE_WAITING_FOR_HEATING, &Radiator::state_WAITING_FOR_HEATING);

    sm.addState(STATE_RECEIVING_HEATING, &Radiator::state_RECEIVING_HEATING);

    sm.addState(STATE_OVERHEATED, &Radiator::state_OVERHEATED);

    sm.addState(STATE_FAULT, &Radiator::state_FAULT);

    // STANDBY átmenetek
    sm.addTransition(STATE_STANDBY, STATE_FAULT, &Radiator::trch_STANDBY_to_FAULT);

    sm.addTransition(STATE_STANDBY, STATE_OVERHEATED, &Radiator::trch_STANDBY_to_OVERHEATED);

    sm.addTransition(STATE_STANDBY, STATE_WAITING_FOR_HEATING, &Radiator::trch_STANDBY_to_WAITING_FOR_HEATING);

    // WAITING_FOR_HEATING átmenetek
    sm.addTransition(STATE_WAITING_FOR_HEATING, STATE_FAULT, &Radiator::trch_WAITING_FOR_HEATING_to_FAULT);

    sm.addTransition(STATE_WAITING_FOR_HEATING, STATE_OVERHEATED, &Radiator::trch_WAITING_FOR_HEATING_to_OVERHEATED);

    sm.addTransition(STATE_WAITING_FOR_HEATING, STATE_RECEIVING_HEATING, &Radiator::trch_WAITING_FOR_HEATING_to_RECEIVING_HEATING);

    sm.addTransition(STATE_WAITING_FOR_HEATING, STATE_STANDBY, &Radiator::trch_WAITING_FOR_HEATING_to_STANDBY);

    // RECEIVING_HEATING átmenetek
    sm.addTransition(STATE_RECEIVING_HEATING, STATE_FAULT, &Radiator::trch_RECEIVING_HEATING_to_FAULT);

    sm.addTransition(STATE_RECEIVING_HEATING, STATE_OVERHEATED, &Radiator::trch_RECEIVING_HEATING_to_OVERHEATED);

    sm.addTransition(STATE_RECEIVING_HEATING, STATE_STANDBY, &Radiator::trch_RECEIVING_HEATING_to_STANDBY);

    sm.addTransition(STATE_RECEIVING_HEATING, STATE_WAITING_FOR_HEATING, &Radiator::trch_RECEIVING_HEATING_to_WAITING_FOR_HEATING);
}

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

const char *Radiator::getStateName(State state) const
{
    switch (state)
    {
    case STATE_STANDBY:
        return "STANDBY";

    case STATE_WAITING_FOR_HEATING:
        return "WAITING_FOR_HEATING";

    case STATE_RECEIVING_HEATING:
        return "RECEIVING_HEATING";

    case STATE_OVERHEATED:
        return "OVERHEATED";

    case STATE_FAULT:
        return "FAULT";

    default:
        return "UNKNOWN";
    }
}

void Radiator::run()
{
    HeatingElement::update();
    tourTemp = HeatingElement::getTourTemperature();
    retourTemp = HeatingElement::getReTourTemperature();
    bodyTemp = HeatingElement::getBodyTemperature();

    sm.run();

    HeatingElement::setStateValue(name, "state", getStateName(sm.getState()));
}

void Radiator::state_STANDBY()
{
    deactivate();
};
void Radiator::state_WAITING_FOR_HEATING()
{
    deactivate();
};
void Radiator::state_RECEIVING_HEATING()
{
    activate();
};
void Radiator::state_OVERHEATED() {};
void Radiator::state_FAULT() {};

bool Radiator::trch_STANDBY_to_WAITING_FOR_HEATING()
{
    return getNeedHeating();
}

bool Radiator::trch_STANDBY_to_OVERHEATED()
{
    bool retVal = false;
    if (bodyTemp > 60)
    {
        retVal = true;
    }
    return retVal;
}

bool Radiator::trch_STANDBY_to_FAULT()
{
    return false;
}

bool Radiator::trch_WAITING_FOR_HEATING_to_RECEIVING_HEATING()
{
    return heatingAvailable;
}

bool Radiator::trch_WAITING_FOR_HEATING_to_STANDBY()
{
    // A felhasználói fűtési igény megszűnése elsőbbséget élvez.
    // Attól, hogy a rendszer még hőt tudna biztosítani, a radiátor
    // kérés nélkül nem maradhat WAITING állapotban.
    return !getNeedHeating();
}

bool Radiator::trch_WAITING_FOR_HEATING_to_OVERHEATED()
{
    bool retVal = false;
    if (bodyTemp > 60)
    {
        retVal = true;
    }
    return retVal;
}

bool Radiator::trch_WAITING_FOR_HEATING_to_FAULT()
{
    return false;
}

bool Radiator::trch_RECEIVING_HEATING_to_WAITING_FOR_HEATING()
{
    return !heatingAvailable && getNeedHeating();
}

bool Radiator::trch_RECEIVING_HEATING_to_STANDBY()
{
    // Ha nincs helyi fűtési igény, a radiátor azonnal álljon vissza.
    // A heatingAvailable csak azt jelzi, hogy lenne hőforrás, nem azt,
    // hogy a radiátornak ténylegesen fűtenie kell.
    return !getNeedHeating();
}

bool Radiator::trch_RECEIVING_HEATING_to_OVERHEATED()
{
    return false;
}

bool Radiator::trch_RECEIVING_HEATING_to_FAULT()
{
    bool retVal = false;
    if (bodyTemp > 60)
    {
        retVal = true;
    }
    return retVal;
}

void Radiator::setHeatingAvailable(bool available)
{
    heatingAvailable = available;
}

Radiator::State Radiator::getState() const
{
    return sm.getState();
}
