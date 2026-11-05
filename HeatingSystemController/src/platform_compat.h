#pragma once

// A projekt eredetileg ESP8266 D-lábneveket használ. Az ESP32 környezetben
// ezeket explicit GPIO számokra képezzük le, hogy a hardver bekötése változatlan
// maradhasson az alapértelmezett esp32dev panelen.
#ifdef ESP32

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

using ESP8266WebServer = WebServer;

#ifndef D0
#define D0 16
#endif
#ifndef D1
#define D1 5
#endif
#ifndef D2
#define D2 4
#endif
#ifndef D4
#define D4 2
#endif

// Az ESP32-CAM AI-Thinker kamera- és SD-kiosztása miatt a GPIO4/GPIO5
// nem megfelelő általános I2C lábnak. Az ESP32-CAM környezet ezeket
// a lábakat a platformio.ini-ben ESP32_CAM makróval jelöli.
#ifdef ESP32_CAM
#define I2C_SDA_PIN 13
#define I2C_SCL_PIN 14
#else
#define I2C_SDA_PIN D2
#define I2C_SCL_PIN D1
#endif

#else

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>

#define I2C_SDA_PIN D2
#define I2C_SCL_PIN D1

#endif
