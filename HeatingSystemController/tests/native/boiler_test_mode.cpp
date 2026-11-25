#include "system_health.h"
#include <cassert>
#include <cstdio>

int main()
{
    assert(!isBoilerTestModeEnabled());
    const uint32_t simulatedHardware = HEALTH_SENSOR_UNAVAILABLE | HEALTH_PWM_UNAVAILABLE;
    setHealthCondition(HEALTH_SENSOR_UNAVAILABLE, true);
    setHealthCondition(HEALTH_PWM_UNAVAILABLE, true);
    assert(getBoilerOperationHealthConditions() == simulatedHardware);
#ifdef TESTINGOVER_API
    assert(boilerTestModeAvailable());
    assert(setBoilerTestMode(true));
    assert(isBoilerTestModeEnabled());
    assert(getBoilerOperationHealthConditions() == 0);
    assert(getHealthConditions() == simulatedHardware);
    // A valódi hibák tesztmódban is blokkolnak, a nyers diagnosztika megmarad.
    setHealthCondition(HEALTH_OVERHEAT, true);
    setHealthCondition(HEALTH_STATE_FAULT, true);
    assert(getBoilerOperationHealthConditions() == (HEALTH_OVERHEAT | HEALTH_STATE_FAULT));
    assert(healthColor(getHealthConditions())[0] == 'r');
    assert(setBoilerTestMode(false));
    assert(getBoilerOperationHealthConditions() == getHealthConditions());
#else
    assert(!boilerTestModeAvailable());
    assert(!setBoilerTestMode(true));
    assert(!isBoilerTestModeEnabled());
    assert(getBoilerOperationHealthConditions() == simulatedHardware);
    assert(setBoilerTestMode(false));
#endif
    puts("Boiler test mode checks passed");
}
