#ifndef PUFFER_H
#define PUFFER_H

#include "HeatingElement.h"
#include "MessageBus.h"
#include <string> // std::string használatához
#include "common.h"
#include "../comon.h"
#include "stateMachine/stateMachine.h"

class Puffer : public HeatingElement
{
public:
    enum State
    {
        CHARGE,
        DISCHARGE,
        FULL_CHARGED,
        FULL_DISCHARGED,
        STANDBY,
        OVERHEATED,
        STATE_COUNT
    };

    // Külső vezérlési kérések
    void requestCharge();
    void requestDischarge();
    void requestStandby();

    // Kért működés lekérdezése
    enum class Request
    {
        NONE,
        CHARGE,
        DISCHARGE,
        STANDBY
    };

    Request getRequest() const;

    Puffer(MessageBus &bus, std::string name, std::string linkedHeatSourceName, float LinkedHeatSource_ActivationTourTemp); // std::string használata

    // Puffer egyedi vezérlése
    void moveEnergyToRadiators(float radiatorRequestTemp);

    bool hasStoredEnergy();

    void update();

    void run();

    bool canSupplyHeat(HeatingElement *element) override; // Override helyes használata

    std::string linkedHeatSourceName;
    float LinkedHeatSource_ActivationTourTemp;

    const char *getStateName(State state) const;
    State getState() const;

private:
    StateMachine<Puffer, State, STATE_COUNT> sm;

    Request requestedMode = Request::NONE;

    float tourTemp;
    float retourTemp;
    float bodyTemp;

    void state_CHARGE();
    void state_DISCHARGE();
    void state_FULL_CHARGED();
    void state_FULL_DISCHARGED();
    void state_STANDBY();
    void state_OVERHEATED();

    bool trch_STANDBY_to_CHARGE();
    bool trch_STANDBY_to_DISCHARGE();
    bool trch_CHARGE_to_STANDBY();
    bool trch_CHARGE_to_FULL_CHARGED();
    bool trch_DISCHARGE_to_STANDBY();
    bool trch_DISCHARGE_to_FULL_DISCHARGED();
    bool trch_DISCHARGE_to_CHARGE();
    bool trch_CHARGE_to_DISCHARGE();
    bool trch_FULL_DISCHARGED_to_CHARGE();
    bool trch_FULL_DISCHARGED_to_STANDBY();
    bool trch_FULL_CHARGED_to_STANDBY();
    bool trch_FULL_CHARGED_to_DISCHARGE();
};

#endif
