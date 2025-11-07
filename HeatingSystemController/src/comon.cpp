#include "comon.h"

#ifndef UNIT_TESTING
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 600000); // 1 órás időeltolás (3600 másodperc), 600 másodpercenként frissít

// Log fájl neve
const char *logFileName = "/log.txt";
const int maxLogFiles = 5; // maximális log fájlok száma

// Tartalom típus meghatározása fájlnév alapján
String getContentType(String filename)
{
    if (filename.endsWith(".htm"))
        return "text/html";
    else if (filename.endsWith(".html"))
        return "text/html";
    else if (filename.endsWith(".css"))
        return "text/css";
    else if (filename.endsWith(".js"))
        return "application/javascript";
    else if (filename.endsWith(".png"))
        return "image/png";
    else if (filename.endsWith(".gif"))
        return "image/gif";
    else if (filename.endsWith(".jpg"))
        return "image/jpeg";
    else if (filename.endsWith(".ico"))
        return "image/x-icon";
    else if (filename.endsWith(".xml"))
        return "text/xml";
    else if (filename.endsWith(".pdf"))
        return "application/pdf";
    else if (filename.endsWith(".zip"))
        return "application/zip";
    return "text/plain"; // Alapértelmezett típus
}

void createLogFile()
{
    File logFile = SPIFFS.open(logFileName, "w");
}

const size_t maxLogFileSize = 1024; // Max méret bájtban (pl. 1 kB)

String getFormattedDate()
{
    // timeClient.getEpochTime() adja az aktuális időt másodpercben
    unsigned long epochTime = timeClient.getEpochTime();

    // Átalakítás éve, hónap, nap formátumba
    int year = 1970 + epochTime / 31556926;                   // kb
    int month = (epochTime % 31556926) / 2629743 + 1;         // hónap 1-12
    int day = ((epochTime % 31556926) % 2629743) / 86400 + 1; // nap 1-31

    char dateStr[11];
    sprintf(dateStr, "%04d-%02d-%02d", year, month, day);
    return String(dateStr);
}

// Segédfüggvény: aktuális dátummal ellátott log fájl neve
String getLogFileName()
{
    // Példa formátum: /log_2025-10-26.txt
    String dateStr = getFormattedDate(); // pl. "2025-10-26"
    return String("/log_") + dateStr + ".txt";
}

void enforceMaxLogFiles()
{
    Dir dir = SPIFFS.openDir("/log"); // ESP8266: könyvtár listázása
    std::vector<String> logFiles;

    // Összegyűjtjük a fájlneveket
    while (dir.next())
    {
        logFiles.push_back(dir.fileName());
    }

    // Ha több fájl van, mint a max, töröljük a legrégebbit
    while (logFiles.size() >= maxLogFiles)
    {
        // A név szerinti rendezés → a legkisebb név = legrégebbi fájl
        std::sort(logFiles.begin(), logFiles.end());
        SPIFFS.remove(logFiles[0]);
        logFiles.erase(logFiles.begin());
    }
}

void logMessage(const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    String timestamp = timeClient.getFormattedTime(); // pl. "12:34:56"
    String logEntry = String("[") + timestamp + String("] ") + String(buffer);

    // 1. Kiíratás Serial-ra
    Serial.print(logEntry);

    // 2. Kiíratás fájlba (SPIFFS)
    String currentLogFile = getLogFileName();
    File logFile = SPIFFS.open(currentLogFile, "a");
    if (logFile)
    {
        if (logFile.size() + logEntry.length() > maxLogFileSize)
        {
            logFile.close();
            enforceMaxLogFiles();
            // Ha a fájl túl nagy, létrehozunk egy új fájlnevet időbélyeggel
            String newLogFile = currentLogFile;
            int counter = 1;
            while (SPIFFS.exists(newLogFile))
            {
                newLogFile = currentLogFile.substring(0, currentLogFile.length() - 4) + "_" + String(counter) + ".txt";
                counter++;
            }
            currentLogFile = newLogFile;
            logFile = SPIFFS.open(currentLogFile, "a");
        }
        if (logFile)
        {
            logFile.print(logEntry);
            logFile.close();
        }
        else
        {
            Serial.println("Nem sikerült megnyitni az új log fájlt.");
        }
    }
    else
    {
        Serial.println("Nem sikerült megnyitni a log fájlt.");
    }
}

boolean sendFile(String path)
{
    File file = SPIFFS.open(path, "r");
    if (!file)
    {
        return false;
    }

    // Fájl méretének ellenőrzése és tartalom küldése
    String contentType = getContentType(path); // Tartalom típus meghatározása
    server.streamFile(file, contentType);
    file.close();
    return true;
}

void handleLogs()
{
    if (server.method() == HTTP_GET)
    {
        // Válasz a jelenlegi konfiguráció JSON formátumban
        /* StaticJsonDocument<512> jsonBuffer;
         jsonBuffer["wifiSSID"] = config.wifiSSID;
         jsonBuffer["wifiPassword"] = config.wifiPassword;

         String response;
         serializeJson(jsonBuffer, response);
         server.send(200, "application/json", response);*/

        sendFile("/log.txt");
    }
}
#else
void logMessage(const char *format, ...)
{
}
#endif
