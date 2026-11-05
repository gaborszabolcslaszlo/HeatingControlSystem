#include "hardware_io.h"

#include "app_globals.h"

namespace
{
void applyPwmIO(int id, int duty)
{
#ifndef TESTINGOVER_API
  pwmController.setChannelDutyCycle(id, duty * 4096 / 100);
#endif
  IOMap[id] = duty;
  isDataNew = true;
}
}

void writePwmIO(int id, int duty)
{
  if (id >= 0 && id < static_cast<int>(manualIOOverrides.size()) &&
      manualIOOverrides[static_cast<size_t>(id)])
  {
    return;
  }
  applyPwmIO(id, duty);
}

void writeIO(int id, int value)
{
  if (id >= 0 && id < static_cast<int>(manualIOOverrides.size()) &&
      manualIOOverrides[static_cast<size_t>(id)])
  {
    return;
  }

  if (value > 0)
  {
#ifndef TESTINGOVER_API
    pwmController.setChannelDutyCycle(id, 4096);
#endif
    IOMap[id] = 100;
  }
  else
  {
#ifndef TESTINGOVER_API
    pwmController.setChannelDutyCycle(id, 0);
#endif
    IOMap[id] = 0;
  }
  isDataNew = true;
}

void setManualPwmIO(int id, int duty)
{
  if (id < 0 || id >= static_cast<int>(manualIOOverrides.size()))
  {
    return;
  }
  manualIOOverrides[static_cast<size_t>(id)] = true;
  applyPwmIO(id, duty);
}

void clearManualIO(int id)
{
  if (id >= 0 && id < static_cast<int>(manualIOOverrides.size()))
  {
    manualIOOverrides[static_cast<size_t>(id)] = false;
  }
}

