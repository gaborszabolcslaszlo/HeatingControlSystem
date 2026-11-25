# Kazán előfűtése és hűtése pufferrel

Mindkét művelet külső API-kérésre indul, automatikus indulás nincs. Egy időben egy kazánon futhat művelet. A vezérlő csak fatüzelésű, `COOLD` állapotú, az `activationThreshold` alatti előremenő hőmérsékletű kazán kérését fogadja el.

## Konfiguráció

A `HeatingSystem.KAZAN[]` kazánobjektumában két külön szakasz van: `preheatFromPuffer` és `coolingFromPuffer`. A mellékelt [konfiguráció](../data/config.json) mindkettőt tartalmazza.

| Mező | Jelentés | Alapérték |
| --- | --- | --- |
| `enabled` | A művelet kérhető-e API-n keresztül | `false` |
| `bufferName` | A használt puffer pontos neve | kötelező, ha engedélyezett |
| `maxDurationSeconds` | Maximális futásidő, 1–3600 másodperc | `300` |
| `minDeltaTemp` | Az induláshoz szükséges hőmérséklet-különbség °C-ban | `2` |
| `stopDeltaTemp` | A hőmérsékletek kiegyenlítődésének tűrése °C-ban | `1` |
| `targetTemp` | Előfűtésnél a kazán visszatérőjének célhőmérséklete | `retourTempProtValue` |
| `pumpSignal` | A keringetőpumpák vezérlése a művelet idején, 1–100% | `100` |
| `bufferPumpSignal` | Csak a kijelölt puffer PWM töltőpumpáinak jele, 1–100% | a `pumpSignal` értéke |
| `stopPumpIds` | Az ideiglenesen leállítandó pumpák `IOnumber` azonosítói | `[]` |

A `minDeltaTemp` legyen nagyobb a `stopDeltaTemp` értékénél. A keringetőpumpák nem szerepelhetnek a leállítási listában. Ismeretlen vagy több pumpához tartozó IO-azonosítót a konfigurációellenőrzés elutasít. Régi, e szakaszokat nem tartalmazó konfiguráció továbbra is használható; a műveletek ilyenkor le vannak tiltva.

Mindkét művelet a kazán és a kiválasztott puffer `dischargepump: false` pumpáit járatja, tehát a puffer töltőágát használja. A `pumpSignal` a művelet alatt felülírja e pumpák PID-jét, hogy a szükséges keringés létrejöjjön. A `stopPumpIds` listán kívüli pumpákat saját normál állapotgépük vezérli. A meglévő fűtési igények megmaradnak. Befejezés vagy megszakítás után az ideiglenes felülírások megszűnnek, a pumpákat ismét a normál vezérlés kezeli.

A mellékelt konfigurációban a kazán pumpája a 7-es IO, a puffer töltőágai a 9-es és 4-es IO-k; a 3-as, 5-ös, 6-os és 8-as IO a műveletek idejére leáll.

## Indítás és befejezés

**Hűtés:** a puffer visszatérőjének a kazán előremenőjénél legalább a beállított különbséggel hidegebbnek kell lennie. A kazán `COOLING_FROM_PUFFER`, a vezérlő `COOLING_BOILER` állapotba kerül. A kazán `COOLD` állapotba tér vissza, ha:

- lejár a maximális futásidő;
- az előremenő hőmérséklete a kazán visszatérőjének hőmérsékletére csökken, a `stopDeltaTemp` tűrésen belül;
- a puffer visszatérője már nem elég hideg a további hűtéshez.

**Előfűtés:** a puffer előremenőjének a kazán előremenőjénél és visszatérőjénél is a beállított különbséggel melegebbnek kell lennie. A kazán `PREHEAT_FROM_PUFFER`, a vezérlő `PREHEATING_BOILER` állapotba kerül. Ha a kazán visszatérője eléri a `targetTemp` értéket, a kazán `RETOUR_PROTECTION` állapotban folytatja a normál működést. Időtúllépés vagy elégtelen pufferhőmérséklet esetén `COOLD` állapotba tér vissza.

Az 5 perces alapérték a művelet futásidejére vonatkozik; nem a `COOLD` állapotba kerülés óta eltelt időre. A befejezési feltételeket a vezérlés következő ciklusa érvényesíti. A vezérlő a művelet után visszatér a normál automatikus működéshez.

Hiányzó/hibás hőmérséklet, túlmelegedés, aktív másik kazán vagy vezérlőhiba esetén az indítás elutasítható, a futó művelet megszakad. Túlmelegedett kazán `OVERHEATED` állapotba kerül; az ideiglenes pumpaleállítások megszűnnek. Kézi IO-felülírással egyidejű indítás nem engedélyezett. Először a kézi IO-vezérlést kell visszaadni automatikus módba, illetve futó műveletnél azt kell megszakítani a kézi pumpavezérlés előtt.

## API

`POST /boiler/buffer-operation`, `Content-Type: application/json`.

Hűtés indítása:

```json
{"boiler":"Kazan","action":"cooling"}
```

Előfűtés indítása:

```json
{"boiler":"Kazan","action":"preheat"}
```

Futó művelet megszakítása:

```json
{"boiler":"Kazan","action":"cancel"}
```

Sikeres indítás: HTTP `200`, `{"status":"started","boiler":"Kazan","action":"cooling"}`. Megszakításkor `status: "cancelled"`. Hibás kérés: `400`. Nem teljesülő működési feltételek: `409`, az `error` mezőben az indokkal. Nem elérhető vezérlés: `503`.

Az aktuális kazán- és vezérlőállapot a meglévő `/state` API-n jelenik meg; az új állapotok az állapotnaplóban is szerepelnek.

## Helyi ellenőrzés

A [natív tesztek](../tests/native/boiler_buffer_operations.cpp) a tényleges kazán-, puffer-, fogyasztó- és vezérlőosztályokat futtatják szimulált órával és IO-val. Ellenőrzik a hűtés hőmérséklet- és időalapú végét, az előfűtést, a pumpák felülírását és visszaadását, a hibás kéréseket, a szenzorhibát és a túlmelegedést.

```sh
bash tests/native/run_boiler_buffer_operations.sh
```

A futtatóhoz `g++` és a már letöltött `.pio/libdeps/esp32wroom/ArduinoJson/src` függőség szükséges. A bináris a `TMPDIR` alá kerül; a tesztek AddressSanitizer és UndefinedBehaviorSanitizer ellenőrzéssel futnak. Ptrace alatt futó sandboxban a LeakSanitizer külön kikapcsolható: `ASAN_OPTIONS=detect_leaks=0`.

A puffer PWM-pumpáinak külön sebessége a `bufferPumpSignal` mezővel adható meg,
mindkét művelet saját szakaszában. Például `"bufferPumpSignal": 45` esetén a
puffer PWM töltőpumpája 45%-on jár. A kazán pumpájára és a puffer ON/OFF
pumpáira továbbra is a `pumpSignal` vonatkozik. Ha az új mező hiányzik, a
korábbi működés marad: a puffer PWM-pumpája is a `pumpSignal` értékét kapja.

## API-ról kapcsolható tesztmód

`TESTINGOVER_API` firmware-ben a `POST /boiler/test-mode` végponttal kapcsolható:

```json
{"enabled":true}
```

Kikapcsolás: `{"enabled":false}`. Lekérdezés: `GET /boiler/test-mode`.
A válasz `available` és `enabled` mezői mutatják az elérhetőséget és az
aktuális módot. A beállítás csak RAM-ban él, újraindításkor kikapcsolódik.
Üzemi firmware-ben a bekapcsolás `403` választ ad.

A mód kizárólag a `HEALTH_SENSOR_UNAVAILABLE` és `HEALTH_PWM_UNAVAILABLE`
hardverállapotokat hagyja figyelmen kívül a kazán/puffer műveletek indításánál
és futtatásánál. Így szimulált hőmérsékletekkel, hiányzó fizikai szenzor vagy
PWM-eszköz mellett is tesztelhető az állapotgép. A `/health` hibajelzései
megmaradnak. A tesztmód válaszában a `healthConditions` az eredeti,
az `operationHealthConditions` a műveletekhez szűrt állapot.

A kazán tényleges hőmérséklet-ellenőrzése, a túlmelegedés kezelése, a
vezérlőhiba, a hideg kazán és a megfelelő pufferhőmérséklet követelménye,
a futásidő és a kézi IO-felülírások kizárása továbbra is érvényes.
Tesztmód kikapcsolása után a normál hardverállapot-ellenőrzések a következő
vezérlési ciklusban újra érvényesülnek.
