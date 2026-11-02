# API integration tests

These tests target the API exposed by `main.cpp` in the `TESTINGOVER_API`
PlatformIO environment.

Create and activate the project virtual environment:

```bash
cd /home/laca/Documents/AlfaOmega/Projektek/HeatingControlSystem/HeatingSystemController
python3 -m venv .venv
source .venv/bin/activate
```

Install the test dependencies inside the active virtual environment:

```bash
python -m pip install -r requirements-test.txt
```

Run against the controller over HTTP on port 80:

```bash
HEATING_CONTROLLER_URL=http://192.168.0.106 \
python -m pytest -m integration -v
```

Verbose módok:

```bash
# Részletes tesztlista és eredmény
python -m pytest -m integration -v

# Még részletesebb kimenet
python -m pytest -m integration -vv

# A tesztek konzolkimenetének megjelenítése is
python -m pytest -m integration -vv -s
```

### Tesztriport fájl készítése

JUnit XML riport készítése:

```bash
mkdir -p reports
python -m pytest -m integration -vv \
  --junitxml=reports/pytest-results.xml
```

A riport helye:

```text
reports/pytest-results.xml
```

### Részletes API kérés-válasz riport

Az API kommunikáció részletes JSON riportba is menthető. Ez tartalmazza a tesztek
által küldött HTTP metódust, URL-t, fejléceket és törzset, valamint az eszköz
válaszának státuszkódját, fejléceit, törzsét és válaszidejét.

```bash
mkdir -p reports
python -m pytest -m integration -vv \
  --api-trace=reports/api-trace.json \
  --api-report=reports/api-report.md \
  --api-html-report=reports/api-report.html \
  --junitxml=reports/pytest-results.xml
```

A részletes riport helye:

```text
reports/api-trace.json
```

Az ember által olvasható tesztriport alapértelmezett helye:

```text
reports/api-report.md
```

Az interaktív, böngészőben megnyitható riport helye:

```text
reports/api-report.html
```

A HTML riportban a tesztesetek és az API lépések összecsukható és kibontható
blokkokban jelennek meg.

A tesztek fa struktúrában jelennek meg: először a tesztfájl, azon belül az API
funkcionális, API stressz- vagy állapotátmeneti csoport, végül az egyes
tesztesetek láthatók. A csoportok mellett a sikeres, hibás és kihagyott tesztek
darabszáma is megjelenik.

Az állapotátmeneti teszteknél a riport külön ellenőrzési pontokat is tartalmaz:
az elvárt és a tényleges érték zöld színnel jelenik meg siker esetén, piros
színnel eltérés esetén.

A HTML riportban a `GET /state` JSON válaszának `state` értékei is színezve
jelennek meg: zöld, ha egyeznek az adott ellenőrzési pont elvárt értékével,
piros, ha eltérnek. Az érték fölé húzva az egérmutatót az elvárt érték is
megjelenik.

A `setTemp` hívás után lekért `/state` válaszban a módosított szenzorértékek
sorai is színezve jelennek meg. A sor zöld, ha az eszköz a beállított értéket
adta vissza, és piros, ha eltérő értéket adott vissza.

### `/state` API stresszteszt

A `test_state_api_stress_after_set_temperature` teszt egy `setTemp` hívás után
ciklusban lekéri a `/state` végpontot. Minden válasznál ellenőrzi, hogy a HTTP
kérés sikeres volt-e, és hogy a válasz JSON objektum-e. Az alapértelmezett
ciklusszám 100.

```bash
HEATING_CONTROLLER_URL=http://192.168.0.106 \
STATE_STRESS_ITERATIONS=100 \
STATE_STRESS_DELAY_SECONDS=0.1 \
python -m pytest tests/test_api.py::test_state_api_stress_after_set_temperature \
  -m integration -vv -s
```

A ciklusszám és a lekérések közötti várakozás módosítható:

```bash
STATE_STRESS_ITERATIONS=500 STATE_STRESS_DELAY_SECONDS=0 \
python -m pytest tests/test_api.py::test_state_api_stress_after_set_temperature \
  -m integration -vv -s
```

Az első `/state` lekérés a `setTemp` utáni, jelenleg 5 másodperces beállási
időt a közös `read_state` fixture-en keresztül kivárja. A kérések bekerülnek a
JSON, Markdown és HTML API riportokba is.

### Az összes API stressztesztje

A további stressztesztek a következő végpontokat vizsgálják:
`/heatingRequest`, `/logs`, `/configuration`, `/manualControlPumps` és `/reboot`.
Az újraindítási teszt csak hibás hitelesítést küld, ezért nem indítja újra az
eszközt. A konfiguráció POST feltöltését a teszt nem ismétli, mert az flashre
ír és megváltoztatná az eszköz konfigurációját.

Az összes API stresszteszt futtatása:

```bash
API_STRESS_ITERATIONS=100 \
python -m pytest tests/test_api.py -m integration -vv -s
```

Az ismétlések száma csökkenthető egy gyors próba kedvéért:

```bash
API_STRESS_ITERATIONS=10 \
python -m pytest tests/test_api.py -m integration -vv -s
```

### Kazán–puffer–radiátor állapotátmenetek

Az állapotátmeneti tesztek a `tests/test_state_transitions.py` fájlban
találhatók. Ezek a következő működési láncokat vizsgálják:

- kazán: `COOLD -> RETOUR_PROTECTION -> HEATING -> OVERHEATED -> HEATING`;
- radiátor: `STANDBY -> WAITING_FOR_HEATING -> RECEIVING_HEATING -> STANDBY`;
- puffer: `STANDBY -> CHARGE -> DISCHARGE -> STANDBY`.

Az aktuális központi vezérlő módja a `/state` válaszban a
`HeatingController.state` mezőben jelenik meg.

Ebben minden teszteset külön fejezetként jelenik meg, látható a teszteset
leírása, az egyes lépések sorrendje, a küldött kérés, az eszköz válasza és a
lépések válaszideje.

Egy `GET` kérésnek általában nincs törzse, ezért a kérésnél az `(üres)` érték
normális. Az eszköz válaszának törzse külön jelenik meg, például a `/state`
végpont esetén ez tartalmazza az aktuális állapotot.

Egyedi riportfájl is megadható:

```bash
python -m pytest -m integration -vv \
  --api-trace=reports/device-test-2026-10-31.json \
  --api-report=reports/device-test-2026-10-31.md \
  --api-html-report=reports/device-test-2026-10-31.html
```

When the virtual environment is no longer needed:

```bash
deactivate
```

The test environment uses the sensor IDs from `config/config.json`; it does not
duplicate those IDs in the test code.
