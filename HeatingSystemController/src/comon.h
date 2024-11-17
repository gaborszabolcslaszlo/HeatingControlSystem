// comon.h

// #ifndef COMON_H
// #define COMON_H

#pragma once

#ifndef UNIT_TESTING
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ESP8266WebServer.h>

extern ESP8266WebServer server;

extern WiFiUDP ntpUDP;
extern NTPClient timeClient; // 1 órás időeltolás (3600 másodperc), 600 másodpercenként frissít

// Log fájl neve
extern const char *logFileName;

// Tartalom típus meghatározása fájlnév alapján
String getContentType(String filename);

void createLogFile();

// Logger funkció, ami hasonló a printf()-hez
void logMessage(const char *format, ...);

boolean sendFile(String path);

void handleLogs();

#else
// Logger funkció, az unit testhez.
void logMessage(const char *format, ...);

#endif
// #endif
