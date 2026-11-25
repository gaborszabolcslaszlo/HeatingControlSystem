#include "system_health.h"
#include <atomic>

namespace
{
  std::atomic<uint32_t> activeConditions{0};
  std::atomic<bool> boilerTestMode{false};
}

bool boilerTestModeAvailable()
{
#ifdef TESTINGOVER_API
  return true;
#else
  return false;
#endif
}

bool isBoilerTestModeEnabled() { return boilerTestMode.load(); }

bool setBoilerTestMode(bool enabled)
{
  if (enabled && !boilerTestModeAvailable())
    return false;
  boilerTestMode.store(enabled);
  return true;
}

uint32_t getBoilerOperationHealthConditions()
{
  const uint32_t conditions = getHealthConditions();
  // Szimulált szenzorérték és IO használható a tesztfirmware-ben.
  // A túlmelegedés és a vezérlőhiba továbbra is blokkol.
  return isBoilerTestModeEnabled()
      ? conditions & ~(HEALTH_SENSOR_UNAVAILABLE | HEALTH_PWM_UNAVAILABLE)
      : conditions;
}

void setHealthCondition(HealthCondition condition, bool active)
{
  const uint32_t bit = static_cast<uint32_t>(condition);
  if (active) activeConditions.fetch_or(bit, std::memory_order_relaxed);
  else activeConditions.fetch_and(~bit, std::memory_order_relaxed);
}

uint32_t getHealthConditions() { return activeConditions.load(std::memory_order_relaxed); }

const char *healthColor(uint32_t conditions)
{
  return (conditions & HEALTH_RED_MASK) ? "red" : (conditions & HEALTH_YELLOW_MASK) ? "yellow" : "green";
}
