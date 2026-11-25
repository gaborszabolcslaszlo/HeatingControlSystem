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
        COOLING_FROM_PUFFER,
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

    struct BufferOperationConfig
    {
        bool enabled = false;
        std::string bufferName;
        unsigned long maxDurationMs = 300000UL;
        float minDeltaTemp = 2.0f;
        float stopDeltaTemp = 1.0f;
        float targetTemp = 38.0f;
        int pumpSignal = 100;
        int bufferPumpSignal = -1; // Hiányzó beállításnál a pumpSignal érvényes.
        std::vector<int> stopPumpIds;
    };
    BufferOperationConfig preheatFromPuffer;
    BufferOperationConfig coolingFromPuffer;

    bool beginBufferOperation(bool cooling);
    void cancelBufferOperation();
    bool isBufferOperationActive() const;
    bool isColdForBufferOperation();
    bool hasUnsafeTemperature();

private:
    bool startRequested = false;
    float retourTempProtValue;
    float tourTempProtValue;
    float activationThreshold;
    bool isRetourProtectionActive = false;
    bool isOverHeatProtectionActive = false;
    bool isKazanActive = false;

    float tourTemp = 0.0f;
    float retourTemp = 0.0f;
    float bodyTemp = 0.0f;
    unsigned long bufferOperationStartedAt = 0;

    FuelType fuelType;

    StateMachine<Kazan, State, STATE_COUNT> sm;

    // State functions
    void state_COOLD();
    void state_RETOUR_PROTECTION();
    void state_HEATING();
    void state_OVERHEATED();
    void state_PREHEAT_FROM_PUFFER();
    void state_COOLING_FROM_PUFFER();

    // Transition Functions
    bool trch_COOLD_to_RETOUR_PROTECTION();
    bool trch_RETOUR_PROTECTION_to_COOLD();
    bool trch_RETOUR_PROTECTION_to_HEATING();
    bool trch_HEATING_to_RETOUR_PROTECTION();
    bool trch_HEATING_to_OVERHEATED();
    bool trch_OVERHEATED_to_HEATING();
    bool trch_PREHEAT_FROM_PUFFER_to_RETOUR_PROTECTION();
    bool trch_PREHEAT_FROM_PUFFER_to_COOLD();
    bool trch_COOLING_FROM_PUFFER_to_COOLD();
    bool trch_bufferOperation_to_OVERHEATED();
};

#endif
