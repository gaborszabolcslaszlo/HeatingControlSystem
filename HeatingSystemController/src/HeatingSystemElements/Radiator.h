#ifndef RADIATOR_H
#define RADIATOR_H

#include "HeatingElement.h"
#include "MessageBus.h"
#include "common.h"
#include "../comon.h"
#include "stateMachine/stateMachine.h"

class Radiator : public HeatingElement
{
public:
    enum State
    {
        STATE_STANDBY,
        STATE_WAITING_FOR_HEATING,
        STATE_RECEIVING_HEATING,
        STATE_OVERHEATED,
        STATE_FAULT,
        STATE_COUNT
    };

    Radiator(MessageBus &bus, std::string name);

    void run();

    State getState() const;

    // A központi vezérlő által küldött kérések
    void requestHeating();
    void requestStandby();

    // A tényleges fűtés és a hibák jelzése
    void setHeatingAvailable(bool available);
    void setOverheated(bool overheated);
    void setFault(bool fault);

    bool needsHeating() const;

    // Radiátor egyedi vezérlése
    void manageHeating(float desiredTemp);

    void update();

    const char *getStateName(State state) const;

private:
    StateMachine<Radiator, State, STATE_COUNT> sm;
    float tourTemp;
    float retourTemp;
    float bodyTemp;

    bool heatingAvailable;
    bool overheated;
    bool fault;

    // Állapotfüggvények
    void state_STANDBY();
    void state_WAITING_FOR_HEATING();
    void state_RECEIVING_HEATING();
    void state_OVERHEATED();
    void state_FAULT();

    // Átmeneti feltételek
    // State transitions
    bool trch_STANDBY_to_WAITING_FOR_HEATING();
    bool trch_STANDBY_to_OVERHEATED();
    bool trch_STANDBY_to_FAULT();

    bool trch_WAITING_FOR_HEATING_to_RECEIVING_HEATING();
    bool trch_WAITING_FOR_HEATING_to_STANDBY();
    bool trch_WAITING_FOR_HEATING_to_OVERHEATED();
    bool trch_WAITING_FOR_HEATING_to_FAULT();

    bool trch_RECEIVING_HEATING_to_WAITING_FOR_HEATING();
    bool trch_RECEIVING_HEATING_to_STANDBY();
    bool trch_RECEIVING_HEATING_to_OVERHEATED();
    bool trch_RECEIVING_HEATING_to_FAULT();
};
#endif