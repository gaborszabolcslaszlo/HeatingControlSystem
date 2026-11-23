#pragma once

// A projekt eredetileg ESP8266 D-lábneveket használ. Az ESP32 környezetben
// ezeket explicit GPIO számokra képezzük le.
#ifdef ESP32

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>
#include <vector>
#include <cstring>

// A projekt régi handlerei paraméter nélküli, szinkron WebServer callbackeket
// használnak. Ez az adapter ezeket az AsyncWebServer kéréskezelőihez köti, így
// a meglévő API-kód átírása nélkül is nem blokkoló lesz a hálózati kiszolgálás.
enum HTTPUploadStatus : uint8_t
{
  UPLOAD_FILE_START,
  UPLOAD_FILE_WRITE,
  UPLOAD_FILE_END,
  UPLOAD_FILE_ABORTED
};

struct HTTPUpload
{
  HTTPUploadStatus status = UPLOAD_FILE_ABORTED;
  String filename;
  uint8_t *buf = nullptr;
  size_t currentSize = 0;
};

class AsyncCompatWebServer
{
public:
  using RequestHandler = void (*)();
  using UploadHandler = int (*)();

  explicit AsyncCompatWebServer(uint16_t port) : asyncServer(port) {}

  void begin() { asyncServer.begin(); }
  void handleClient() {}
  void keepAlive(bool) {}
  void enableCORS(bool enabled)
  {
    corsEnabled = enabled;
  }

  void on(const char *uri, WebRequestMethodComposite method, RequestHandler handler)
  {
    registerRoute(uri, method, handler, nullptr);
  }

  void on(const char *uri, WebRequestMethodComposite method, RequestHandler handler,
          UploadHandler uploadHandler)
  {
    registerRoute(uri, method, handler, uploadHandler);
  }

  void onNotFound(RequestHandler handler)
  {
    asyncServer.onNotFound([this, handler](AsyncWebServerRequest *request) {
      invoke(request, handler, nullptr);
    });
  }

  bool hasArg(const char *name) const
  {
    if (!request)
      return false;
    if (strcmp(name, "plain") == 0)
      return body.length() > 0;
    return request->hasParam(name, true) || request->hasParam(name);
  }

  String arg(const char *name) const
  {
    if (!request)
      return String();
    if (strcmp(name, "plain") == 0)
      return body;
    if (request->hasParam(name, true))
      return request->getParam(name, true)->value();
    if (request->hasParam(name))
      return request->getParam(name)->value();
    return String();
  }

  int method() const { return request ? static_cast<int>(request->method()) : 0; }
  String uri() const { return request ? request->url() : String(); }

  void sendHeader(const String &name, const String &value)
  {
    responseHeaders.push_back({name, value});
  }

  void send(int code, const char *contentType = nullptr, const String &content = String())
  {
    responseCode = code;
    responseType = contentType ? contentType : "text/plain";
    responseBody = content;
    responsePrepared = true;
  }

  void setContentLength(size_t) {}
  void sendContent(const String &content)
  {
    responseBody += content;
    responsePrepared = true;
  }

  void sendResponse(AsyncWebServerResponse *response)
  {
    if (!request || !response || responseSent)
      return;
    applyHeaders(response);
    request->send(response);
    responseSent = true;
  }

  size_t streamFile(File &file, const String &contentType, bool download = false)
  {
    if (!request || !file)
      return 0;
    String path = file.name();
    if (!path.startsWith("/"))
      path = "/" + path;

    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, path, contentType, download);
    if (!response)
    {
      send(500, "text/plain", "Failed to create file response");
      return 0;
    }
    applyHeaders(response);
    sendResponse(response);
    return file.size();
  }

  HTTPUpload &upload() { return currentUpload; }
  AsyncWebServerRequest *currentRequest() const { return request; }

private:
  struct Header
  {
    String name;
    String value;
  };

  AsyncWebServer asyncServer;
  AsyncWebServerRequest *request = nullptr;
  String body;
  String responseBody;
  String responseType;
  int responseCode = 200;
  bool responsePrepared = false;
  bool responseSent = false;
  bool corsEnabled = false;
  std::vector<Header> responseHeaders;
  HTTPUpload currentUpload;

  void applyHeaders(AsyncWebServerResponse *response)
  {
    for (const Header &header : responseHeaders)
      response->addHeader(header.name.c_str(), header.value.c_str());
    if (corsEnabled)
      response->addHeader("Access-Control-Allow-Origin", "*");
  }

  void finish()
  {
    if (!request || responseSent)
      return;
    if (!responsePrepared)
      send(404, "text/plain", "Not found");
    AsyncWebServerResponse *response = request->beginResponse(responseCode, responseType, responseBody);
    if (!response)
    {
      request->send(500, "text/plain", "Failed to create response");
      return;
    }
    applyHeaders(response);
    request->send(response);
  }

  void invoke(AsyncWebServerRequest *incoming, RequestHandler handler, UploadHandler uploadHandler)
  {
    request = incoming;
    body = incoming->hasParam("plain", true) ? incoming->getParam("plain", true)->value() : String();
    responseBody = String();
    responseType = "text/plain";
    responseCode = 200;
    responsePrepared = false;
    responseSent = false;
    responseHeaders.clear();
    currentUpload = HTTPUpload{};
    if (uploadHandler)
      uploadHandler();
    if (handler)
      handler();
    finish();
    request = nullptr;
  }

  void registerRoute(const char *uri, WebRequestMethodComposite method,
                     RequestHandler handler, UploadHandler uploadHandler)
  {
    asyncServer.on(uri, method,
      [this, handler, uploadHandler](AsyncWebServerRequest *incoming) {
        // A body callback is installed below for POST/PUT requests. The
        // request callback is still needed for GET and empty-body requests.
        if (incoming->method() == HTTP_GET || incoming->contentLength() == 0)
          invoke(incoming, handler, uploadHandler);
      },
      [this, handler, uploadHandler](AsyncWebServerRequest *incoming, String filename,
                            size_t index, uint8_t *data, size_t len, bool final) {
        request = incoming;
        if (index == 0)
        {
          body = String();
          responseBody = String();
          responseType = "text/plain";
          responseCode = 200;
          responsePrepared = false;
          responseSent = false;
          responseHeaders.clear();
          currentUpload.filename = filename;
          currentUpload.status = UPLOAD_FILE_START;
          currentUpload.buf = data;
          currentUpload.currentSize = 0;
          if (uploadHandler)
            uploadHandler();
        }

        // A callback utolsó hívása még tartalmazhat adatot. Ezt először
        // WRITE állapotban ki kell írni, különben a fájl vége elveszik.
        if (len > 0)
        {
          currentUpload.status = UPLOAD_FILE_WRITE;
          currentUpload.buf = data;
          currentUpload.currentSize = len;
          if (uploadHandler)
            uploadHandler();
        }

        if (final)
        {
          currentUpload.status = UPLOAD_FILE_END;
          currentUpload.buf = nullptr;
          currentUpload.currentSize = 0;
          if (uploadHandler)
            uploadHandler();
          if (handler)
            handler();
          finish();
          request = nullptr;
        }
      },
      [this, handler, uploadHandler](AsyncWebServerRequest *incoming, uint8_t *data,
                                    size_t len, size_t index, size_t total) {
        request = incoming;
        if (index == 0)
        {
          body = String();
          responseBody = String();
          responseType = "text/plain";
          responseCode = 200;
          responsePrepared = false;
          responseSent = false;
          responseHeaders.clear();
        }
        body.concat(reinterpret_cast<const char *>(data), len);
        if (index + len == total)
        {
          // Multipart feltöltésnél a request body callback is lefuthat, de a
          // konfiguráció feldolgozása csak az upload callback utolsó csomagja
          // után történhet meg, amikor a fájl már teljesen kiírásra került.
          if (!uploadHandler && handler)
            handler();
          if (!uploadHandler)
          {
            finish();
            request = nullptr;
          }
        }
      });
  }
};

using ESP8266WebServer = AsyncCompatWebServer;

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

// ESP32-WROOM DevKit V1 / ESP-WROOM-32D, panelen látható feliratokkal:
//   D21  -> SDA (GPIO21)
//   D22  -> SCL (GPIO22)
//   D26  -> PCA9685 OE (GPIO26)
//   D27  -> DS18B20 OneWire adat (GPIO27)
// Ezeket a lábakat a platformio.ini ESP32_WROOM makrója választja ki.
#ifdef ESP32_WROOM
#undef D0
#undef D1
#undef D2
#undef D4
#define D0 26 // Fejlesztőlap felirata: D26, PCA9685 output enable
#define D1 22 // Fejlesztőlap felirata: D22, PCA9685 I2C SCL
#define D2 21 // Fejlesztőlap felirata: D21, PCA9685 I2C SDA
#define D4 27 // Fejlesztőlap felirata: D27, DS18B20 OneWire adat
#define STATUS_LED_PIN 2 // Fejlesztőlap felirata: D2, beépített kék LED
#ifndef HEALTH_YELLOW_PIN
#define HEALTH_YELLOW_PIN 25 // Fejlesztőlap felirata: D25, sárga hibajelzés
#endif
#ifndef HEALTH_RED_PIN
#define HEALTH_RED_PIN 33 // Fejlesztőlap felirata: D33, piros hibajelzés / buzzer meghajtó
#endif
#define I2C_SDA_PIN D2
#define I2C_SCL_PIN D1
#define ONE_WIRE_BUS_PIN D4
// Az ESP32-CAM AI-Thinker kamera- és SD-kiosztása miatt a GPIO4/GPIO5
// nem megfelelő általános I2C lábnak.
#elif defined(ESP32_CAM)
#define I2C_SDA_PIN 13
#define I2C_SCL_PIN 14
#define ONE_WIRE_BUS_PIN 2
#define STATUS_LED_PIN 2
#else
#define I2C_SDA_PIN D2
#define I2C_SCL_PIN D1
#define ONE_WIRE_BUS_PIN D4
#define STATUS_LED_PIN D4
#endif

#else

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>

#define I2C_SDA_PIN D2
#define I2C_SCL_PIN D1
#define STATUS_LED_PIN D4

#endif
