#define COMON_IMPLEMENTATION
#include "comon.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>
#include <vector>

#ifndef UNIT_TESTING
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 600000); // 1 órás időeltolás (3600 másodperc), 600 másodpercenként frissít

// Log fájl neve
const char *logFileName = "/log.txt";
constexpr size_t MAX_TOTAL_LOG_SIZE = 10240;
char applicationLogBuffer[APPLICATION_LOG_BUFFER_SIZE + 1];
size_t applicationLogBufferLength = 0;
unsigned long applicationLogBufferStartedAt = 0;
String applicationLogBufferFile;

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
    File logFile = SPIFFS.open(logFileName, "a");
    if (logFile)
    {
        logFile.close();
    }
}

namespace
{
String normalizeLogPath(const String &path)
{
    return path.startsWith("/") ? path : String("/") + path;
}

bool isLogFile(const String &name)
{
    const String path = normalizeLogPath(name);
    return path == "/log.txt" ||
           (path.startsWith("/log_") && path.endsWith(".txt"));
}

void collectLogFiles(std::vector<String> &logFiles, size_t &totalSize)
{
    totalSize = 0;
#ifdef ESP32
    File root = SPIFFS.open("/");
    File entry = root.openNextFile();
    while (entry)
    {
        const String name = normalizeLogPath(entry.name());
        if (isLogFile(name))
        {
            logFiles.push_back(name);
            totalSize += entry.size();
        }
        entry = root.openNextFile();
    }
#else
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    {
        const String name = normalizeLogPath(dir.fileName());
        if (isLogFile(name))
        {
            logFiles.push_back(name);
            totalSize += dir.fileSize();
        }
    }
#endif
    std::sort(logFiles.begin(), logFiles.end());
}

void enforceLogStorage(size_t incomingBytes, const String &currentFile)
{
    std::vector<String> logFiles;
    size_t totalSize = 0;
    collectLogFiles(logFiles, totalSize);

    // Delete the oldest files until the new line fits into the global 10 KB
    // budget. The current file may also be removed if it is the only file left
    // and has no room for the new line.
    while (!logFiles.empty() && totalSize + incomingBytes > MAX_TOTAL_LOG_SIZE)
    {
        const String oldest = logFiles.front();
        File oldFile = SPIFFS.open(oldest, "r");
        const size_t oldSize = oldFile ? oldFile.size() : 0;
        if (oldFile)
        {
            oldFile.close();
        }
        SPIFFS.remove(oldest);
        totalSize = totalSize > oldSize ? totalSize - oldSize : 0;
        logFiles.erase(logFiles.begin());
    }

    // If one log line is larger than the complete budget, start a fresh
    // current file; it is preferable to keep the newest line than to exceed
    // the global limit.
    if (totalSize + incomingBytes > MAX_TOTAL_LOG_SIZE && SPIFFS.exists(currentFile))
    {
        SPIFFS.remove(currentFile);
    }
}
}

String getFormattedDate()
{
    const time_t epochTime = static_cast<time_t>(timeClient.getEpochTime());
    const tm *calendar = gmtime(&epochTime);
    char dateStr[11] = "1970-01-01";
    if (calendar != nullptr)
    {
        snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d",
                 calendar->tm_year + 1900, calendar->tm_mon + 1, calendar->tm_mday);
    }
    return String(dateStr);
}

// Segédfüggvény: aktuális dátummal ellátott log fájl neve
String getLogFileName()
{
    // Példa formátum: /log_2025-10-26.txt
    String dateStr = getFormattedDate(); // pl. "2025-10-26"
    return String("/log_") + dateStr + ".txt";
}

void writeLogMessage(const char *format, ...)
{
    char message[512];
    va_list arguments;
    va_start(arguments, format);
    const int written = vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    if (written < 0)
    {
        return;
    }

    char line[560];
    const int lineLength = snprintf(line, sizeof(line), "[%02d:%02d:%02d] %s",
                                    timeClient.getHours(),
                                    timeClient.getMinutes(),
                                    timeClient.getSeconds(),
                                    message);
    if (lineLength <= 0)
    {
        return;
    }

    Serial.print(line);

    const size_t lineBytes = strnlen(line, sizeof(line));
    if (lineBytes == 0)
    {
        return;
    }

    const String currentFile = getLogFileName();
    if (applicationLogBufferLength > 0 && applicationLogBufferFile != currentFile)
    {
        flushLogBuffer(true);
    }

    if (lineBytes > APPLICATION_LOG_BUFFER_SIZE)
    {
        // The formatted line is currently bounded by the local 560-byte
        // buffer, but keep this path safe if the buffer size is configured
        // below that value.
        flushLogBuffer(true);
        enforceLogStorage(lineBytes, currentFile);
        File logFile = SPIFFS.open(currentFile, "a");
        if (logFile)
        {
            logFile.seek(logFile.size(), SeekSet);
            logFile.write(reinterpret_cast<const uint8_t *>(line), lineBytes);
            logFile.flush();
            logFile.close();
        }
        return;
    }

    if (applicationLogBufferLength == 0)
    {
        applicationLogBufferFile = currentFile;
        applicationLogBufferStartedAt = millis();
    }

    if (applicationLogBufferLength + lineBytes > APPLICATION_LOG_BUFFER_SIZE)
    {
        flushLogBuffer(true);
        applicationLogBufferFile = currentFile;
        applicationLogBufferStartedAt = millis();
    }

    memcpy(applicationLogBuffer + applicationLogBufferLength, line, lineBytes);
    applicationLogBufferLength += lineBytes;
    applicationLogBuffer[applicationLogBufferLength] = '\0';
}

void flushLogBuffer(bool force)
{
    if (applicationLogBufferLength == 0)
    {
        return;
    }

    if (!force && millis() - applicationLogBufferStartedAt < APPLICATION_LOG_FLUSH_INTERVAL_MS)
    {
        return;
    }

    const String fileName = applicationLogBufferFile.length() > 0
                                ? applicationLogBufferFile
                                : getLogFileName();
    enforceLogStorage(applicationLogBufferLength, fileName);

    File logFile = SPIFFS.open(fileName, "a");
    if (logFile)
    {
        logFile.seek(logFile.size(), SeekSet);
        logFile.write(reinterpret_cast<const uint8_t *>(applicationLogBuffer),
                      applicationLogBufferLength);
        logFile.flush();
        logFile.close();
    }

    applicationLogBufferLength = 0;
    applicationLogBufferStartedAt = 0;
    applicationLogBufferFile = "";
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

    if (server.hasArg("act"))
    {
        String fileName = getLogFileName();

        if (!sendFile(fileName))
        {
            server.send(404, "text/plain", "File Not Found");
        }
        return;
    }

    // /logs?name=filename.txt  → fájl tartalma
    if (server.hasArg("name"))
    {
        String fileName = server.arg("name");

        if (!sendFile(fileName))
        {
            server.send(404, "text/plain", "File Not Found");
        }
        return;
    }

    // Ha nincs name paraméter → listázzuk a /log mappát
    String json = "[";
    bool first = true;

#ifdef ESP32
    File root = SPIFFS.open("/");
    File entry = root.openNextFile();
    while (entry)
    {
        if (!first)
            json += ",";
        first = false;
        json += "\"" + String(entry.name()) + "\"";
        entry = root.openNextFile();
    }
#else
    Dir dir = SPIFFS.openDir("/");
    while (dir.next())
    {
        if (!first)
            json += ",";
        first = false;

        json += "\"" + dir.fileName() + "\"";
    }
#endif

    json += "]";

    server.send(200, "application/json", json);
}

#else
void logMessage(const char *format, ...)
{
}

#endif
