#include "Puffer.h"

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
    return getBodyTemperature() > 30; // Van tárolt energia, ha a hőmérséklet magasabb, mint a minimum
}

void Puffer::update()
{
    HeatingElement::update();
    HeatingElement::ElementsStateMap[name]["HT"] = std::to_string(this->heatTransfer.transferValue);
}

bool Puffer::canSupplyHeat(HeatingElement *element)
{
    double bodyTemp = this->getBodyTemperature();        // Kazán előremenő hőmérséklete
    double returnTemp = element->getReTourTemperature(); // Elem visszatérő hőmérséklete

    double minTempDifference = 5.0; // Például, minimum 5 °C különbség szükséges

    // Ha a különbség az előremenő és visszatérő hőmérséklet között elég nagy, akkor még tud hőt biztosítani
    if ((bodyTemp - returnTemp) > minTempDifference)
    {
        return true; // Kazán tud még hőt biztosítani
    }

    return false; // A hőmérsékletkülönbség túl kicsi, a fűtési elem már nem tud több hőt felvenni
}
