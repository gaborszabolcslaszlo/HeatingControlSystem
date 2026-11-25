#include "platform_compat.h"

extern void handleWebUserInterfaceRequest();

void setupWebRoutes(ESP8266WebServer &server) {
    server.on("/chunk-OCY2Z4ZF.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-GI5K245U.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-7WVX3TJP.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-PPMYEPSE.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-EKHZB7JQ.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/styles-57STMZLU.css", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-PT4YT34D.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-4FVY4ALO.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/polyfills-5CFQRCPP.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-JBYBRMTW.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-JNH2XJNQ.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-DX6P6CXM.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/favicon.ico", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/config.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-OSQMNGTH.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-L6EGDXNG.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/3rdpartylicenses.txt", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-MJPOH5HG.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-7UTFF36H.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/main-UDACYG5T.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-3RKIWRIW.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-O2QFVHY2.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-MLQIMDH4.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/index.html", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/build-info.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-GMD5DSNC.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-ESHVIENM.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-JVWFAM56.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-N6YWMH7K.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-QATW22KT.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-ANDYEVZT.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-42ZXBYAK.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/chunk-4LIZICYH.js", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/prerendered-routes.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/img/flag-en.svg", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/img/flag-ro.svg", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/img/jsoneditor-icons.svg", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/img/flag-hu.svg", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/mock-/testconfig.json", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/media/flag-en.svg", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/media/flag-ro.svg", HTTP_GET, handleWebUserInterfaceRequest);
    server.on("/media/flag-hu.svg", HTTP_GET, handleWebUserInterfaceRequest);
}
