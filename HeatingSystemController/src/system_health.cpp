#include "system_health.h"
#include <atomic>

namespace { std::atomic<uint32_t> activeConditions{0}; }

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
