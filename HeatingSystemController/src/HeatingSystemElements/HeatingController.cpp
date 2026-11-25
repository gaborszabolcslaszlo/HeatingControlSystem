#include "HeatingController.h"
#include <algorithm>
#include <cmath>

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
    sm.addState(COOLING_BOILER, &HeatingController::state_COOLING_BOILER);
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
    case COOLING_BOILER:
        return "COOLING_BOILER";

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
    if (operationBoiler)
    {
        std::string error;
        if (!bufferOperationPossible(*operationBoiler, *operationBuffer,
                                     coolingOperation, false, error))
        {
            logInfo("[BOILER] Buffer operation stopped: %s\n", error.c_str());
            operationBoiler->cancelBufferOperation();
            finishBufferOperation();
        }
    }

    for (Kazan *kazan : boilers)
        kazan->run();

    if (operationBoiler && !operationBoiler->isBufferOperationActive())
        finishBufferOperation();

    for (Puffer *puffer : buffers)
        puffer->run();

    for (Radiator *radiator : consumers)
        radiator->run();

    updateConditions();
    if (operationBoiler)
    {
        // A kézi művelet alatt a normál tranzíciók nem szakítják meg a kérést.
        if (coolingOperation)
            state_COOLING_BOILER();
        else
            state_PREHEATING_BOILER();
        return;
    }
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
    case COOLING_BOILER:
        return "COOLING_BOILER";
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
    applyOperationPumps();
}

void HeatingController::state_COOLING_BOILER()
{
    applyOperationPumps();
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

void HeatingController::setElements(const std::vector<HeatingElement *> &managedElements)
{
    elements = managedElements;
}

namespace
{
    bool refreshOperationSensors(HeatingElement &element)
    {
        if (element.tourSensors.empty() || element.retourSensors.empty() ||
            (element.ElemType == HeatingElementType::KAZAN && element.bodySensors.empty()))
            return false;
        for (Sensor &sensor : element.sensors)
        {
            const auto reading = Sensor::SensorsValue.find(sensor.id);
            if (reading == Sensor::SensorsValue.end() || !std::isfinite(reading->second) ||
                reading->second + sensor.offset < -55.0f || reading->second + sensor.offset > 125.0f)
                return false;
            sensor.update();
        }
        return true;
    }
}

bool HeatingController::bufferOperationPossible(Kazan &boiler, Puffer &buffer,
                                               bool cooling, bool starting,
                                               std::string &error)
{
    const auto &config = cooling ? boiler.coolingFromPuffer : boiler.preheatFromPuffer;
    if (!config.enabled || boiler.getFuelType() != Kazan::FuelType::WOOD)
        error = "Operation is disabled or boiler is not wood-fired";
    else if (getState() == FAULT || hasOverheat())
        error = "Controller fault or overheat";
    else if (!refreshOperationSensors(boiler) || !refreshOperationSensors(buffer))
        error = "Missing or invalid boiler/buffer temperature";
    else if (boiler.hasUnsafeTemperature() || buffer.getTourTemperature() >= 95.0f ||
             buffer.getReTourTemperature() >= 95.0f || buffer.getBodyTemperature() >= 95.0f)
        error = "Boiler or buffer temperature is unsafe";
    else if (starting && !boiler.isColdForBufferOperation())
        error = "Boiler must be COOLD and below its activation temperature";
    else
    {
        for (Kazan *other : boilers)
        {
            if (other != &boiler && other->getState() != Kazan::COOLD)
            {
                error = "Another boiler is active";
                return false;
            }
        }
        const float delta = starting ? config.minDeltaTemp : config.stopDeltaTemp;
        if (!starting && !cooling && boiler.getReTourTemperature() >= config.targetTemp)
            return true;
        if (cooling)
        {
            if (boiler.getTourTemperature() <= boiler.getReTourTemperature() + config.stopDeltaTemp)
                error = "Boiler supply has already cooled to return temperature";
            else if (boiler.getTourTemperature() <= buffer.getReTourTemperature() + delta)
                error = "Buffer return is not cold enough to cool the boiler";
            else
                return true;
        }
        else if (starting && boiler.getReTourTemperature() >= config.targetTemp)
            error = "Boiler has already reached the preheat target";
        else if (buffer.getTourTemperature() <=
                 std::max(boiler.getTourTemperature(), boiler.getReTourTemperature()) + delta)
            error = "Buffer supply is not warm enough to preheat the boiler";
        else
            return true;
    }
    return false;
}

bool HeatingController::requestBoilerBufferOperation(const std::string &boilerName,
                                                     bool cooling, std::string &error)
{
    if (operationBoiler)
    {
        error = "A boiler buffer operation is already running";
        return false;
    }
    Kazan *boiler = nullptr;
    for (Kazan *candidate : boilers)
        if (candidate->name == boilerName)
            boiler = candidate;
    if (!boiler)
    {
        error = "Unknown boiler";
        return false;
    }
    const auto &config = cooling ? boiler->coolingFromPuffer : boiler->preheatFromPuffer;
    Puffer *buffer = nullptr;
    for (Puffer *candidate : buffers)
        if (candidate->name == config.bufferName)
            buffer = candidate;
    if (!buffer)
    {
        error = "Configured buffer is missing";
        return false;
    }
    if (!bufferOperationPossible(*boiler, *buffer, cooling, true, error))
        return false;

    // Csak a kazán és a kijelölt puffer töltőágának pumpáit indítjuk.
    std::vector<Pump *> runPumps;
    std::vector<int> runSignals;
    for (HeatingElement *element : {static_cast<HeatingElement *>(boiler), static_cast<HeatingElement *>(buffer)})
    {
        size_t count = 0;
        for (Pump &pump : element->pumps)
        {
            if (pump.dischargepump)
                continue;
            const int signal = element == buffer && pump.workingMode == PumpWorkingMode::PWM &&
                                       config.bufferPumpSignal >= 0
                                   ? config.bufferPumpSignal : config.pumpSignal;
            if (pump.workingMode == PumpWorkingMode::UNKNOWN ||
                signal < 1 || signal > 100 ||
                pump.maxControlSig < signal || signal < pump.minControlSig ||
                std::find(config.stopPumpIds.begin(), config.stopPumpIds.end(), pump.id) != config.stopPumpIds.end())
            {
                error = "Pump signal or stop list conflicts with the circulation pumps";
                return false;
            }
            runPumps.push_back(&pump);
            runSignals.push_back(signal);
            ++count;
        }
        if (count == 0)
        {
            error = "Boiler and buffer both need a circulation pump";
            return false;
        }
    }
    std::vector<Pump *> stopPumps;
    for (int id : config.stopPumpIds)
    {
        Pump *matched = nullptr;
        for (HeatingElement *element : elements)
            for (Pump &pump : element->pumps)
                if (pump.id == id)
                    matched = &pump;
        if (!matched)
        {
            error = "Unknown pump IO in stopPumpIds";
            return false;
        }
        stopPumps.push_back(matched);
    }
    if (!boiler->beginBufferOperation(cooling))
    {
        error = "Boiler cannot enter the requested operation";
        return false;
    }
    operationBoiler = boiler;
    operationBuffer = buffer;
    coolingOperation = cooling;
    sm.setState(cooling ? COOLING_BOILER : PREHEATING_BOILER);
    operationPumps = stopPumps;
    operationPumps.insert(operationPumps.end(), runPumps.begin(), runPumps.end());
    for (Pump *pump : stopPumps)
        pump->setOperationOverride(0);
    for (size_t i = 0; i < runPumps.size(); ++i)
        runPumps[i]->setOperationOverride(runSignals[i]);
    applyOperationPumps();
    logInfo("[BOILER] %s: %s started\n", boilerName.c_str(), cooling ? "cooling" : "preheat");
    return true;
}

void HeatingController::applyOperationPumps()
{
    if (!operationBoiler)
        return;
    operationBuffer->requestCharge();
    for (Pump *pump : operationPumps)
        pump->update();
}

void HeatingController::finishBufferOperation()
{
    for (Pump *pump : operationPumps)
    {
        pump->setOperationOverride(-1);
        pump->update();
    }
    operationPumps.clear();
    operationBuffer->requestStandby();
    operationBoiler = nullptr;
    operationBuffer = nullptr;
    sm.setState(STANDBY);
}

bool HeatingController::cancelBoilerBufferOperation(const std::string &boilerName, std::string &error)
{
    if (!operationBoiler || operationBoiler->name != boilerName)
    {
        error = "No buffer operation is running for this boiler";
        return false;
    }
    operationBoiler->cancelBufferOperation();
    finishBufferOperation();
    logInfo("[BOILER] %s: buffer operation cancelled\n", boilerName.c_str());
    return true;
}
