
#include "HeatingSystem.h"

HeatingSystem::HeatingSystem(const std::string &filename)
{
    this->intiHeatingSystem(filename);
}

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
    pasivElemList.push_back(element);
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
        logMessage("Unknown Heating System Element type: %s \n", typeString.c_str());
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
        logMessage("Error: Kazan element is missing in the configuration.\n");
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
    logMessage("-----Heating System:-----\n");

    logMessage("-----Kazan Elements:------\n");
    for (auto &element : kazan)
    {
        element->printHeatingElement();
    }

    logMessage("-----Radiator Elements:----\n");
    for (auto &element : radiators)
    {
        element->printHeatingElement();
    }

    logMessage("-----Puffer Elements:-------\n");
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
    bool isPasiveElementNeedHeating = false;

    for (HeatingElement *elemDeact : heatingPriorityList)
    {
        elemDeact->deactivate();
    }

    for (HeatingElement *actPuffer : puffer)
    {
        actPuffer->setNeedHeating(false);
    }

    if (kazanPtr->getIsKazanActive())
    {
        for (HeatingElement *elem : pasivElemList)
        {
            if (elem->getNeedHeating())
            {
                isPasiveElementNeedHeating = true;
            }
        }

        if (!isPasiveElementNeedHeating)
        {
            for (HeatingElement *elem : puffer)
            {
                elem->setNeedHeating(false);
                if (kazan->canSupplyHeat(elem))
                {
                    elem->setNeedHeating(true);
                }
            }
        }
    }

    for (HeatingElement *elem : heatingPriorityList)
    {
        if (kazanPtr->getIsKazanActive())
        {
            kazanPtr->activate();
        }
        else
        {
            kazanPtr->deactivate();
        }

        if (kazanPtr->getIsOverHeatProtectionActive())
        {
            elem->activate();
            kazanPtr->activate();
        }
        else
        {
            if (kazanPtr->getIsKazanActive())
            {
                if (kazanPtr->getIsRetourProtectionActive())
                { //?????? ha tobb hazan van akko nam lesz ok
                    elem->deactivatePump();
                }
                else
                {
                    if (kazan->canSupplyHeat(elem) && elem->getNeedHeating())
                    {
                        elem->activate();
                        kazanPtr->activate();
                    }
                    else
                    {
                        continue; // Tovább lép a következő elemre
                    }
                }
            }
            else
            {
                for (HeatingElement *actPuffer : puffer)
                {

                    Puffer *pufferPtr = static_cast<Puffer *>(actPuffer);
                    // skip self check Puffer
                    if (elem->ElemType != HeatingElementType::PUFER)
                    {
                        logMessage("%s", actPuffer->name.c_str());
                        if (pufferPtr->hasStoredEnergy() && elem->getNeedHeating() && pufferPtr->canSupplyHeat(elem))
                        {
                            logMessage("Pufferbol futes activ: %s -> %s\n", pufferPtr->name.c_str(), elem->name.c_str());
                            elem->activate();
                            actPuffer->activate();
                        }
                        /*else if (!pufferPtr->hasStoredEnergy() || !elem->getNeedHeating() || !pufferPtr->canSupplyHeat(elem))
                        {
                            elem->deactivate();
                            logMessage("Deactivate: %s\n", elem->name.c_str());

                            if (!isOneElement)
                            {
                                actPuffer->deactivate();
                                logMessage("Deactivate no one: %s\n", elem->name.c_str());
                            }
                        }*/
                    }
                }
                logMessage("%s\n", elem->name.c_str());
            }
        }
    }
}

void HeatingSystem::intiHeatingSystem(const std::string &filename)
{
    logMessage("----- int Heating System -----\n");
    StaticJsonDocument<3048> doc; // Adjust size as necessary

#ifndef UNIT_TESTING
    File configFile = SPIFFS.open(filename.c_str(), "r");
    if (!configFile)
    {
        logMessage("Failed to open configuration file.\n");
        return;
    }
    DeserializationError error = deserializeJson(doc, configFile);
#else
    // Fájl megnyitása
    std::ifstream configFile(filename);

    // Ellenőrizni, hogy sikerült-e megnyitni a fájlt
    if (!configFile.is_open())
    {
        logMessage("Failed to open configuration file on linux\n");
        return;
    }

    std::string fileContent((std::istreambuf_iterator<char>(configFile)), std::istreambuf_iterator<char>());
    DeserializationError error = deserializeJson(doc, fileContent);
#endif

    if (error)
    {
        logMessage("Failed to read configuration. \n");
        logMessage("%s", error.c_str());
        return;
    }

    JsonObject heatingSystem = doc["HeatingSystem"];

    // Check for mandatory components and validate each section
    for (JsonPair keyValue : heatingSystem)
    {
        const char *key = keyValue.key().c_str(); // Get the key
        JsonArray elements = heatingSystem[key].as<JsonArray>();

        for (JsonObject element : elements)
        {
            std::string name = element["name"];

            if (name.empty())
            {
                logMessage("Error: Name is mandatory for each element.\n");
                continue; // Skip this element if name is missing
            }

            HeatingElement *heatingElement;

            HeatingElementType type = elementTypeFromString(key);
            if (type == HeatingElementType::UNKNOWN)
            {
                logMessage("Unknown Heating System Element type: %s \n", key);
            }

            switch (type)
            {
            case HeatingElementType::KAZAN:
            {
                float retourTempProtValue = element["retourTempProtValue"];
                float tourTempProtValue = element["tourTempProtValue"];
                float activationThreshold = element["activationThreshold"];
                heatingElement = new Kazan(messageBus, name, retourTempProtValue, tourTempProtValue, activationThreshold);
                break;
            }
            case HeatingElementType::RADIATOR:
            {
                heatingElement = new Radiator(messageBus, name);
                break;
            }
            case HeatingElementType::PUFER:
            {
                heatingElement = new Puffer(messageBus, name);
                break;
            }
            default:
                break;
            }

            heatingElement->ElemType = type;

            // Validate and load sensors
            JsonArray sensors = element["sensors"].as<JsonArray>();
            bool isSensorsValid = true;
            for (JsonObject sensor : sensors)
            {
                std::string model = sensor["model"];
                SensorPosition position = stringToPosition(sensor["position"]);
                std::string id = sensor["id"];
                float offset;
                if (sensor.containsKey("offset"))
                {
                    offset = sensor["offset"];
                }
                else
                {
                    offset = 0.0;
                }
                Sensor newSensor(model, position, id, offset);
                if (newSensor.validate())
                {
                    heatingElement->addSensor(newSensor);
                }
                else
                {
                    isSensorsValid = false;
                    logMessage("Error: Invalid sensor configuration.\n");
                }
            }
            if (isSensorsValid)
            {
                heatingElement->classifySensors();
            }

            // Load pumps (name is mandatory)
            JsonArray pumps = element["pumps"].as<JsonArray>();
            for (JsonObject pump : pumps)
            {
                std::string pumpName = pump["name"];
                if (pumpName.empty())
                {
                    logMessage("Error: Name is mandatory for each pump.\n");
                    continue; // Skip this pump if name is missing
                }

                std::string model = pump["model"];
                int maxControlSig = pump["maxControlSig"];
                int minControlSig = pump["minControlSig"];
                std::string workingMode = pump["workingMode"];
                int IOnumber = pump["IOnumber"];

                Pump newPump(IOnumber, pumpName, model, maxControlSig, minControlSig, workingMode);
                heatingElement->addPump(newPump);
            }

            // Load valves (name is mandatory)
            JsonArray valves = element["valves"].as<JsonArray>();
            for (JsonObject valve : valves)
            {
                std::string valveName = valve["name"];
                if (valveName.empty())
                {
                    logMessage("Error: Name is mandatory for each valve.\n");
                    continue; // Skip this valve if name is missing
                }

                int maxControlSig = valve["maxControlSig"];
                int minControlSig = valve["minControlSig"];
                std::string workingMode = valve["workingMode"];

                Valve newValve(valveName, maxControlSig, minControlSig, workingMode);
                heatingElement->addValve(newValve);
            }

            // Add heatingElement to your heating system collection
            // hsystem.addHeatingElement(heatingElement, keyValue.key().c_str());
            switch (type)
            {
            case HeatingElementType::KAZAN:
                this->addKazan(heatingElement);
                break;
            case HeatingElementType::RADIATOR:
                this->addRadiator(heatingElement);
                break;
            case HeatingElementType::PUFER:
                this->addPuffer(heatingElement);
                break;

            default:
                break;
            }

            // heatingSystemCollection.push_back(heatingElement); // Store the heating element in the collection
            // heatingElement->printHeatingElement();
        }

        this->postInitTasks();
    }
}
