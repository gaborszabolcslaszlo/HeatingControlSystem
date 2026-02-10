#include "Puffer.h"
#include "Kazan.h"

Puffer::Puffer(MessageBus &bus, std::string name, std::string linkedHeatSourceName, float LinkedHeatSource_ActivationTourTemp) : HeatingElement(bus, name)
{
    this->linkedHeatSourceName = linkedHeatSourceName;
    this->LinkedHeatSource_ActivationTourTemp = LinkedHeatSource_ActivationTourTemp;
}

// Puffer egyedi vezérlése
void Puffer::moveEnergyToRadiators(float radiatorRequestTemp)
{
}

bool Puffer::hasStoredEnergy()
{
    return getBodyTemperatureFromLevel(100) > 45; // Van tárolt energia, ha a hőmérséklet magasabb, mint a minimum
}

Kazan *Puffer::getActiveKazan()
{
    auto activeAndHeating = filterHeatingElements(
        [](HeatingElement *e)
        {
            return e->isActive;
            ;
        });

    return activeAndHeating.empty() ? nullptr : static_cast<Kazan *>(activeAndHeating.front());
}

void Puffer::update()
{
    HeatingElement::update();

    Kazan *kazan = Puffer::getActiveKazan();
    bool isElementNeedHeating = !getAllNeedHeatingElements().empty();

    // puffer help kazan
    if (kazan->getIsRetourProtectionActive() && isElementNeedHeating)
    {
    }

    for (Pump &pump : DischargePumps)
    {
        pump.update(); // Hívjuk meg a függvényt minden szenzorra
    }

    HeatingElement::ElementsStateMap[name]["HT"] = std::to_string(this->heatTransfer.transferValue);
    HeatingElement::ElementsStateMap[name]["iCHA"] = std::to_string(this->isChargeActivated);
    HeatingElement::ElementsStateMap[name]["idCHA"] = std::to_string(this->isDeschargeActivated);
}

bool Puffer::canSupplyHeat(HeatingElement *element)
{
    double bodyTemp = this->getBodyTemperatureFromLevel(100);
    double returnTemp = element->getReTourTemperature(); // Elem visszatérő hőmérséklete

    double minTempDifference = 5.0; // Például, minimum 5 °C különbség szükséges

    // Ha a különbség az előremenő és visszatérő hőmérséklet között elég nagy, akkor még tud hőt biztosítani
    if ((bodyTemp - returnTemp) > minTempDifference)
    {
        return true; // Kazán tud még hőt biztosítani
    }

    return false; // A hőmérsékletkülönbség túl kicsi, a fűtési elem már nem tud több hőt felvenni
}

void Puffer::chargeFromSource(HeatingElement *element)
{
    double bodyTemp = this->getBodyTemperatureFromLevel(0);
    double tourTemp = element->getTourTemperature();

    if (tourTemp >= this->LinkedHeatSource_ActivationTourTemp)
    {
        this->isChargeActivated = true;
        this->isDeschargeActivated = false;
        this->setNeedHeating(true);
        this->activate();
    }
    else
    {
        this->isChargeActivated = false;
        this->isDeschargeActivated = false;
        this->setNeedHeating(false);
    }
}

void Puffer::discharge(bool active)
{
    if (active)
    {
        this->isChargeActivated = false;
        this->isDeschargeActivated = true;
        this->setPumpControlSinal(-100);
        this->setNeedHeating(false);
    }
    else
    {
        this->isDeschargeActivated = false;
    }
}

float Puffer::getBodyTemperatureFromLevel(int level)
{
    Sensor *sensorTop = NULL;
    for (Sensor sensor : sensors)
    {
        if (sensor.levelMark == 100)
        {
            return sensor.getTemperature();
        }
    }

    if (sensors.empty())
    {
        return 0.0;
    }
    else
    {
        return sensors.front().getTemperature(); // error return  highet temp value.
    }
}