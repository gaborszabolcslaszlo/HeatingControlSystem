#pragma once

// A dinamikus API-végpontok közötti minimális időköz.
// Build flaggel konfigurálható, például: -DAPI_MIN_REQUEST_INTERVAL_MS=250.
// A 0 érték kikapcsolja a limitet.
#ifndef API_MIN_REQUEST_INTERVAL_MS
#define API_MIN_REQUEST_INTERVAL_MS 0UL
#endif

constexpr unsigned long API_MIN_REQUEST_INTERVAL_MS_VALUE =
    API_MIN_REQUEST_INTERVAL_MS;

bool allowApiRequest();
void handleHeatingRequest();
void handleReboot();
void handleSetTemperatureTest();
void handleManualMode();
void handleDeviceInfo();
void hadleStateRequest();
