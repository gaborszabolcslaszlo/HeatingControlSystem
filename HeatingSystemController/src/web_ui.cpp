#include "web_ui.h"

#include <FS.h>
#include "app_globals.h"
#include "comon.h"
void handleFile()
{
  String path = server.uri();
  if (path.endsWith("/"))
    path += "index.html"; // főoldal
  String contentType = getContentType(path);

  if (SPIFFS.exists(path))
  {
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, contentType);
    file.close();
  }
  else
  {
    // Ha nem létezik: Angular routing -> index.html visszaadása
    File file = SPIFFS.open("/index.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  }
}

void handleWebUserInterfaceRequest()
{
  String path = server.uri(); // pl. "/" vagy "/style.css"

  // A böngésző automatikusan lekéri a favicon-t. Ha nincs külön ikon a
  // SPIFFS-en, ne próbáljuk a teljes webes fájlkezelési útvonalon kiszolgálni,
  // mert ez felesleges AsyncWebServer/SPIFFS munkát okoz.
  if (path == "/favicon.ico")
  {
    server.send(204, "image/x-icon", "");
    return;
  }

  // Ha "/" érkezik, az index.html legyen
  if (path == "/")
  {
    path = "/index.html";
  }

  String fullPath = "" + path; // a PROGMEM/SPIFFS útvonal
  logDebug("Serving: %s\n", fullPath.c_str());

  // MIME típus meghatározása
  String contentType = "text/plain";
  bool isGzipped = false;

  if (path.endsWith(".html"))
  {
    contentType = "text/html";
  }
  else if (path.endsWith(".css"))
  {
    contentType = "text/css";
    fullPath += ".gz";
    isGzipped = true;
  }
  else if (path.endsWith(".js"))
  {
    contentType = "application/javascript";
    fullPath += ".gz";
    isGzipped = true;
  }
  else if (path.endsWith(".png"))
  {
    contentType = "image/png";
  }
  else if (path.endsWith(".jpg") || path.endsWith(".jpeg"))
  {
    contentType = "image/jpeg";
  }
  else if (path.endsWith(".svg"))
  {
    contentType = "image/svg+xml";
  }

  if (!SPIFFS.exists(fullPath))
  {
    server.send(404, "text/plain", "File not found");
    return;
  }

  if (isGzipped)
  {
    // A fájl tartalma már a build során gzip-pel tömörítve került a SPIFFS-be.
    // Ezt a fejlécet a böngésző használja a kitömörítéshez; nélküle a gzip
    // bájtokat JavaScriptként próbálja értelmezni.
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "public, max-age=3600");
    server.sendHeader("ETag", "v1"); // egyszerű statikus ETag
  }
  // server.sendHeader("Cache-Control", "max-age=31536000");

  File file = SPIFFS.open(fullPath, "r");
  logDebug("Send file: %s\n", fullPath.c_str());
  server.streamFile(file, contentType);
  file.close();
}

