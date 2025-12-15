import os
import shutil
import subprocess

import gzip
import shutil
from pathlib import Path

def gzip_folder(folder, pattern):
    for path in Path(folder).rglob(pattern):
        gz_path = path.with_suffix(path.suffix + '.gz')
        with open(path, 'rb') as f_in:
            with gzip.open(gz_path, 'wb', compresslevel=9) as f_out:
                shutil.copyfileobj(f_in, f_out)
        path.unlink()  # eredeti fájl törlése
        print(f'Compressed {path} -> {gz_path}')



# forrás könyvtár (másik projekt mockdata mappája)
source_dir = "/home/laca/Documents/AlfaOmega/Projektek/KazanVezerlo/Websajat/HeatingSystemWebApp/dist/heating-system-web-app/browser"
# cél könyvtár (aktuális projekt data/mockdata)
target_dir = "data"



# ha nem létezik a cél, hozd létre
os.makedirs(target_dir, exist_ok=True)

# másold át a teljes könyvtárat
if os.path.exists(source_dir):
    # először töröld a régi mockdata mappát (ha van)
   # if os.path.exists(target_dir):
    #    shutil.rmtree(target_dir)
   # shutil.copytree(source_dir, target_dir)
    print(f"✅ Web interface copied from {source_dir} to {target_dir}")
else:
    print(f"⚠️ Source directory not found: {source_dir}")

# JS fájlok
gzip_folder('data', '*.js')

# CSS fájlok
gzip_folder('data', '*.css')

shutil.copy2('config/config.json', 'data')



BUILD_DIR = target_dir          # <-- Angular build mappa
OUTPUT_FILE = "src/esp_routes.cpp"

routes = []

# Walk through all files
for root, dirs, files in os.walk(BUILD_DIR):
    for file in files:
        full_path = os.path.join(root, file)
        rel_path = full_path.replace(BUILD_DIR, "").replace(".gz", "").replace("\\", "/")

        if not rel_path.startswith("/"):
            rel_path = "/" + rel_path

        routes.append(f'    server.on("{rel_path}", HTTP_GET, handleWebUserInterfaceRequest);')

# Generáljuk a függvényt
function_code = """\
#include <ESP8266WebServer.h>

extern void handleWebUserInterfaceRequest();

void setupWebRoutes(ESP8266WebServer &server) {{
{routes}
}}
""".format(routes="\n".join(routes))

# Írjuk ki a fájlba
with open(OUTPUT_FILE, "w") as f:
    f.write(function_code)

print("✔ Generated:", OUTPUT_FILE)
print(f"✔ Total routes: {len(routes)}")
