#pragma once

// A dinamikus API-végpontok közötti minimális időköz.
constexpr unsigned long API_MIN_REQUEST_INTERVAL_MS = 250;

bool allowApiRequest();
void handleHeatingRequest();
void handleReboot();
void handleSetTemperatureTest();
void handleManualMode();
void handleDeviceInfo();
void hadleStateRequest();
