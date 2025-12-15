#include <ESP8266WebServer.h>

extern void handleWebUserInterfaceRequest();

void setupWebRoutes(ESP8266WebServer &server) {
    server.on("/chunk-FDERIQAA.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-QNS42V4J.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-UXNJRG6E.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/favicon.ico", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/config.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-RLIN3SJI.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-SZXWT5CF.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-VK6FF2CX.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-X7WD3GZQ.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/main.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/index.html", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/styles.css", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/polyfills.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-5VPOCR6L.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-HXALNZ2F.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-4U4TMMME.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-VY3YD44W.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-NLN4MQFZ.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/img/jsoneditor-icons.svg", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/mock-/testconfig.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/media/jsoneditor-icons.svg", HTTP_GET, handleWebUserInterfaceRequest);
}
