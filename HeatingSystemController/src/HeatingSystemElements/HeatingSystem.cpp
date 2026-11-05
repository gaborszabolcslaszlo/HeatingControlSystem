
#include "HeatingSystem.h"
#ifndef UNIT_TESTING
#include <FS.h>
#ifdef ESP32
#include <SPIFFS.h>
#endif
#endif

namespace
{
const std::string kHeatingControllerStateKey{"HeatingController"};
}

HeatingSystem::HeatingSystem(const std::string &filename)
{
    this->intiHeatingSystem(filename);
    // A vezérlő saját állapota akkor is jelenjen meg a /state válaszban,
    // ha az első időzített update() még nem futott le.
    HeatingElement::setStateValue(kHeatingControllerStateKey, "state", controller.getModeName());
}

void HeatingSystem::postInitTasks()
{
}

void HeatingSystem::addKazan(Kazan *element)
{
    kazan.push_back(element);
    mergedList.push_back(element);
}

void HeatingSystem::addRadiator(Radiator *element)
{
    radiators.push_back(element);
    mergedList.push_back(element);
    heatingPriorityList.push_back(element);
    pasivElemList.push_back(element);
}

void HeatingSystem::addPuffer(Puffer *element)
{
    puffer.push_back(element);
    mergedList.push_back(element);
    heatingPriorityList.push_back(element);
}

void HeatingSystem::addBojler(HeatingElement *element)
{
    bojler.push_back(element);
    mergedList.push_back(element);
    heatingPriorityList.push_back(element);
}

void HeatingSystem::addSunCollector(HeatingElement *element)
{
    sunCollector.push_back(element);
    mergedList.push_back(element);
}

bool HeatingSystem::validate()
{
    // Validate Kazan elements
    if (kazan.empty())
    {
        logError("Error: Kazan element is missing in the configuration.\n");
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
    logDebug("-----Heating System:-----\n");

    logDebug("-----Kazan Elements:------\n");
    for (auto &element : kazan)
    {
        element->printHeatingElement();
    }

    logDebug("-----Radiator Elements:----\n");
    for (auto &element : radiators)
    {
        element->printHeatingElement();
    }

    logDebug("-----Puffer Elements:-------\n");
    for (auto &element : puffer)
    {
        element->printHeatingElement();
    }
}

void HeatingSystem::update()
{

    controller.run();
    HeatingElement::setStateValue(kHeatingControllerStateKey, "state", controller.getModeName());

    /* for (HeatingElement *element : mergedList)
     {
         element->update();
     }

     for (HeatingElement *element : kazan)
     {
         Kazan *kazanPtr = static_cast<Kazan *>(element);
         kazanPtr->update();
     }

     for (HeatingElement *element : puffer)
     {
         Puffer *kazanPtr = static_cast<Puffer *>(element);
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
     }*/
}

void HeatingSystem::cleanAllDeviceState()
{
    for (HeatingElement *elemDeact : heatingPriorityList)
    {
        // elemDeact->deactivate();
        elemDeact->cleanState();
    }

    for (HeatingElement *actPuffer : puffer)
    {
        actPuffer->setNeedHeating(false);
        // actPuffer->deactivate();
        actPuffer->cleanState();
    }
}

void HeatingSystem::controlHeatingSystem(HeatingElement *kazan)
{
    Kazan *kazanPtr = static_cast<Kazan *>(kazan);
    bool isPasiveElementNeedHeating = false;

    cleanAllDeviceState();

    // lekezeli azt az esetet ha nincs egyetlen elemre sem futesi igeny benyujtva nh properti az false.
    // akkor aktivalja alapbol a puffernek ha van a rendszerben a nh flaget.
    if (kazanPtr->getIsKazanActive())
    {
        for (HeatingElement *elem : pasivElemList)
        {
            if (elem->getNeedHeating())
            {
                isPasiveElementNeedHeating = true;
            }
        }

        // ha nincsen mashova tenni a hot
        if (!isPasiveElementNeedHeating)
        {
            for (HeatingElement *elem : puffer)
            {
                elem->setNeedHeating(false);
                if (kazan->canSupplyHeat(elem))
                {
                    elem->setNeedHeating(true);
                    elem->activate();
                }
            }
        }
        else
        {
            // parhuzamosan puffer toltes homerseklet folott
            for (HeatingElement *elem : puffer)
            {
                Puffer *pufferElem = static_cast<Puffer *>(elem);
                // elem->setNeedHeating(false);
                if (kazan->canSupplyHeat(elem) && pufferElem->LinkedHeatSource_ActivationTourTemp < kazan->getTourTemperature())
                {
                    elem->setNeedHeating(true);
                    elem->activate(false);
                }
            }
        }
    }

    // kazan allapotok ellenorzese
    if (kazanPtr->getIsKazanActive())
    {
        kazanPtr->activate();
    }
    else
    {
        kazanPtr->deactivate();
    }

    for (HeatingElement *elem : heatingPriorityList)
    {
        if (kazanPtr->getIsOverHeatProtectionActive())
        {
            elem->activate();
            kazanPtr->activate();
        }
        else // Normal mod
        {
            // kihagya a napkolektorokat
            if (elem->ElemType == HeatingElementType::SUNCOLLECTOR)
            {
                continue;
            }

            // kazan futes kezelese
            if (kazanPtr->getIsKazanActive())
            {
                if (kazanPtr->getIsRetourProtectionActive())
                { //?????? ha tobb kazan van akko nam lesz ok
                    // elem->deactivatePump();
                    HeatingFromPuffer_Handler(elem, false);
                }
                else
                {
                    // elemek activalasa ha szukseges
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

            if (!kazanPtr->getIsKazanActive())
            {
                HeatingFromPuffer_Handler(elem, false);
                // logInfo("%s\n", elem->name.c_str());
            }

            for (HeatingElement *sunColElemPtr : sunCollector)
            {
                SunCollector *sunColPtr = static_cast<SunCollector *>(sunColElemPtr);
                // skip self check Puffer
                logInfo("%s", sunColPtr->name.c_str());
                bool isOneElement = false;

                // ha egy elem keri a hot es van a napellemben
                if (sunColPtr->hasStoredEnergy() && elem->getNeedHeating() && sunColPtr->canSupplyHeat(elem))
                {
                    logInfo("Napelembol activ: %s -> %s\n", sunColPtr->name.c_str(), elem->name.c_str());
                    elem->activate();
                    sunColPtr->activate();
                    isOneElement = true;
                }

                // pufferbe iranyitja a hot, az elsobe amely fogani tudja
                if (!isOneElement)
                {
                    for (HeatingElement *sunHeatPuffer : puffer)
                    {
                        Puffer *pufferPtr = static_cast<Puffer *>(sunHeatPuffer);
                        if (sunColPtr->hasStoredEnergy() && sunColPtr->canSupplyHeat(sunHeatPuffer))
                        {
                            elem->activate();
                            sunColPtr->activate();
                            break;
                        }
                    }
                }
            }
        }
    }
}

void HeatingSystem::HeatingFromPuffer_Handler(HeatingElement *elem, bool NeedHeating)
{
    // puferbol valo futes
    for (HeatingElement *actPuffer : puffer)
    {
        // logInfo("Pufferbol futes activ ??");
        Puffer *pufferPtr = static_cast<Puffer *>(actPuffer);
        // skip self check Puffer
        if (elem->ElemType != HeatingElementType::PUFER && elem->ElemType != HeatingElementType::SUNCOLLECTOR)
        {
            // logInfo("%s", actPuffer->name.c_str());
            if (pufferPtr->hasStoredEnergy() && elem->getNeedHeating() && pufferPtr->canSupplyHeat(elem))
            {
                // logInfo("Pufferbol futes activ: %s -> %s\n", pufferPtr->name.c_str(), elem->name.c_str());
                elem->activate();
                if (NeedHeating)
                {
                    actPuffer->activate(false);
                }
                else
                {
                    actPuffer->activate(true);
                }
            }
        }
    }
}

void HeatingSystem::intiHeatingSystem(const std::string &filename)
{
    logDebug("----- int Heating System -----\n");
    JsonDocument doc;

#ifndef UNIT_TESTING
    File configFile = SPIFFS.open(filename.c_str(), "r");
    if (!configFile)
    {
        logError("Failed to open configuration file.\n");
        return;
    }
    DeserializationError error = deserializeJson(doc, configFile);
#else
    // Fájl megnyitása
    std::ifstream configFile(filename);

    // Ellenőrizni, hogy sikerült-e megnyitni a fájlt
    if (!configFile.is_open())
    {
        logError("Failed to open configuration file on linux\n");
        return;
    }

    std::string fileContent((std::istreambuf_iterator<char>(configFile)), std::istreambuf_iterator<char>());
    DeserializationError error = deserializeJson(doc, fileContent);
#endif

    if (error)
    {
        logError("Failed to read configuration. \n");
        logInfo("%s", error.c_str());
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
                logError("Error: Name is mandatory for each element.\n");
                continue; // Skip this element if name is missing
            }

            HeatingElement *heatingElement;

            HeatingElementType type = elementTypeFromString(key);
            if (type == HeatingElementType::UNKNOWN)
            {
                logError("Unknown Heating System Element type: %s \n", key);
            }

            switch (type)
            {
            case HeatingElementType::KAZAN:
            {
                float retourTempProtValue = element["retourTempProtValue"];
                float tourTempProtValue = element["tourTempProtValue"];
                float activationThreshold = element["activationThreshold"];
                heatingElement = new Kazan(messageBus, name, retourTempProtValue, tourTempProtValue, activationThreshold, Kazan::FuelType::WOOD);
                break;
            }
            case HeatingElementType::RADIATOR:
            {
                heatingElement = new Radiator(messageBus, name);
                break;
            }
            case HeatingElementType::PUFER:
            {
                std::string linkedHeatSourceName = element["linkedHeatSourceName"];
                float LinkedHeatSource_ActivationTourTemp = element["LinkedHeatSource_ActivationTourTemp"];

                heatingElement = new Puffer(messageBus, name, linkedHeatSourceName, LinkedHeatSource_ActivationTourTemp);
                break;
            }
            case HeatingElementType::SUNCOLLECTOR:
            {
                heatingElement = new SunCollector(messageBus, name);
                break;
            }
            case HeatingElementType::BOJLER:
            {
                heatingElement = new Bojler(messageBus, name);
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
                int level = sensor["level"] | 0;
                Sensor newSensor(model, position, id, offset, level);
                if (newSensor.validate())
                {
                    Sensor::LocationsById[id] = {name, position, level};
                    heatingElement->addSensor(newSensor);
                }
                else
                {
                    isSensorsValid = false;
                    logError("Error: Invalid sensor configuration.\n");
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
                    logError("Error: Name is mandatory for each pump.\n");
                    continue; // Skip this pump if name is missing
                }

                std::string model = pump["model"];
                int maxControlSig = pump["maxControlSig"];
                int minControlSig = pump["minControlSig"];
                std::string workingMode = pump["workingMode"];
                bool dischargepump = false;
                if (pump.containsKey("dischargepump"))
                {
                    dischargepump = pump["dischargepump"];
                }
                int IOnumber = pump["IOnumber"];
                int SaftyIOnumberForAnalog = -1;
                if (pump.containsKey("SaftyIOnumberForAnalog"))
                {
                    SaftyIOnumberForAnalog = pump["SaftyIOnumberForAnalog"];
                }

                PID pid_c = PID(0.0f, 0.0f, 0.0f);
                if (pump.containsKey("pid"))
                {
                    JsonObject pid = pump["pid"];
                    float p = pid["p"] | 0.0f;
                    float i = pid["i"] | 0.0f;
                    float d = pid["d"] | 0.0f;
                    std::string tempSensorId = pid["tempSensorId"];
                    if (tempSensorId.empty())
                    {
                        logError("Error: tempSensorId is mandatory for PID controller.\n");
                        continue; // Skip this pump if name is missing
                    }
                    pid_c = PID(p, i, d);
                    pid_c.enabled = pid["enabled"] | true;
                    pid_c.tempSensorId = tempSensorId;
                    pid_c.outMin = 5;
                    pid_c.outMax = 99.9;
                    pid_c.setPointConfig = pid["setPoint"];
                    pid_c.allparamok = true;
                }
                Pump newPump(IOnumber, pumpName, model, maxControlSig, minControlSig, workingMode, SaftyIOnumberForAnalog, dischargepump, pid_c);
                heatingElement->addPump(newPump);
            }

            // Load valves (name is mandatory)
            JsonArray valves = element["valves"].as<JsonArray>();
            for (JsonObject valve : valves)
            {
                std::string valveName = valve["name"];
                if (valveName.empty())
                {
                    logError("Error: Name is mandatory for each valve.\n");
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
                this->addKazan((Kazan *)heatingElement);
                this->controller.addBoiler(*static_cast<Kazan *>(heatingElement));
                break;
            case HeatingElementType::RADIATOR:
                this->addRadiator((Radiator *)heatingElement);
                this->controller.addConsumer(*static_cast<Radiator *>(heatingElement));
                break;
            case HeatingElementType::PUFER:
                this->addPuffer((Puffer *)heatingElement);
                this->controller.addBuffer(*static_cast<Puffer *>(heatingElement));
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
