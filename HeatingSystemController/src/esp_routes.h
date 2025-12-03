#pragma once
#include <ESP8266WebServer.h>

extern void handleWebUserInterfaceRequest();

void setupWebRoutes(ESP8266WebServer &server);
