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


python -m pytest -m integration -v
```

### API-kérés korlátozás kezelése

A firmware API-kérés-limitje a `API_MIN_REQUEST_INTERVAL_MS` build flaggel
állítható. Alapértelmezésben `0`, vagyis ki van kapcsolva; például a
`-DAPI_MIN_REQUEST_INTERVAL_MS=250` 250 ms-os limitet kapcsol be.

A tesztek ettől függetlenül kezelik a korábbi firmware-verziók `429 Too Many
Requests` válaszát: legfeljebb két alkalommal újrapróbálják a kérést, és követik
az eszköz `Retry-After` fejlécét.

Az állapotellenőrzések előtt a tesztek alapértelmezésben 3 másodpercet várnak a
vezérlő állapotának beállására. Ez a `STATE_SETTLE_SECONDS` környezeti változóval
felülírható.

Ha az első `/state` válaszban még nem látható az elvárt állapot, az
állapotátmeneti tesztek legfeljebb 12 másodpercig újra lekérdezik az API-t, a
lekérések között a fenti 3 másodperces időközzel.

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

### PID pumpa teszt

A `test_puffer_pid_pumps_produce_pwm_output` teszt hőmérsékletadatokat küld a
kazán és a puffer szenzoraira, majd a beállási idő után lekéri a `/state`
végpontot. Ellenőrzi, hogy a puffer `CHARGE` állapotba került, és hogy a
konfiguráció szerinti 3-as és 4-es PWM kimeneten számszerű, 0 és 100 közötti
vezérlési érték jelent meg.

```bash
HEATING_CONTROLLER_URL=http://192.168.0.106 \
python -m pytest tests/test_api.py::test_puffer_pid_pumps_produce_pwm_output \
  -m integration -vv -s
```

Ez az API integrációs teszt azt ellenőrzi, hogy a PID pumpa kimenete létrejön és
érvényes PWM tartományban van. A PID matematikai szabályozási görbéjének külön
vizsgálatához a firmware oldalon egységteszt szükséges, mert az API state
válaszából csak az aktuális kimenet figyelhető meg.

A `test_puffer_pid_output_follows_temperature_error` teszt a `CHARGE` állapot
alatt csak a `dischargepump: false` konfigurációjú 4-es pumpa kimenetének
trendjét vizsgálja. Negatív hibánál alacsonyabb, pozitív hibánál magasabb,
nulla hibánál stabil, majd ismét negatív hibánál csökkenő kimenetet vár. A
3-as, `dischargepump: true` pumpának közben kikapcsolva kell maradnia.

A `test_puffer_discharge_uses_only_discharge_pid_pump` teszt a
`DISCHARGE` állapotban ellenőrzi a pumpaszerepeket. A kazán retour értéke 45 °C
alatt marad, ezért a kazán nem válthat `HEATING` állapotba. Ilyenkor csak a
3-as, `dischargepump: true` pumpa lehet aktív, a 4-es pumpának kikapcsolva kell
maradnia.

A DISCHARGE PID emelkedési és csökkenési trendje a jelenlegi konfigurációval
nem vizsgálható ugyanebben az állapotban, mert a PID bemenete a kazán retour
szenzora. 45 °C fölötti retourérték a kazánt `HEATING` állapotba viszi, ami a
vezérlőt visszaváltja `CHARGE` működésre. Ehhez külön PID bemeneti szenzor vagy
firmware oldali mérési interfész szükséges.

```bash
HEATING_CONTROLLER_URL=http://192.168.0.106 \
python -m pytest \
  tests/test_api.py::test_puffer_pid_output_follows_temperature_error \
  -m integration -vv -s
```

A `DISCHARGE` trendteszt külön futtatása:

```bash
HEATING_CONTROLLER_URL=http://192.168.0.106 \
python -m pytest \
  tests/test_api.py::test_puffer_discharge_uses_only_discharge_pid_pump \
  -m integration -vv -s
```

HEATING_CONTROLLER_URL=http://192.168.0.107 \
python -m pytest \
  tests/test_api.py::test_radiator_waiting_receiving_and_standby \
  -m integration -vv -s


A két trendteszt együttes futtatása:

```bash
HEATING_CONTROLLER_URL=http://192.168.4.1 \
python -m pytest \
  tests/test_api.py::test_puffer_pid_output_follows_temperature_error \
  tests/test_api.py::test_puffer_discharge_uses_only_discharge_pid_pump \
  -m integration -vv -s
```

### Az összes API stressztesztje

A további stressztesztek a következő végpontokat vizsgálják:
`/heatingRequest`, `/logs`, `/configuration`, `/manualControlPumps` és `/reboot`.
Az újraindítási teszt csak hibás hitelesítést küld, ezért nem indítja újra az
eszközt. A konfiguráció POST feltöltését a teszt nem ismétli, mert az flashre
ír és megváltoztatná az eszköz konfigurációját.

A `/manualControlPumps` végpont `POST` kérést és JSON törzset használ. A JSON
objektum kulcsa az IO csatorna, értéke pedig a közvetlen PWM jel 0 és 100 között:

```bash
curl -X POST http://192.168.0.106/manualControlPumps \
  -H 'Content-Type: application/json' \
  -d '{"3":50,"4":0}'
```

Ez az IO3 csatornát 50%-os PWM értékre, az IO4 csatornát pedig 0%-ra állítja.

IO-nkénti manuális felülbírálás explicit formában:

```bash
curl -X POST http://192.168.0.106/manualControlPumps \
  -H 'Content-Type: application/json' \
  -d '{"3":{"manual":true,"value":50},"4":{"manual":false}}'
```

Ebben a példában az IO3-at a vezérlő nem írhatja felül, az IO4 pedig automatikus
vezérlés alatt marad. Egy IO visszaadásához az automatikus vezérlőnek a hozzá
tartozó `manual` értéket `false`-ra kell állítani.

A manuális felülbírálás és az automatikus visszavétel tesztje. A teszt külön
ellenőrzi az IO3-at `DISCHARGE`, valamint az IO4-et `CHARGE` állapotban. A
háttérben futó vezérlő írása manuális módban nem változtatja meg az értéket,
majd a flag kikapcsolása után a vezérlő visszaveszi az IO-t:

```bash
HEATING_CONTROLLER_URL=http://192.168.0.106 \
python -m pytest \
  tests/test_api.py::test_manual_control_pumps_override_each_io_independently \
  -m integration -vv -s
```

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

The history rotation test is intentionally slow because the firmware writes at
most one sample per second. Run it explicitly with:

```bash
HEATING_CONTROLLER_URL=http://192.168.0.116 \
STATE_HISTORY_ROTATION_TIMEOUT_SECONDS=1800 \
python -m pytest -m 'integration and slow' -vv -s tests/test_api.py::test_state_history_rotates_and_raw_files_are_downloadable
```

The timeout and sample interval can be changed with
`STATE_HISTORY_ROTATION_TIMEOUT_SECONDS` and
`STATE_HISTORY_SAMPLE_INTERVAL_SECONDS`.



HEATING_CONTROLLER_URL=http://192.168.0.116 \
python -m pytest -m integration -vv \
  --api-trace=reports/api-trace.json \
  --api-report=reports/api-report.md \
  --api-html-report=reports/api-report.html \
  --junitxml=reports/pytest-results.xml

### Pufferes kazán-előfűtés és -hűtés

A [test_boiler_buffer_operations.py](test_boiler_buffer_operations.py) mindkét
műveletet külön paraméterezett API-tesztekkel ellenőrzi:

- indítás, kazán-/vezérlőállapot és pumpaszerepek;
- megszakítás és a keringetőpumpák leállása;
- előfűtési célhőmérséklet, illetve kiegyenlítődő előremenő/visszatérő;
- második indítás és nem megfelelő pufferhőmérséklet elutasítása;
- túlmelegedés miatti megszakítás, a kazán hőelvezetésének megtartásával;
- hibás JSON-mezők elutasítása;
- konfigurált futásidő lejárata (`slow`).

A tesztek `TESTINGOVER_API` firmware-t igényelnek: szimulált hőmérsékleteket
küldenek a `/setTemp` végpontra és pumpákat indítanak. A `/configuration`
válaszából veszik a szenzorazonosítókat, az offseteket, a pumpák IO-ját és a
műveleti beállításokat. Nem töltenek fel konfigurációt és nem írnak flashre.
A `preheatFromPuffer` és `coolingFromPuffer` szakaszt a tesztelt eszközön
engedélyezni kell; az engedély nélküli művelet tesztje `SKIPPED` lesz.
A konfiguráció mezőit a [funkció leírása](../docs/boiler-buffer-operations.md)
részletezi. A kézi IO-felülírásokat a teszt előtt automatikus módba kell tenni.

Gyorsabb tesztek az időkorlát kivárása nélkül:

```bash
HEATING_CONTROLLER_URL=http://192.168.0.106 \
python -m pytest tests/test_boiler_buffer_operations.py \
  -m 'integration and not slow' -vv
```

Az időkorlátos tesztek a tényleges `maxDurationSeconds` időt várják ki
(alapértelmezésben műveletenként 300 másodperc):

```bash
HEATING_CONTROLLER_URL=http://192.168.0.106 \
python -m pytest tests/test_boiler_buffer_operations.py -m 'integration and slow' -vv
```

A futó állapot megfigyeléséhez a maximális futásidő legyen hosszabb a
`STATE_SETTLE_SECONDS + 2` értéknél. A tesztek hibánál is megkísérlik a művelet
megszakítását és a hideg alapállapot visszaállítását. A fűtési igényeket a
tesztek elején kikapcsolják. Az API-lépések és az állapot-/pumpaellenőrzések a
meglévő JSON, Markdown és HTML riportokba kerülnek.

### Tesztmód a hiányzó fizikai szenzor/PWM hardver megkerüléséhez

Az új firmware feltöltése után, `TESTINGOVER_API` környezetben:

```bash
curl -X POST http://192.168.0.106/boiler/test-mode \
  -H 'Content-Type: application/json' -d '{"enabled":true}'
```

Ez a kézi kapcsolás a pufferes kazántesztekhez már nem szükséges:
a tesztmodul automatikusan bekapcsolja a tesztmódot, és az összes teszt,
illetve a pumpák takarítása után visszaállítja az eredeti beállítást.
Ez sikertelen teszt esetén is megtörténik; ha eredetileg ki volt kapcsolva,
a végén újra kikapcsolódik. A kapcsolások az API-riportban is megjelennek.
Hiányzó tesztmód API vagy üzemi firmware esetén egyértelmű előfeltételhibát jelez.
A mód kézi
kikapcsolásához küldj `{"enabled":false}` törzset ugyanoda. Az aktuális
beállítás `GET /boiler/test-mode` kéréssel olvasható. Újraindítás után a mód
kikapcsolódik; üzemi firmware-ben a bekapcsolás `403` választ ad. A
hőmérsékleti és túlmelegedési ellenőrzések tesztmódban is működnek.

A [test_boiler_test_mode.py](test_boiler_test_mode.py) a kapcsolást,
az eredeti mód visszaállítását és a hibás kérések elutasítását ellenőrzi:

```bash
python -m pytest -k boiler_test_mode -m integration -vv
```

### Időkorlát utáni automatikus leállás

A `test_boiler_buffer_configured_timeout` külön előfűtésre és hűtésre is kivárja
a konfigurált futásidőt, a hőmérsékletek megváltoztatása nélkül. Ellenőrzi:

- a kazán `COOLD`, a puffer és a vezérlő `STANDBY` állapotba kerül;
- a keringetőpumpák leállnak, a leállítási listában szereplők is inaktívak;
- a firmware már lezárta a műveletet (utólagos `cancel` kérésre `409` érkezik);
- három további megfigyelés alatt sem indul újra automatikusan.

A `not slow` szűrő ezeket kihagyja. Csak a két időkorlátos teszt futtatása,
HTML riporttal:

```bash
HEATING_CONTROLLER_URL=http://192.168.0.116 \
python -m pytest -k boiler_buffer_configured_timeout -m integration -vv \
  --api-html-report=reports/boiler-buffer-timeout-report.html
```

300 másodperces beállítással a két teszt összesen legalább tíz percig tart.
A teszt szimulált hőmérsékleteket használ: az állapotokat és a pumpák vezérlését
ellenőrzi, a kazán tényleges fizikai kihűlését nem méri. Az időkorlát lejárata
lezárja a kézi műveletet; utána a normál automatikus vezérlés működik tovább.
