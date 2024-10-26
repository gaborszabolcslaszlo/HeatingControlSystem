// state.h

#ifndef STATE_H
#define STATE_H

struct Configuration {
  String lastLogFileName;
};

Configuration state;

void saveState()
{

    StaticJsonDocument<512> jsonBuffer;
    jsonBuffer["lastLogFileName"] = state.lastLogFileName;

    File file = SPIFFS.open("/" + "state.json", "w");
    if (!file) {
        logMessage("Nem sikerült megnyitni a state.json fájlt írásra.");
        return;
    }

    serializeJson(jsonBuffer, file);

    file.close();
}


#endif
