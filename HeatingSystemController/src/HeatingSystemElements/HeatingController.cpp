#include "HeatingController.h"

HeatingController::HeatingController()
    : sm(this, STANDBY),
      heatingDemand(false),
      radiatorDemand(false),
      bufferCharging(false),
      bufferDischarging(false),
      overheatDetected(false),
      name("Controller")
{
    // Állapotfüggvények regisztrálása
    sm.addState(STANDBY, &HeatingController::state_STANDBY);
    sm.addState(HEATING, &HeatingController::state_HEATING);
    sm.addState(CHARGING_BUFFER, &HeatingController::state_CHARGING_BUFFER);
    sm.addState(CHARGING_BUFFER_AND_HEATING, &HeatingController::state_CHARGING_BUFFER_AND_HEATING);
    sm.addState(DISCHARGING_BUFFER_AND_HEATING, &HeatingController::state_DISCHARGING_BUFFER_AND_HEATING);
    sm.addState(PREHEATING_BOILER, &HeatingController::state_PREHEATING_BOILER);
    sm.addState(FAULT, &HeatingController::state_FAULT);

    // Az összes különböző állapotpár tranzíciójának regisztrálása.
    sm.addTransition(STANDBY, HEATING, &HeatingController::trch_STANDBY_to_HEATING);
    sm.addTransition(STANDBY, CHARGING_BUFFER, &HeatingController::trch_STANDBY_to_CHARGING_BUFFER);
    sm.addTransition(STANDBY, CHARGING_BUFFER_AND_HEATING, &HeatingController::trch_STANDBY_to_CHARGING_BUFFER_AND_HEATING);
    sm.addTransition(STANDBY, DISCHARGING_BUFFER_AND_HEATING, &HeatingController::trch_STANDBY_to_DISCHARGING_BUFFER_AND_HEATING);
    sm.addTransition(STANDBY, PREHEATING_BOILER, &HeatingController::trch_STANDBY_to_PREHEATING_BOILER);
    sm.addTransition(STANDBY, FAULT, &HeatingController::trch_STANDBY_to_FAULT);
    sm.addTransition(HEATING, STANDBY, &HeatingController::trch_HEATING_to_STANDBY);
    sm.addTransition(HEATING, CHARGING_BUFFER, &HeatingController::trch_HEATING_to_CHARGING_BUFFER);
    sm.addTransition(HEATING, CHARGING_BUFFER_AND_HEATING, &HeatingController::trch_HEATING_to_CHARGING_BUFFER_AND_HEATING);
    sm.addTransition(HEATING, DISCHARGING_BUFFER_AND_HEATING, &HeatingController::trch_HEATING_to_DISCHARGING_BUFFER_AND_HEATING);
    sm.addTransition(HEATING, PREHEATING_BOILER, &HeatingController::trch_HEATING_to_PREHEATING_BOILER);
    sm.addTransition(HEATING, FAULT, &HeatingController::trch_HEATING_to_FAULT);
    sm.addTransition(CHARGING_BUFFER, STANDBY, &HeatingController::trch_CHARGING_BUFFER_to_STANDBY);
    sm.addTransition(CHARGING_BUFFER, HEATING, &HeatingController::trch_CHARGING_BUFFER_to_HEATING);
    sm.addTransition(CHARGING_BUFFER, CHARGING_BUFFER_AND_HEATING, &HeatingController::trch_CHARGING_BUFFER_to_CHARGING_BUFFER_AND_HEATING);
    sm.addTransition(CHARGING_BUFFER, DISCHARGING_BUFFER_AND_HEATING, &HeatingController::trch_CHARGING_BUFFER_to_DISCHARGING_BUFFER_AND_HEATING);
    sm.addTransition(CHARGING_BUFFER, PREHEATING_BOILER, &HeatingController::trch_CHARGING_BUFFER_to_PREHEATING_BOILER);
    sm.addTransition(CHARGING_BUFFER, FAULT, &HeatingController::trch_CHARGING_BUFFER_to_FAULT);
    sm.addTransition(CHARGING_BUFFER_AND_HEATING, STANDBY, &HeatingController::trch_CHARGING_BUFFER_AND_HEATING_to_STANDBY);
    sm.addTransition(CHARGING_BUFFER_AND_HEATING, HEATING, &HeatingController::trch_CHARGING_BUFFER_AND_HEATING_to_HEATING);
    sm.addTransition(CHARGING_BUFFER_AND_HEATING, CHARGING_BUFFER, &HeatingController::trch_CHARGING_BUFFER_AND_HEATING_to_CHARGING_BUFFER);
    sm.addTransition(CHARGING_BUFFER_AND_HEATING, DISCHARGING_BUFFER_AND_HEATING, &HeatingController::trch_CHARGING_BUFFER_AND_HEATING_to_DISCHARGING_BUFFER_AND_HEATING);
    sm.addTransition(CHARGING_BUFFER_AND_HEATING, PREHEATING_BOILER, &HeatingController::trch_CHARGING_BUFFER_AND_HEATING_to_PREHEATING_BOILER);
    sm.addTransition(CHARGING_BUFFER_AND_HEATING, FAULT, &HeatingController::trch_CHARGING_BUFFER_AND_HEATING_to_FAULT);
    sm.addTransition(DISCHARGING_BUFFER_AND_HEATING, STANDBY, &HeatingController::trch_DISCHARGING_BUFFER_AND_HEATING_to_STANDBY);
    sm.addTransition(DISCHARGING_BUFFER_AND_HEATING, HEATING, &HeatingController::trch_DISCHARGING_BUFFER_AND_HEATING_to_HEATING);
    sm.addTransition(DISCHARGING_BUFFER_AND_HEATING, CHARGING_BUFFER, &HeatingController::trch_DISCHARGING_BUFFER_AND_HEATING_to_CHARGING_BUFFER);
    sm.addTransition(DISCHARGING_BUFFER_AND_HEATING, CHARGING_BUFFER_AND_HEATING, &HeatingController::trch_DISCHARGING_BUFFER_AND_HEATING_to_CHARGING_BUFFER_AND_HEATING);
    sm.addTransition(DISCHARGING_BUFFER_AND_HEATING, PREHEATING_BOILER, &HeatingController::trch_DISCHARGING_BUFFER_AND_HEATING_to_PREHEATING_BOILER);
    sm.addTransition(DISCHARGING_BUFFER_AND_HEATING, FAULT, &HeatingController::trch_DISCHARGING_BUFFER_AND_HEATING_to_FAULT);
    sm.addTransition(PREHEATING_BOILER, STANDBY, &HeatingController::trch_PREHEATING_BOILER_to_STANDBY);
    sm.addTransition(PREHEATING_BOILER, HEATING, &HeatingController::trch_PREHEATING_BOILER_to_HEATING);
    sm.addTransition(PREHEATING_BOILER, CHARGING_BUFFER, &HeatingController::trch_PREHEATING_BOILER_to_CHARGING_BUFFER);
    sm.addTransition(PREHEATING_BOILER, CHARGING_BUFFER_AND_HEATING, &HeatingController::trch_PREHEATING_BOILER_to_CHARGING_BUFFER_AND_HEATING);
    sm.addTransition(PREHEATING_BOILER, DISCHARGING_BUFFER_AND_HEATING, &HeatingController::trch_PREHEATING_BOILER_to_DISCHARGING_BUFFER_AND_HEATING);
    sm.addTransition(PREHEATING_BOILER, FAULT, &HeatingController::trch_PREHEATING_BOILER_to_FAULT);
    sm.addTransition(FAULT, STANDBY, &HeatingController::trch_FAULT_to_STANDBY);
    sm.addTransition(FAULT, HEATING, &HeatingController::trch_FAULT_to_HEATING);
    sm.addTransition(FAULT, CHARGING_BUFFER, &HeatingController::trch_FAULT_to_CHARGING_BUFFER);
    sm.addTransition(FAULT, CHARGING_BUFFER_AND_HEATING, &HeatingController::trch_FAULT_to_CHARGING_BUFFER_AND_HEATING);
    sm.addTransition(FAULT, DISCHARGING_BUFFER_AND_HEATING, &HeatingController::trch_FAULT_to_DISCHARGING_BUFFER_AND_HEATING);
    sm.addTransition(FAULT, PREHEATING_BOILER, &HeatingController::trch_FAULT_to_PREHEATING_BOILER);
}

HeatingController::State HeatingController::getState() const
{
    return sm.getState();
}

const char *HeatingController::getStateName(State state) const
{
    switch (state)
    {
    case STANDBY:
        return "STANDBY";

    case HEATING:
        return "HEATING";

    case CHARGING_BUFFER:
        return "CHARGING_BUFFER";

    case CHARGING_BUFFER_AND_HEATING:
        return "CHARGING_BUFFER_AND_HEATING";

    case DISCHARGING_BUFFER_AND_HEATING:
        return "DISCHARGING_BUFFER_AND_HEATING";

    case PREHEATING_BOILER:
        return "PREHEATING_BOILER";

    case FAULT:
        return "FAULT";

    case STATE_COUNT:
    default:
        return "UNKNOWN";
    }
}

void HeatingController::addBoiler(Kazan &kazan)
{
    boilers.push_back(&kazan);
}

void HeatingController::addBuffer(Puffer &puffer)
{
    buffers.push_back(&puffer);
}

void HeatingController::addConsumer(Radiator &radiator)
{
    consumers.push_back(&radiator);
}

void HeatingController::run()
{
    for (Kazan *kazan : boilers)
        kazan->run();

    for (Puffer *puffer : buffers)
        puffer->run();

    for (Radiator *radiator : consumers)
        radiator->run();

    updateConditions();
    sm.run();
}

void HeatingController::setHeatingDemand(bool demand)
{
    heatingDemand = demand;
}

bool HeatingController::getHeatingDemand() const
{
    return heatingDemand || radiatorDemand;
}

#include <algorithm>

bool HeatingController::hasKazanWithState(
    std::initializer_list<Kazan::State> states) const
{
    return std::any_of(boilers.begin(), boilers.end(),
                       [&states](const Kazan *kazan)
                       {
                           return kazan != nullptr &&
                                  std::find(states.begin(), states.end(),
                                            kazan->getState()) != states.end();
                       });
}

bool HeatingController::hasPufferWithState(
    std::initializer_list<Puffer::State> states) const
{
    return std::any_of(buffers.begin(), buffers.end(),
                       [&states](const Puffer *puffer)
                       {
                           return puffer != nullptr &&
                                  std::find(states.begin(), states.end(),
                                            puffer->getState()) != states.end();
                       });
}

bool HeatingController::hasRadiatorWithState(
    std::initializer_list<Radiator::State> states) const
{
    return std::any_of(consumers.begin(), consumers.end(),
                       [&states](const Radiator *radiator)
                       {
                           return radiator != nullptr &&
                                  std::find(states.begin(), states.end(),
                                            radiator->getState()) != states.end();
                       });
}

HeatingController::State HeatingController::getMode() const
{
    return sm.getState();
}

const char *HeatingController::getModeName() const
{
    switch (getMode())
    {
    case STANDBY:
        return "STANDBY";
    case HEATING:
        return "HEATING";
    case CHARGING_BUFFER:
        return "CHARGING_BUFFER";
    case CHARGING_BUFFER_AND_HEATING:
        return "CHARGING_BUFFER_AND_HEATING";
    case DISCHARGING_BUFFER_AND_HEATING:
        return "DISCHARGING_BUFFER_AND_HEATING";
    case PREHEATING_BOILER:
        return "PREHEATING_BOILER";
    case FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}

bool HeatingController::canAnyPufferAcceptHeat()
{
    return std::any_of(
        buffers.begin(),
        buffers.end(),
        [](Puffer *puffer)
        {
            if (puffer == nullptr)
                return false;

            const auto state = puffer->getState();

            return state == Puffer::CHARGE ||
                   state == Puffer::DISCHARGE ||
                   state == Puffer::STANDBY ||
                   state == Puffer::FULL_DISCHARGED;
        });
}

bool HeatingController::canAnyPufferHasHeat()
{
    return std::any_of(
        buffers.begin(),
        buffers.end(),
        [](Puffer *puffer)
        {
            if (puffer == nullptr)
                return false;

            const auto state = puffer->getState();

            return (state == Puffer::CHARGE ||
                    state == Puffer::DISCHARGE ||
                    state == Puffer::STANDBY ||
                    state == Puffer::FULL_CHARGED) &&
                   puffer->hasStoredEnergy();
        });
}

void HeatingController::updateConditions()
{
    radiatorDemand = false;
    bufferCharging = false;
    bufferDischarging = false;

    for (Radiator *radiator : consumers)
    {
        if (radiator->getNeedHeating())
            radiatorDemand = true;
    }

    for (Puffer *puffer : buffers)
    {
        if (puffer->getState() == Puffer::CHARGE)
            bufferCharging = true;

        if (puffer->getState() == Puffer::DISCHARGE)
            bufferDischarging = true;
    }

    overheatDetected = hasOverheat();
}

bool HeatingController::hasOverheat() const
{
    for (const Kazan *kazan : boilers)
    {
        if (kazan->getState() == Kazan::OVERHEATED)
            return true;
    }

    for (const Puffer *puffer : buffers)
    {
        if (puffer->getState() == Puffer::OVERHEATED)
            return true;
    }

    return false;
}

void HeatingController::consumersNotify(bool heatAvailable)
{
    for (Radiator *radiator : consumers)
        radiator->setHeatingAvailable(heatAvailable);
}

bool HeatingController::hasHeatAvailableForConsumers()
{
    return hasKazanWithState({Kazan::HEATING}) || canAnyPufferHasHeat();
}

bool HeatingController::shouldWaitForHeat()
{
    return hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING}) &&
           !hasHeatAvailableForConsumers();
}

// Állapotfüggvények

void HeatingController::state_STANDBY()
{
    for (Kazan *kazan : boilers)
    {
        if (kazan->getFuelType() == Kazan::FuelType::GAS)
            kazan->requestStop();
    }

    for (Puffer *puffer : buffers)
        puffer->requestStandby();

    consumersNotify(false);
}

void HeatingController::state_HEATING()
{
    if (!boilers.empty())
        boilers[0]->requestStart();

    for (Puffer *puffer : buffers)
        puffer->requestStandby();

    consumersNotify(hasHeatAvailableForConsumers());
}

void HeatingController::state_CHARGING_BUFFER()
{
    for (Kazan *kazan : boilers)
        kazan->requestStart();

    for (Puffer *puffer : buffers)
        puffer->requestCharge();

    consumersNotify(false);
}

void HeatingController::state_CHARGING_BUFFER_AND_HEATING()
{
    for (Kazan *kazan : boilers)
        kazan->requestStart();

    for (Puffer *puffer : buffers)
        puffer->requestCharge();

    consumersNotify(hasHeatAvailableForConsumers());
}

void HeatingController::state_DISCHARGING_BUFFER_AND_HEATING()
{
    for (Kazan *kazan : boilers)
    {
        if (kazan->getFuelType() == Kazan::FuelType::GAS)
            kazan->requestStop();
    }

    for (Puffer *puffer : buffers)
        puffer->requestDischarge();

    consumersNotify(hasHeatAvailableForConsumers());
}

void HeatingController::state_PREHEATING_BOILER()
{
    // Az előmelegítési logika később kerül ide.
}

void HeatingController::state_FAULT()
{
    // Hiba esetén a gázkazán leállítását kérjük.
    // A fatüzelésű kazán hőelvezetését külön biztosítani kell.
    for (Kazan *kazan : boilers)
    {
        if (kazan->getFuelType() == Kazan::FuelType::GAS)
            kazan->requestStop();
    }

    consumersNotify(false);

    // A pufferek és a szivattyúk biztonsági működését
    // nem állítjuk le automatikusan.
}

// Minden tranzíció alapértelmezés szerint false értéket ad vissza.

bool HeatingController::trch_STANDBY_to_HEATING()
{
    return hasKazanWithState({Kazan::HEATING}) && hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING});
}

bool HeatingController::trch_STANDBY_to_CHARGING_BUFFER()
{
    return hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED}) &&
           !hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING}) &&
           canAnyPufferAcceptHeat();
}

bool HeatingController::trch_STANDBY_to_CHARGING_BUFFER_AND_HEATING()
{
    return hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED}) &&
           hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING}) &&
           canAnyPufferAcceptHeat(); // canAnyPufferHasHeat();
}

bool HeatingController::trch_STANDBY_to_DISCHARGING_BUFFER_AND_HEATING()
{
    return !hasKazanWithState({Kazan::HEATING, Kazan::RETOUR_PROTECTION}) && hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING}) && canAnyPufferHasHeat();
}

bool HeatingController::trch_STANDBY_to_PREHEATING_BOILER()
{
    return false;
}

bool HeatingController::trch_STANDBY_to_FAULT()
{
    return false;
}

bool HeatingController::trch_HEATING_to_STANDBY()
{
    return shouldWaitForHeat() ||
           (!hasKazanWithState({Kazan::HEATING, Kazan::RETOUR_PROTECTION, Kazan::PREHEAT_FROM_PUFFER}) &&
            !hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING}));
}

bool HeatingController::trch_HEATING_to_CHARGING_BUFFER()
{
    return hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED}) && !hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING}) && canAnyPufferAcceptHeat();
}

bool HeatingController::trch_HEATING_to_CHARGING_BUFFER_AND_HEATING()
{
    return hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED}) && hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING}) && canAnyPufferAcceptHeat();
}

bool HeatingController::trch_HEATING_to_DISCHARGING_BUFFER_AND_HEATING()
{
    return hasKazanWithState({Kazan::RETOUR_PROTECTION}) && hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING}) && canAnyPufferHasHeat();
}
bool HeatingController::trch_HEATING_to_PREHEATING_BOILER()
{
    return false;
}

bool HeatingController::trch_HEATING_to_FAULT()
{
    return false;
}

bool HeatingController::trch_CHARGING_BUFFER_to_STANDBY()
{
    return shouldWaitForHeat() ||
           (!hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED, Kazan::RETOUR_PROTECTION, Kazan::PREHEAT_FROM_PUFFER}) &&
            !hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING}));
}

bool HeatingController::trch_CHARGING_BUFFER_to_HEATING()
{
    return hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED}) && hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING});
}

bool HeatingController::trch_CHARGING_BUFFER_to_CHARGING_BUFFER_AND_HEATING()
{
    return hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED}) && hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING});
}

bool HeatingController::trch_CHARGING_BUFFER_to_DISCHARGING_BUFFER_AND_HEATING()
{
    return hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING});
}

bool HeatingController::trch_CHARGING_BUFFER_to_PREHEATING_BOILER()
{
    return false;
}

bool HeatingController::trch_CHARGING_BUFFER_to_FAULT()
{
    return false;
}

bool HeatingController::trch_CHARGING_BUFFER_AND_HEATING_to_STANDBY()
{
    return shouldWaitForHeat() ||
           (!hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED, Kazan::RETOUR_PROTECTION}) &&
            !hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING}));
}
bool HeatingController::trch_CHARGING_BUFFER_AND_HEATING_to_HEATING()
{
    return hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED}) && hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING}) && !canAnyPufferAcceptHeat();
}

bool HeatingController::trch_CHARGING_BUFFER_AND_HEATING_to_CHARGING_BUFFER()
{
    return hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED}) && !hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING}) && canAnyPufferAcceptHeat();
}

bool HeatingController::trch_CHARGING_BUFFER_AND_HEATING_to_DISCHARGING_BUFFER_AND_HEATING()
{
    return !hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED}) && hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING}) && canAnyPufferHasHeat();
}

bool HeatingController::trch_CHARGING_BUFFER_AND_HEATING_to_PREHEATING_BOILER()
{
    return false;
}

bool HeatingController::trch_CHARGING_BUFFER_AND_HEATING_to_FAULT()
{
    return false;
}

bool HeatingController::trch_DISCHARGING_BUFFER_AND_HEATING_to_STANDBY()
{
    return shouldWaitForHeat() ||
           (!hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED, Kazan::RETOUR_PROTECTION}) &&
            !hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING}));
}

bool HeatingController::trch_DISCHARGING_BUFFER_AND_HEATING_to_HEATING()
{
    return hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED}) && hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING}) && !canAnyPufferAcceptHeat();
}

bool HeatingController::trch_DISCHARGING_BUFFER_AND_HEATING_to_CHARGING_BUFFER()
{
    return hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED}) && !hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING}) && canAnyPufferAcceptHeat();
}

bool HeatingController::trch_DISCHARGING_BUFFER_AND_HEATING_to_CHARGING_BUFFER_AND_HEATING()
{
    return hasKazanWithState({Kazan::HEATING, Kazan::OVERHEATED}) && hasRadiatorWithState({Radiator::STATE_WAITING_FOR_HEATING, Radiator::STATE_RECEIVING_HEATING}) && canAnyPufferAcceptHeat();
}

bool HeatingController::trch_DISCHARGING_BUFFER_AND_HEATING_to_PREHEATING_BOILER()
{
    return false;
}

bool HeatingController::trch_DISCHARGING_BUFFER_AND_HEATING_to_FAULT()
{
    return false;
}

bool HeatingController::trch_PREHEATING_BOILER_to_STANDBY()
{
    return false;
}

bool HeatingController::trch_PREHEATING_BOILER_to_HEATING()
{
    return false;
}

bool HeatingController::trch_PREHEATING_BOILER_to_CHARGING_BUFFER()
{
    return false;
}

bool HeatingController::trch_PREHEATING_BOILER_to_CHARGING_BUFFER_AND_HEATING()
{
    return false;
}

bool HeatingController::trch_PREHEATING_BOILER_to_DISCHARGING_BUFFER_AND_HEATING()
{
    return false;
}

bool HeatingController::trch_PREHEATING_BOILER_to_FAULT()
{
    return false;
}

bool HeatingController::trch_FAULT_to_STANDBY()
{
    return false;
}

bool HeatingController::trch_FAULT_to_HEATING()
{
    return false;
}

bool HeatingController::trch_FAULT_to_CHARGING_BUFFER()
{
    return false;
}

bool HeatingController::trch_FAULT_to_CHARGING_BUFFER_AND_HEATING()
{
    return false;
}

bool HeatingController::trch_FAULT_to_DISCHARGING_BUFFER_AND_HEATING()
{
    return false;
}

bool HeatingController::trch_FAULT_to_PREHEATING_BOILER()
{
    return false;
}
