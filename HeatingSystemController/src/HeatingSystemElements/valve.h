#ifndef VALVE_H
#define VALVE_H

#include <WString.h>
#include <Arduino.h>

// Valve class
class Valve
{
public:
    String name;
    int maxControlSig;
    int minControlSig;
    String workingMode;

    Valve();

    Valve(String name, int maxControlSig, int minControlSig, String workingMode);

    bool validate();

    void print();
};

#endif