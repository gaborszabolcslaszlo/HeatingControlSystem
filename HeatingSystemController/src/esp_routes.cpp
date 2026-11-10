#include "platform_compat.h"

extern void handleWebUserInterfaceRequest();

void setupWebRoutes(ESP8266WebServer &server) {
    server.on("/styles-TIGEGTDJ.css", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/polyfills-5CFQRCPP.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-NLASJOXY.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/favicon.ico", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-YNVFHQ6G.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-KCOYRGOW.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/config.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-OSQMNGTH.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/main-HOLS74VN.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-44V4SASC.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-L6EGDXNG.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/3rdpartylicenses.txt", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-3RKIWRIW.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/index.html", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-XX42M77V.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-2U4LTPJF.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-MMLXR7CD.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-UK5S5SP2.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-RXAEVWPG.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-VUAW2AX4.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-CCJ53F72.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-QN3THLCL.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-LJYBWGHH.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-7LVQEHPO.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-QATW22KT.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-B3NLQRSQ.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/prerendered-routes.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/img/jsoneditor-icons.svg", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/mock-/testconfig.json", HTTP_GET, handleWebUserInterfaceRequest);
}
