
#include "HeatingSystem.h"

HeatingSystem::HeatingSystem() {}

void HeatingSystem::postInitTasks()
{
}

void HeatingSystem::addKazan(HeatingElement *element)
{
    kazan.push_back(element);
    mergedList.push_back(element);
}

void HeatingSystem::addRadiator(HeatingElement *element)
{
    radiators.push_back(element);
    mergedList.push_back(element);
    heatingPriorityList.push_back(element);
}

void HeatingSystem::addPuffer(HeatingElement *element)
{
    puffer.push_back(element);
    mergedList.push_back(element);
    heatingPriorityList.push_back(element);
}

void HeatingSystem::addHeatingElement(HeatingElement *element, std::string typeString)
{
    HeatingElementType type = elementTypeFromString(typeString);
    if (type == HeatingElementType::UNKNOWN)
    {
        Serial.printf("Unknown Heating System Element type: %s \n", typeString.c_str());
    }
    switch (type)
    {
    case HeatingElementType::KAZAN:
        addKazan(element);
        break;
    case HeatingElementType::RADIATOR:
        addRadiator(element);
        break;
    case HeatingElementType::PUFER:
        addPuffer(element);
        break;

    default:
        break;
    }
}

bool HeatingSystem::validate()
{
    // Validate Kazan elements
    if (kazan.empty())
    {
        Serial.println("Error: Kazan element is missing in the configuration.");
        return false;
    }

    for (auto &element : mergedList)
    {
        if (!element->validate())
            return false;
    }

    return true;
}

void HeatingSystem::printHeatingSystem()
{
    Serial.println("Heating System:");

    Serial.println("  Kazan Elements:");
    for (auto &element : kazan)
    {
        element->printHeatingElement();
    }

    Serial.println("  Radiator Elements:");
    for (auto &element : radiators)
    {
        element->printHeatingElement();
    }

    Serial.println("  Puffer Elements:");
    for (auto &element : puffer)
    {
        element->printHeatingElement();
    }
}

void HeatingSystem::update()
{

    for (HeatingElement *element : mergedList)
    {
        element->update();
    }

    for (HeatingElement *element : kazan)
    {
        Kazan *kazanPtr = static_cast<Kazan *>(element);
        kazanPtr->update();
    }

    controlHeatingSystem(kazan[0]);

    for (HeatingElement *element : mergedList)
    {
        element->update();
    }

    for (HeatingElement *element : kazan)
    {
        Kazan *kazanPtr = static_cast<Kazan *>(element);
        kazanPtr->update();
    }
}

void HeatingSystem::controlHeatingSystem(HeatingElement *kazan)
{
    Kazan *kazanPtr = static_cast<Kazan *>(kazan);
    for (HeatingElement *elem : heatingPriorityList)
    {
        if (kazanPtr->getIsOverHeatProtectionActive())
        {
            elem->activatePump();
            kazanPtr->activatePump();
        }
        else
        {
            if (kazanPtr->getIsActive())
            {
                if (kazanPtr->getIsRetourProtectionActive())
                {
                    elem->deactivatePump();
                }
                else
                {
                    if (kazan->canSupplyHeat(elem) && elem->getNeedHeating())
                    {
                        elem->activate();
                    }
                    else
                    {
                        continue; // Tovább lép a következő elemre
                    }
                }
            }
            else
            {
                bool isOneElement = false;
                for (HeatingElement *actPuffer : puffer)
                {
                    Puffer *pufferPtr = static_cast<Puffer *>(actPuffer);
                    if (pufferPtr->hasStoredEnergy() && elem->getNeedHeating() && pufferPtr->canSupplyHeat(elem))
                    {
                        Serial.printf("Pufferbol futes activ: %s\n", elem->name);
                        elem->activate();
                        pufferPtr->activate();
                        isOneElement = true;
                    }
                    else if (!pufferPtr->hasStoredEnergy() || !elem->getNeedHeating() || !pufferPtr->canSupplyHeat(elem))
                    {
                        elem->deactivate();
                        pufferPtr->deactivate();
                    }
                }
            }
        }
    }
}
