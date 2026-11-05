// comon.h

// #ifndef COMON_H
// #define COMON_H

#pragma once

// Alkalmazásszintű naplózás.
#ifndef DISABLE_APPLICATION_LOGGING
#define DISABLE_APPLICATION_LOGGING 0
#endif

enum LogLevel
{
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_DEBUG = 3
};

// A megadott és annál fontosabb szintek jelennek meg.
#ifndef APPLICATION_LOG_LEVEL
#define APPLICATION_LOG_LEVEL LOG_LEVEL_WARN
#endif

#ifndef APPLICATION_LOG_BUFFER_SIZE
#define APPLICATION_LOG_BUFFER_SIZE 2048U
#endif

#ifndef APPLICATION_LOG_FLUSH_INTERVAL_MS
#define APPLICATION_LOG_FLUSH_INTERVAL_MS 5000UL
#endif

#ifndef UNIT_TESTING
#include "platform_compat.h"
#include <WiFiUdp.h>
#include <NTPClient.h>

extern ESP8266WebServer server;

extern WiFiUDP ntpUDP;
extern NTPClient timeClient; // 1 órás időeltolás (3600 másodperc), 600 másodpercenként frissít

// Log fájl neve
extern const char *logFileName;

// Tartalom típus meghatározása fájlnév alapján
String getContentType(String filename);
String getFormattedDate();

void createLogFile();
void writeLogMessage(const char *format, ...);
void flushLogBuffer(bool force = false);

#if DISABLE_APPLICATION_LOGGING
#define LOG_WRITE(...) \
    do                 \
    {                  \
    } while (0)
#else
#define LOG_WRITE(level, format, ...)              \
    do                                             \
    {                                              \
        if ((level) <= APPLICATION_LOG_LEVEL)      \
        {                                          \
            writeLogMessage(format, ##__VA_ARGS__); \
        }                                          \
    } while (0)
#endif

#define logError(format, ...) LOG_WRITE(LOG_LEVEL_ERROR, format, ##__VA_ARGS__)
#define logWarn(format, ...) LOG_WRITE(LOG_LEVEL_WARN, format, ##__VA_ARGS__)
#define logInfo(format, ...) LOG_WRITE(LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define logDebug(format, ...) LOG_WRITE(LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)

// Kompatibilitás a meglévő hívásokkal: ezek INFO szintű üzenetek.
#define logMessage(format, ...) logInfo(format, ##__VA_ARGS__)

boolean sendFile(String path);

void handleLogs();

#else
// Logger funkció, az unit testhez.
#if DISABLE_APPLICATION_LOGGING && !defined(COMON_IMPLEMENTATION)
#define logMessage(...) \
    do                  \
    {                   \
    } while (0)
#else
void logMessage(const char *format, ...);
#endif

#endif
// #endif
