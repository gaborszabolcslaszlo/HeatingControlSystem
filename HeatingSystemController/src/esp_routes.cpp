#include <ESP8266WebServer.h>

extern void handleWebUserInterfaceRequest();

void setupWebRoutes(ESP8266WebServer &server) {
    server.on("/chunk-5RMPLDRU.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-QW5SIL57.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-J7QTW3HM.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-HON3JT5H.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-JZ4IRRXY.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/favicon.ico", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/config.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-4RLAD6CJ.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-G4Q2ZA5X.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-IY25BWT3.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-EZ744CWP.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/main.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/index.html", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-DUUVGNO4.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/styles.css", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-4NK7EJ4N.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/polyfills.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-4NBGH7XJ.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/assets/.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/img/jsoneditor-icons.svg", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/mock-/testconfig.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/media/jsoneditor-icons.svg", HTTP_GET, handleWebUserInterfaceRequest);
}
