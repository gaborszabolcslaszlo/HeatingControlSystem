#include "platform_compat.h"

extern void handleWebUserInterfaceRequest();

void setupWebRoutes(ESP8266WebServer &server) {
    server.on("/chunk-ZASAKTGF.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-MG3KREYE.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/styles-TIGEGTDJ.css", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/polyfills-5CFQRCPP.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-57RXTHPG.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-ICMV6BLG.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/favicon.ico", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/config.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-OSQMNGTH.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-DDGKWJKN.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-4LOHYQGA.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-FBUI7H6I.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-L6EGDXNG.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-5ESMVYO6.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/3rdpartylicenses.txt", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-3RKIWRIW.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-GZBDS4DE.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/index.html", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-NEFMFGIT.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-J7OV3TBE.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-2G7QVSGG.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/main-N4YZXRTE.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-ZE3IFFO2.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-UFCVJF5I.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-FCF6FK67.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-QATW22KT.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-5H6NEO75.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/prerendered-routes.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/img/jsoneditor-icons.svg", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/mock-/testconfig.json", HTTP_GET, handleWebUserInterfaceRequest);
}
