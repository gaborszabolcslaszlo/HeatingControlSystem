// HeatingSystem class (contains the entire system)
#ifndef HEATINGSYSTEM_H
#define HEATINGSYSTEM_H

#include "HeatingElement.h"
#include <string>
#include "Kazan.h"
#include "Puffer.h"
#include <ArduinoJson.h>
#include "MessageBus.h"
#include "Bojler.h"
#include "Radiator.h"
#include "common.h"

class HeatingSystem
{
public:
public:
    std::vector<HeatingElement *> kazan;
    std::vector<HeatingElement *> radiators;
    std::vector<HeatingElement *> puffer;
    std::vector<HeatingElement *> mergedList;

    std::vector<HeatingElement *> heatingPriorityList;

    std::map<std::string, float> *ptrSensorsValue;

    HeatingSystem(const std::string &filename);

    void postInitTasks();

    void addKazan(HeatingElement *element);

    void addRadiator(HeatingElement *element);

    void addPuffer(HeatingElement *element);

    void addHeatingElement(HeatingElement *element, std::string typeString);

    bool validate();

    void printHeatingSystem();

    void update();

    void controlHeatingSystem(HeatingElement *kazan);

    void intiHeatingSystem(const std::string &filename);

private:
    MessageBus messageBus;
};

#endif
