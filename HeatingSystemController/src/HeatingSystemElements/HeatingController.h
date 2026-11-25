#pragma once

#include <vector>
#include "stateMachine/stateMachine.h"
#include "Kazan.h"
#include "Puffer.h"
#include "Radiator.h"
#include <initializer_list>

class HeatingController
{
public:
    enum State
    {
        STANDBY,
        HEATING,
        CHARGING_BUFFER,
        CHARGING_BUFFER_AND_HEATING,
        DISCHARGING_BUFFER_AND_HEATING,
        PREHEATING_BOILER,
        FAULT,
        COOLING_BOILER,
        STATE_COUNT
    };

    HeatingController();

    void addBoiler(Kazan &kazan);
    void addBuffer(Puffer &puffer);
    void addConsumer(Radiator &radiator);

    void run();

    void setHeatingDemand(bool demand);
    bool getHeatingDemand() const;

    State getMode() const;
    const char *getModeName() const;
    std::string name;

    const char *getStateName(State state) const;
    State getState() const;

    void setElements(const std::vector<HeatingElement *> &elements);
    bool requestBoilerBufferOperation(const std::string &boilerName, bool cooling,
                                     std::string &error);
    bool cancelBoilerBufferOperation(const std::string &boilerName, std::string &error);
    bool isBoilerBufferOperationActive() const { return operationBoiler != nullptr; }

private:
    std::vector<HeatingElement *> elements;
    Kazan *operationBoiler = nullptr;
    Puffer *operationBuffer = nullptr;
    bool coolingOperation = false;
    std::vector<Pump *> operationPumps;
    bool bufferOperationPossible(Kazan &boiler, Puffer &buffer, bool cooling,
                                 bool starting, std::string &error);
    void applyOperationPumps();
    void finishBufferOperation();
    std::vector<Kazan *> boilers;
    std::vector<Puffer *> buffers;
    std::vector<Radiator *> consumers;

    StateMachine<HeatingController, State, STATE_COUNT> sm;

    bool heatingDemand;
    bool radiatorDemand;
    bool bufferCharging;
    bool bufferDischarging;
    bool overheatDetected;

    void updateConditions();
    void consumersNotify(bool heatAvailable);
    bool hasHeatAvailableForConsumers();
    bool shouldWaitForHeat();
    bool hasOverheat() const;
    bool canAnyPufferAcceptHeat();
    bool canAnyPufferHasHeat();

    bool hasKazanWithState(std::initializer_list<Kazan::State> states) const;
    bool hasPufferWithState(std::initializer_list<Puffer::State> states) const;
    bool hasRadiatorWithState(std::initializer_list<Radiator::State> states) const;

    // Állapotfüggvények
    void state_STANDBY();
    void state_HEATING();
    void state_CHARGING_BUFFER();
    void state_CHARGING_BUFFER_AND_HEATING();
    void state_DISCHARGING_BUFFER_AND_HEATING();
    void state_PREHEATING_BOILER();
    void state_COOLING_BOILER();
    void state_FAULT();

    // Minden különböző állapotpárhoz külön tranzíciós függvény.
    bool trch_STANDBY_to_HEATING();
    bool trch_STANDBY_to_CHARGING_BUFFER();
    bool trch_STANDBY_to_CHARGING_BUFFER_AND_HEATING();
    bool trch_STANDBY_to_DISCHARGING_BUFFER_AND_HEATING();
    bool trch_STANDBY_to_PREHEATING_BOILER();
    bool trch_STANDBY_to_FAULT();
    bool trch_HEATING_to_STANDBY();
    bool trch_HEATING_to_CHARGING_BUFFER();
    bool trch_HEATING_to_CHARGING_BUFFER_AND_HEATING();
    bool trch_HEATING_to_DISCHARGING_BUFFER_AND_HEATING();
    bool trch_HEATING_to_PREHEATING_BOILER();
    bool trch_HEATING_to_FAULT();
    bool trch_CHARGING_BUFFER_to_STANDBY();
    bool trch_CHARGING_BUFFER_to_HEATING();
    bool trch_CHARGING_BUFFER_to_CHARGING_BUFFER_AND_HEATING();
    bool trch_CHARGING_BUFFER_to_DISCHARGING_BUFFER_AND_HEATING();
    bool trch_CHARGING_BUFFER_to_PREHEATING_BOILER();
    bool trch_CHARGING_BUFFER_to_FAULT();
    bool trch_CHARGING_BUFFER_AND_HEATING_to_STANDBY();
    bool trch_CHARGING_BUFFER_AND_HEATING_to_HEATING();
    bool trch_CHARGING_BUFFER_AND_HEATING_to_CHARGING_BUFFER();
    bool trch_CHARGING_BUFFER_AND_HEATING_to_DISCHARGING_BUFFER_AND_HEATING();
    bool trch_CHARGING_BUFFER_AND_HEATING_to_PREHEATING_BOILER();
    bool trch_CHARGING_BUFFER_AND_HEATING_to_FAULT();
    bool trch_DISCHARGING_BUFFER_AND_HEATING_to_STANDBY();
    bool trch_DISCHARGING_BUFFER_AND_HEATING_to_HEATING();
    bool trch_DISCHARGING_BUFFER_AND_HEATING_to_CHARGING_BUFFER();
    bool trch_DISCHARGING_BUFFER_AND_HEATING_to_CHARGING_BUFFER_AND_HEATING();
    bool trch_DISCHARGING_BUFFER_AND_HEATING_to_PREHEATING_BOILER();
    bool trch_DISCHARGING_BUFFER_AND_HEATING_to_FAULT();
    bool trch_PREHEATING_BOILER_to_STANDBY();
    bool trch_PREHEATING_BOILER_to_HEATING();
    bool trch_PREHEATING_BOILER_to_CHARGING_BUFFER();
    bool trch_PREHEATING_BOILER_to_CHARGING_BUFFER_AND_HEATING();
    bool trch_PREHEATING_BOILER_to_DISCHARGING_BUFFER_AND_HEATING();
    bool trch_PREHEATING_BOILER_to_FAULT();
    bool trch_FAULT_to_STANDBY();
    bool trch_FAULT_to_HEATING();
    bool trch_FAULT_to_CHARGING_BUFFER();
    bool trch_FAULT_to_CHARGING_BUFFER_AND_HEATING();
    bool trch_FAULT_to_DISCHARGING_BUFFER_AND_HEATING();
    bool trch_FAULT_to_PREHEATING_BOILER();
};
