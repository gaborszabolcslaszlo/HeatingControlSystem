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
#include "suncollector.h"
#include "common.h"
#include <fstream> // Fájlok olvasásához és írásához
#include <iostream>
#include "../comon.h"
#include "Controllers/pid.h"
#include "HeatingController.h"

class HeatingSystem
{
public:
public:
    std::vector<Kazan *> kazan;
    std::vector<Radiator *> radiators;
    std::vector<Puffer *> puffer;
    std::vector<HeatingElement *> bojler;
    std::vector<HeatingElement *> sunCollector;
    std::vector<HeatingElement *> mergedList;

    std::vector<HeatingElement *> pasivElemList;
    std::vector<HeatingElement *> heatingPriorityList;

    std::map<std::string, float> *ptrSensorsValue;

    HeatingSystem(const std::string &filename);

    void postInitTasks();

    void addKazan(Kazan *element);

    void addRadiator(Radiator *element);

    void addPuffer(Puffer *element);

    void addBojler(HeatingElement *element);

    void addSunCollector(HeatingElement *element);

    void addHeatingElement(HeatingElement *element, std::string typeString);

    bool validate();

    void printHeatingSystem();

    void update();

    bool hasControllerFault() const { return controller.getState() == HeatingController::FAULT; }
    bool requestBoilerBufferOperation(const std::string &name, bool cooling, std::string &error)
    {
        return controller.requestBoilerBufferOperation(name, cooling, error);
    }
    bool cancelBoilerBufferOperation(const std::string &name, std::string &error)
    {
        return controller.cancelBoilerBufferOperation(name, error);
    }
    bool isBoilerBufferOperationActive() const { return controller.isBoilerBufferOperationActive(); }

    void intiHeatingSystem(const std::string &filename);

private:
    MessageBus messageBus;

    HeatingController controller;

    void cleanAllDeviceState();

    bool isSystemFullOk;
};

#endif
