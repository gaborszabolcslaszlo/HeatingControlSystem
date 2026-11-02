#include "hardware_io.h"

#include "app_globals.h"
void writePwmIO(int id, int duty)
{
  pwmController.setChannelDutyCycle(id, duty * 4096 / 100);
  IOMap[id] = duty;
  isDataNew = true;
}

void writeIO(int id, int value)
{
  if (value > 0)
  {
    pwmController.setChannelDutyCycle(id, 4096);
    IOMap[id] = 100;
  }
  else
  {
    pwmController.setChannelDutyCycle(id, 0);
    IOMap[id] = 0;
  }
  isDataNew = true;
}

