#ifndef KAZAN_H
#define KAZAN_H

#include "HeatingElement.h"
#include "MessageBus.h"
#include <string>
#include "common.h"
#include "../comon.h"
#include "stateMachine/stateMachine.h"

class Kazan : public HeatingElement
{
public:
    enum State
    {
        COOLD,
        RETOUR_PROTECTION,
        HEATING,
        OVERHEATED,
        PREHEAT_FROM_PUFFER,
        STATE_COUNT
    };

    enum class FuelType
    {
        GAS,
        WOOD
    };

    Kazan(MessageBus &bus, std::string name, float retourTempProtValue, float tourTempProtValue, float activationThreshold, FuelType type);

    void checkRetourLowTemperatureProtection();

    void onMessageReceived(const std::string &message) override;
    void update();
    void run();
    bool getIsOverHeatProtectionActive() const;

    bool getIsRetourProtectionActive() const;

    bool getIsKazanActive() const;

    const char *getStateName(State state) const;
    State getState() const;
    FuelType getFuelType() const;

    // Külső vezérlési kérések
    void requestStart();
    void requestStop();
    bool isStartRequested() const;

private:
    bool startRequested = false;
    float retourTempProtValue;
    float tourTempProtValue;
    float activationThreshold;
    bool isRetourProtectionActive;
    bool isOverHeatProtectionActive;
    bool isKazanActive;

    float tourTemp;
    float retourTemp;
    float bodyTemp;

    FuelType fuelType;

    StateMachine<Kazan, State, STATE_COUNT> sm;

    // State functions
    void state_COOLD();
    void state_RETOUR_PROTECTION();
    void state_HEATING();
    void state_OVERHEATED();
    void state_PREHEAT_FROM_PUFFER();

    // Transition Functions
    bool trch_COOLD_to_RETOUR_PROTECTION();
    bool trch_RETOUR_PROTECTION_to_COOLD();
    bool trch_RETOUR_PROTECTION_to_HEATING();
    bool trch_HEATING_to_RETOUR_PROTECTION();
    bool trch_HEATING_to_OVERHEATED();
    bool trch_OVERHEATED_to_HEATING();
    bool trch_PREHEAT_FROM_PUFFER_to_RETOUR_PROTECTION();
    bool trch_PREHEAT_FROM_PUFFER_to_COOLD();
};

#endif