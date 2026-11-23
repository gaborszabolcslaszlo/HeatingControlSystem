#pragma once
#include <stdint.h>

// Active conditions, not latched history. Red always overrides yellow.
enum HealthCondition : uint32_t
{
  HEALTH_SENSOR_UNAVAILABLE = 1u << 0,
  HEALTH_PWM_UNAVAILABLE = 1u << 1,
  HEALTH_STATE_FAULT = 1u << 2,
  HEALTH_SENSOR_RANGE = 1u << 8,
  HEALTH_SENSOR_SPIKE = 1u << 9,
  HEALTH_OVERHEAT = 1u << 10,
  HEALTH_MEMORY_FRAGMENTED = 1u << 11,
  HEALTH_FLASH_LOW = 1u << 12
};
constexpr uint32_t HEALTH_RED_MASK = HEALTH_SENSOR_UNAVAILABLE | HEALTH_PWM_UNAVAILABLE | HEALTH_STATE_FAULT;
constexpr uint32_t HEALTH_YELLOW_MASK = HEALTH_SENSOR_RANGE | HEALTH_SENSOR_SPIKE | HEALTH_OVERHEAT | HEALTH_MEMORY_FRAGMENTED | HEALTH_FLASH_LOW;

void setHealthCondition(HealthCondition condition, bool active);
uint32_t getHealthConditions();
const char *healthColor(uint32_t conditions);
