import shutil
import os

# forrás könyvtár (másik projekt mockdata mappája)
source_dir = "/home/laca/Documents/AlfaOmega/Projektek/KazanVezerlo/Websajat/HeatingSystemWebApp/dist/heating-system-web-app/browser"
# cél könyvtár (aktuális projekt data/mockdata)
target_dir = "data/browser"

# ha nem létezik a cél, hozd létre
os.makedirs(target_dir, exist_ok=True)

# másold át a teljes könyvtárat
if os.path.exists(source_dir):
    # először töröld a régi mockdata mappát (ha van)
    if os.path.exists(target_dir):
        shutil.rmtree(target_dir)
    shutil.copytree(source_dir, target_dir)
    print(f"✅ Web interface copied from {source_dir} to {target_dir}")
else:
    print(f"⚠️ Source directory not found: {source_dir}")
