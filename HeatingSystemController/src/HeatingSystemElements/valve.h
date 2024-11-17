#ifndef VALVE_H
#define VALVE_H

#include "../comon.h"
#include <string>
#include "common.h"
#include "../comon.h"
// Valve class
class Valve
{
public:
    std::string name;
    int maxControlSig;
    int minControlSig;
    std::string workingMode;

    Valve();

    Valve(std::string name, int maxControlSig, int minControlSig, std::string workingMode);

    bool validate();

    void print();
};

#endif