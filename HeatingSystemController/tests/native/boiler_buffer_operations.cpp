#ifndef UNIT_TESTING
#define UNIT_TESTING
#endif
#ifndef logInfo
#define logInfo(...) ((void)0)
#endif
#ifndef logDebug
#define logDebug(...) ((void)0)
#endif
#ifndef logWarn
#define logWarn(...) ((void)0)
#endif
#ifndef logError
#define logError(...) ((void)0)
#endif

#include "HeatingSystemElements/HeatingController.h"
#include "HeatingSystemElements/boiler_buffer_config.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

static unsigned long clockMs = 0;
unsigned long millis() { return clockMs; }
std::map<std::string, float> Sensor::SensorsValue;
std::map<int, int> outputs;
void writeIO(int id, int value) { outputs[id] = value; }
void writePwmIO(int id, int value) { outputs[id] = value; }
void logMessage(const char *, ...) {}

struct Fixture
{
    MessageBus bus;
    Kazan boiler{bus, "Boiler", 38, 80, 34, Kazan::FuelType::WOOD};
    Puffer buffer{bus, "Buffer", "Boiler", 60};
    Radiator consumer{bus, "Consumer"};
    HeatingController controller;
    Fixture()
    {
        clockMs = 0;
        Sensor::SensorsValue.clear();
        outputs.clear();
        boiler.ElemType = HeatingElementType::KAZAN;
        buffer.ElemType = HeatingElementType::PUFER;
        consumer.ElemType = HeatingElementType::RADIATOR;
        for (HeatingElement *element : {static_cast<HeatingElement *>(&boiler), static_cast<HeatingElement *>(&buffer)})
        {
            element->addSensor(Sensor("mock", SensorPosition::TOUR, element->name + "-tour", 0));
            element->addSensor(Sensor("mock", SensorPosition::RETOUR, element->name + "-retour", 0));
            element->addSensor(Sensor("mock", SensorPosition::BODY, element->name + "-body", 0));
            element->classifySensors();
        }
        PID pid(0,0,0); pid.enabled=false;
        boiler.addPump(Pump(7,"boiler","mock",100,0,"ON/OFF",0,false,pid));
        // Disabled PID still has to circulate during the manual operation.
        buffer.addPump(Pump(9,"charge","mock",100,0,"PWM",0,false,pid));
        buffer.addPump(Pump(8,"discharge","mock",100,0,"PWM",0,true,pid));
        consumer.addPump(Pump(6,"consumer","mock",100,0,"ON/OFF",0,false,pid));
        for (auto *config : {&boiler.preheatFromPuffer, &boiler.coolingFromPuffer})
        {
            config->enabled=true;
            config->bufferName="Buffer";
            config->maxDurationMs=300000;
            config->stopPumpIds={6,8};
        }
        controller.addBoiler(boiler); controller.addBuffer(buffer); controller.addConsumer(consumer);
        controller.setElements({&boiler,&buffer,&consumer});
        temperatures(30,20,25,60,15);
    }
    void temperatures(float tour, float retour, float body, float bufferTour, float bufferRetour)
    {
        Sensor::SensorsValue["Boiler-tour"]=tour;
        Sensor::SensorsValue["Boiler-retour"]=retour;
        Sensor::SensorsValue["Boiler-body"]=body;
        Sensor::SensorsValue["Buffer-tour"]=bufferTour;
        Sensor::SensorsValue["Buffer-retour"]=bufferRetour;
        Sensor::SensorsValue["Buffer-body"]=(bufferTour+bufferRetour)/2;
    }
    void start(bool cooling)
    {
        std::string error;
        assert(controller.requestBoilerBufferOperation("Boiler",cooling,error));
        controller.run();
        assert(controller.getState() == (cooling ? HeatingController::COOLING_BOILER : HeatingController::PREHEATING_BOILER));
        assert(boiler.getState() == (cooling ? Kazan::COOLING_FROM_PUFFER : Kazan::PREHEAT_FROM_PUFFER));
        assert(outputs[7]==100 && outputs[9]==100 && outputs[6]==0 && outputs[8]==0);
        assert(buffer.getState()==Puffer::CHARGE);
    }
};

// Közvetlenül a kazán állapotgépét futtatjuk, hogy a vezérlő előzetes
// túlmelegedés-ellenőrzése ne fedhesse el az átmenetek prioritásának hibáját.
void test_preheat_overheat_takes_priority_over_timeout_and_target()
{
    Fixture f;
    f.start(false);
    clockMs = f.boiler.preheatFromPuffer.maxDurationMs;
    f.temperatures(85, 38, 85, 90, 15);
    f.boiler.run();
    assert(f.boiler.getState() == Kazan::OVERHEATED);
    assert(outputs[7] > 0);
    f.controller.run();
    assert(!f.controller.isBoilerBufferOperationActive());
    assert(f.boiler.getState() == Kazan::OVERHEATED);
}

void test_cooling_overheat_takes_priority_over_timeout_and_equal_temperatures()
{
    Fixture f;
    f.start(true);
    clockMs = f.boiler.coolingFromPuffer.maxDurationMs;
    // Az előremenő és a visszatérő már kiegyenlítődött, de a kazántest túlmeleg.
    f.temperatures(30, 30, 85, 60, 15);
    f.boiler.run();
    assert(f.boiler.getState() == Kazan::OVERHEATED);
    assert(outputs[7] > 0);
    f.controller.run();
    assert(!f.controller.isBoilerBufferOperationActive());
    assert(f.boiler.getState() == Kazan::OVERHEATED);
}

void test_configured_timeout_boundary(bool cooling)
{
    Fixture f;
    auto &config = cooling ? f.boiler.coolingFromPuffer : f.boiler.preheatFromPuffer;
    config.maxDurationMs = 2000;
    // Az időzítés az indításhoz képest történjen, ne a rendszer indulásához.
    clockMs = 10000;
    f.start(cooling);
    clockMs += config.maxDurationMs - 1;
    f.controller.run();
    assert(f.controller.isBoilerBufferOperationActive());
    assert(outputs[7] == 100 && outputs[9] == 100);
    ++clockMs;
    f.controller.run();
    assert(f.boiler.getState() == Kazan::COOLD);
    assert(!f.controller.isBoilerBufferOperationActive());
    assert(outputs[7] == 0 && outputs[9] == 0);
}

void test_preheat_cancel_restores_pump_control()
{
    Fixture f;
    f.start(false);
    std::string error;
    assert(f.controller.cancelBoilerBufferOperation("Boiler", error));
    f.controller.run();
    assert(f.boiler.getState() == Kazan::COOLD);
    assert(!f.controller.isBoilerBufferOperationActive());
    assert(outputs[7] == 0 && outputs[9] == 0);
    f.consumer.pumps[0].enable();
    f.consumer.pumps[0].update();
    assert(outputs[6] == 100);
}

int main()
{
    for (bool cooling : {false, true})
    {
        Fixture f;
        auto &config = cooling ? f.boiler.coolingFromPuffer : f.boiler.preheatFromPuffer;
        config.bufferPumpSignal = 45;
        PID pid(0,0,0);
        f.buffer.addPump(Pump(4,"charge relay","mock",100,0,"ON/OFF",0,false,pid));
        std::string error;
        assert(f.controller.requestBoilerBufferOperation("Boiler", cooling, error));
        f.controller.run();
        assert(outputs[9] == 45);
        assert(outputs[7] == 100 && outputs[4] == 100);
        assert(outputs[6] == 0 && outputs[8] == 0);
        assert(f.controller.cancelBoilerBufferOperation("Boiler", error));
        f.controller.run();
        assert(outputs[9] == 0 && outputs[4] == 0);
    }
    puts("PASS: separate buffer PWM for preheat and cooling");
    test_preheat_overheat_takes_priority_over_timeout_and_target();
    puts("PASS: preheat overheat priority");
    test_cooling_overheat_takes_priority_over_timeout_and_equal_temperatures();
    puts("PASS: cooling overheat priority");
    test_configured_timeout_boundary(false);
    puts("PASS: preheat configured timeout boundary");
    test_configured_timeout_boundary(true);
    puts("PASS: cooling configured timeout boundary");
    test_preheat_cancel_restores_pump_control();
    puts("PASS: preheat cancellation restores pump control");
    { Fixture f; f.start(true);
      f.temperatures(21,20,25,60,15); f.controller.run();
      assert(f.boiler.getState()==Kazan::COOLD);
      assert(!f.controller.isBoilerBufferOperationActive());
      assert(outputs[7]==0 && outputs[9]==0);
    }
    { Fixture f; f.start(true); clockMs+=300000; f.controller.run();
      assert(f.boiler.getState()==Kazan::COOLD);
      assert(!f.controller.isBoilerBufferOperationActive());
      assert(outputs[7]==0 && outputs[9]==0);
    }
    { Fixture f; f.start(false); f.temperatures(40,38,35,40,15); f.controller.run();
      assert(f.boiler.getState()==Kazan::RETOUR_PROTECTION);
      assert(!f.controller.isBoilerBufferOperationActive());
    }
    { Fixture f; f.start(false); clockMs+=300000; f.controller.run();
      assert(f.boiler.getState()==Kazan::COOLD);
      assert(!f.controller.isBoilerBufferOperationActive());
    }
    { Fixture f; f.start(true); std::string error;
      assert(!f.controller.requestBoilerBufferOperation("Boiler",true,error));
      assert(f.controller.cancelBoilerBufferOperation("Boiler",error));
      f.controller.run(); assert(outputs[7]==0 && outputs[9]==0);
      assert(!f.controller.isBoilerBufferOperationActive());
      f.consumer.pumps[0].enable(); f.consumer.pumps[0].update(); assert(outputs[6]==100);
    }
    { Fixture f; std::string error;
      f.temperatures(40,20,25,60,15);
      assert(!f.controller.requestBoilerBufferOperation("Boiler",true,error));
      f.temperatures(30,20,25,60,31);
      assert(!f.controller.requestBoilerBufferOperation("Boiler",true,error));
      f.temperatures(30,20,25,28,15);
      assert(!f.controller.requestBoilerBufferOperation("Boiler",false,error));
      f.temperatures(30,20,25,60,15); f.boiler.coolingFromPuffer.stopPumpIds={7};
      assert(!f.controller.requestBoilerBufferOperation("Boiler",true,error));
      assert(f.boiler.getState()==Kazan::COOLD);
      f.boiler.coolingFromPuffer.stopPumpIds={15};
      assert(!f.controller.requestBoilerBufferOperation("Boiler",true,error));
    }
    { Fixture f; f.start(true); Sensor::SensorsValue.erase("Boiler-tour"); f.controller.run();
      assert(!f.controller.isBoilerBufferOperationActive()); assert(outputs[9]==0);
    }
    { Fixture f; f.start(true); f.temperatures(85,20,85,60,15); f.controller.run();
      assert(f.boiler.getState()==Kazan::OVERHEATED);
      assert(!f.controller.isBoilerBufferOperationActive());
    }
    { Fixture f; f.start(true); f.temperatures(30,20,85,60,15); f.controller.run();
      assert(f.boiler.getState()==Kazan::OVERHEATED);
      assert(!f.controller.isBoilerBufferOperationActive());
    }
    { Fixture f; f.consumer.setNeedHeating(true);
      for (int i=0;i<5;++i) f.controller.run();
      assert(outputs[6]==100);
      f.start(true); assert(outputs[6]==0);
      f.temperatures(21,20,25,60,15); f.controller.run();
      for (int i=0;i<3;++i) f.controller.run();
      assert(f.consumer.getNeedHeating()); assert(outputs[6]==100);
    }
    { Fixture f; std::string error;
      f.boiler.coolingFromPuffer.bufferName="missing";
      assert(!f.controller.requestBoilerBufferOperation("Boiler",true,error));
      f.boiler.coolingFromPuffer.bufferName="Buffer";
      f.boiler.coolingFromPuffer.enabled=false;
      assert(!f.controller.requestBoilerBufferOperation("Boiler",true,error));
    }
    { Fixture f; std::string error; Sensor::SensorsValue["Boiler-tour"]=std::numeric_limits<float>::quiet_NaN();
      assert(!f.controller.requestBoilerBufferOperation("Boiler",true,error));
    }
    { Fixture f; f.start(false); f.temperatures(30,20,25,29,15); f.controller.run();
      assert(!f.controller.isBoilerBufferOperationActive());
    }
    { Fixture f; clockMs=~0UL-1000UL; f.start(true); clockMs+=300000UL; f.controller.run();
      assert(!f.controller.isBoilerBufferOperationActive());
    }
    {
        JsonDocument doc;
        assert(!deserializeJson(doc, R"json({"HeatingSystem": {
          "KAZAN": [{"name":"Boiler", "retourTempProtValue":38, "tourTempProtValue":80,
            "sensors":[{"position":"tour"},{"position":"retour"},{"position":"body"}],
            "pumps":[{"IOnumber":7,"minControlSig":0,"maxControlSig":100}],
            "coolingFromPuffer":{"enabled":true,"bufferName":"Buffer",
              "maxDurationSeconds":300,"stopPumpIds":[6,8]}}],
          "PUFER": [{"name":"Buffer",
            "sensors":[{"position":"tour"},{"position":"retour"}],
            "pumps":[{"IOnumber":9,"minControlSig":0,"maxControlSig":100},
                     {"IOnumber":8,"dischargepump":true}]}],
          "RADIATOR": [{"name":"Consumer","pumps":[{"IOnumber":6}]}]
        }})json"));
        std::string error;
        assert(validateBoilerBufferConfiguration(doc["HeatingSystem"],error));
        auto section=doc["HeatingSystem"]["KAZAN"][0]["coolingFromPuffer"].as<JsonObject>();
        section["maxDurationSeconds"]=-1;
        assert(!validateBoilerBufferConfiguration(doc["HeatingSystem"],error));
        section["maxDurationSeconds"]=300; section["bufferName"]="missing";
        assert(!validateBoilerBufferConfiguration(doc["HeatingSystem"],error));
        section["bufferName"]="Buffer"; section["stopPumpIds"][0]=7;
        assert(!validateBoilerBufferConfiguration(doc["HeatingSystem"],error));
        auto boiler=doc["HeatingSystem"]["KAZAN"][0].as<JsonObject>();
        boiler.remove("preheatFromPuffer"); boiler.remove("coolingFromPuffer");
        assert(validateBoilerBufferConfiguration(doc["HeatingSystem"],error));
    }
    puts("Boiler buffer operation tests passed");
}
