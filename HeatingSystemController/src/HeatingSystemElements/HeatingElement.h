#ifndef HEATINGELEMENT_H
#define HEATINGELEMENT_H

#include "pump.h"
#include "valve.h"
#include "HeatingTransfer.h"
#include "MessageBus.h"
#include "Sensor.h"
#include <string>
#include "common.h"
#include "../comon.h"

enum class HeatingElementType
{
    KAZAN,
    RADIATOR,
    BOJLER,
    PUFER, // Used for error handling
    UNKNOWN
};

std::string elementTypeToString(HeatingElementType type);

HeatingElementType elementTypeFromString(const std::string &str);

// HeatingElement class (for Kazan, Radiators, Puffer)
class HeatingElement
{
public:
    std::string name;
    bool isActive; // Állapotjelző, hogy az elem éppen aktív-e
    std::vector<Sensor> sensors;
    std::vector<Pump> pumps;
    std::vector<Valve> valves;

    std::vector<Sensor *> tourSensors;
    std::vector<Sensor *> retourSensors;
    std::vector<Sensor *> bodySensors;

    HeatingTransfer heatTransfer;

    HeatingElement(MessageBus &bus, const std::string name);

    void activatePump();

    void deactivatePump();

    void activate();

    void deactivate();

    bool getIsActive() const;

    void addSensor(Sensor sensor);

    void addPump(Pump pump);

    void addValve(Valve valve);

    float getTourTemperature();

    float getReTourTemperature();

    float getBodyTemperature();

    void classifySensors();

    float calculateAverageTemperature(const std::vector<Sensor *> &sensors);

    // Function to print sensors using Serial
    void printSensors(const std::vector<Sensor *> &sensors, const std::string &group);

    bool validate();

    void printHeatingElement();

    void update();

    virtual bool canSupplyHeat(HeatingElement *element);

    bool getNeedHeating() const;
    void setNeedHeating(bool needHeating_);
    static std::map<std::string, std::map<std::string, std::string>> ElementsStateMap;

protected:
    bool needHeating;
    MessageBus &messageBus;
    virtual void onMessageReceived(const std::string &message);
};

#endif