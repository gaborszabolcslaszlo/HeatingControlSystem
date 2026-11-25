#pragma once

#include <ArduinoJson.h>
#include <cmath>
#include <map>
#include <set>
#include <string>

// A fájlbetöltés és a konfigurációs API ugyanazokat a szabályokat használja.
inline bool validateBoilerBufferConfiguration(JsonObject system, std::string &error)
{
    auto fail = [&error](const char *message) { error = message; return false; };
    std::map<int, unsigned> pumpIds;
    for (JsonPair group : system)
        for (JsonObject element : group.value().as<JsonArray>())
            for (JsonObject pump : element["pumps"].as<JsonArray>())
                ++pumpIds[pump["IOnumber"].as<int>()];

    for (JsonObject boiler : system["KAZAN"].as<JsonArray>())
    {
        for (const char *key : {"preheatFromPuffer", "coolingFromPuffer"})
        {
            if (!boiler.containsKey(key))
                continue;
            if (!boiler[key].is<JsonObject>())
                return fail("Boiler buffer operation must be an object");
            JsonObject config = boiler[key].as<JsonObject>();
            if (config.containsKey("enabled") && !config["enabled"].is<bool>())
                return fail("Boiler buffer operation enabled must be boolean");
            if (config.containsKey("maxDurationSeconds") &&
                (!config["maxDurationSeconds"].is<unsigned long>() ||
                 config["maxDurationSeconds"].as<unsigned long>() < 1 ||
                 config["maxDurationSeconds"].as<unsigned long>() > 3600))
                return fail("maxDurationSeconds must be an integer between 1 and 3600");
            for (const char *field : {"minDeltaTemp", "stopDeltaTemp", "targetTemp"})
                if (config.containsKey(field) &&
                    (!config[field].is<float>() || !std::isfinite(config[field].as<float>())))
                    return fail("Boiler operation temperatures must be finite numbers");
            const float minDelta = config["minDeltaTemp"] | 2.0f;
            const float stopDelta = config["stopDeltaTemp"] | 1.0f;
            if (stopDelta < 0 || minDelta <= stopDelta)
                return fail("minDeltaTemp must exceed nonnegative stopDeltaTemp");
            for (const char *field : {"pumpSignal", "bufferPumpSignal"})
                if (config.containsKey(field) &&
                    (!config[field].is<int>() || config[field].as<int>() < 1 ||
                     config[field].as<int>() > 100))
                    return fail("pumpSignal and bufferPumpSignal must be integers between 1 and 100");
            if (config.containsKey("stopPumpIds") && !config["stopPumpIds"].is<JsonArray>())
                return fail("stopPumpIds must be an array of pump IO numbers");
            std::set<int> stopIds;
            for (JsonVariant id : config["stopPumpIds"].as<JsonArray>())
            {
                if (!id.is<int>() || id.as<int>() < 0 || id.as<int>() > 15 ||
                    pumpIds[id.as<int>()] != 1 || !stopIds.insert(id.as<int>()).second)
                    return fail("stopPumpIds contains an unknown, ambiguous or duplicate pump IO");
            }
            if (!(config["enabled"] | false))
                continue;
            const char *bufferName = config["bufferName"] | "";
            JsonObject buffer;
            unsigned matches = 0;
            for (JsonObject candidate : system["PUFER"].as<JsonArray>())
                if (std::string(candidate["name"] | "") == bufferName)
                {
                    buffer = candidate;
                    ++matches;
                }
            if (bufferName[0] == '\0' || matches != 1)
                return fail("Enabled boiler operation needs a unique bufferName");
            if (std::string(key) == "preheatFromPuffer")
            {
                const float target = config["targetTemp"] | (boiler["retourTempProtValue"] | 38.0f);
                if (target <= 0 || target >= (boiler["tourTempProtValue"] | 80.0f))
                    return fail("Preheat target must be below the boiler overheat limit");
            }
            const int defaultSignal = config["pumpSignal"] | 100;
            for (JsonObject element : {boiler, buffer})
            {
                unsigned circulationPumps = 0;
                bool hasTour = false, hasRetour = false, hasBody = false;
                for (JsonObject sensor : element["sensors"].as<JsonArray>())
                {
                    const std::string position = sensor["position"] | "";
                    hasTour |= position == "tour";
                    hasRetour |= position == "retour";
                    hasBody |= position == "body";
                }
                if (!hasTour || !hasRetour || (element == boiler && !hasBody))
                    return fail("Boiler operation requires supply/return sensors and boiler body sensor");
                for (JsonObject pump : element["pumps"].as<JsonArray>())
                {
                    if (pump["dischargepump"] | false)
                        continue;
                    const int signal = element == buffer && std::string(pump["workingMode"] | "") == "PWM"
                                           ? (config["bufferPumpSignal"] | defaultSignal) : defaultSignal;
                    const int id = pump["IOnumber"] | -1;
                    if (stopIds.count(id) || pumpIds[id] != 1 ||
                        signal > (pump["maxControlSig"] | 100) || signal < (pump["minControlSig"] | 0))
                        return fail("Circulation pump conflicts with stopPumpIds or pumpSignal");
                    ++circulationPumps;
                }
                if (circulationPumps == 0)
                    return fail("Boiler and buffer each need a charge/circulation pump");
            }
        }
    }
    return true;
}
