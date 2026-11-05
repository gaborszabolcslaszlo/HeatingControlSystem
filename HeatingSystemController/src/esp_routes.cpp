#include "platform_compat.h"

extern void handleWebUserInterfaceRequest();

void setupWebRoutes(ESP8266WebServer &server) {
    server.on("/chunk-FDERIQAA.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-T2EJRZCG.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-D7KOTXTX.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-FSH7XGJD.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/styles-TIGEGTDJ.css", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/polyfills-5CFQRCPP.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-2FYX6YH3.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/favicon.ico", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/config.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-3YS62J5R.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/main-LFFXPF5U.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-XVH6FDWV.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/3rdpartylicenses.txt", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-CUE3N72M.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-WILS5BCL.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/index.html", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-HXALNZ2F.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-OUCQWUSL.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-72NHJVXV.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-4HFINCG7.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/prerendered-routes.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/img/jsoneditor-icons.svg", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/mock-/testconfig.json", HTTP_GET, handleWebUserInterfaceRequest);
}
