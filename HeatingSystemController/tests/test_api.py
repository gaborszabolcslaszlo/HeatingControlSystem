from __future__ import annotations

import json
import os
import time
from pathlib import Path

import pytest


CONFIG_PATH = Path(__file__).resolve().parents[1] / "config" / "config.json"


def _sensor_ids(element_type: str) -> list[str]:
    config = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
    return [
        sensor["id"]
        for element in config["HeatingSystem"][element_type]
        for sensor in element.get("sensors", [])
    ]


def _puffer_values() -> dict[str, float]:
    config = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
    puffer = config["HeatingSystem"]["PUFER"][0]
    values = {}
    for sensor in puffer["sensors"]:
        values[sensor["id"]] = 55.0 if sensor["position"] == "body" else 45.0
    return values


def _configured_values() -> dict[str, float]:
    config = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
    values = {}
    for element_type in ("KAZAN", "PUFER"):
        for element in config["HeatingSystem"][element_type]:
            for sensor in element.get("sensors", []):
                values[sensor["id"]] = 55.0
    return values


def _state_history_files(api_session, api_base_url: str) -> dict:
    response = api_session.get(f"{api_base_url}/state-history/files", timeout=(2, 15))
    response.raise_for_status()
    payload = response.json()
    assert isinstance(payload, dict)
    assert isinstance(payload.get("files"), list)
    # Régebbi firmware-ek a SPIFFS fájlnevet kezdő '/' nélkül adták vissza.
    # A raw API viszont abszolút fájlnevet vár, ezért a teszt egységesítse.
    for entry in payload["files"]:
        if isinstance(entry, dict) and isinstance(entry.get("name"), str):
            if not entry["name"].startswith("/"):
                entry["name"] = "/" + entry["name"]
    return payload


def _assert_raw_history_file(response, expected_size: int | None = None) -> None:
    response.raise_for_status()
    assert response.headers.get("Content-Type", "").startswith(
        "application/octet-stream"
    )
    content = response.content
    assert len(content) > 8, "A letöltött history fájl üres vagy túl rövid"
    if expected_size is not None:
        assert len(content) == expected_size
    # Új fájlok HLOG fejléccel kezdődnek, régebbi firmware fájljai viszont
    # közvetlenül schema rekorddal indulhatnak.
    offset = 8 if content[:4] == b"HLOG" else 0
    assert content[:4] == b"HLOG" or content[0] == 1
    record_types = []
    while offset + 5 <= len(content):
        record_type = content[offset]
        frame_length = int.from_bytes(content[offset + 1 : offset + 3], "little")
        frame_end = offset + 5 + frame_length
        assert frame_end <= len(content), "Csonka history rekord"
        record_types.append(record_type)
        offset = frame_end
    assert 1 in record_types, "Hiányzik a history schema rekord"
    assert 2 in record_types, "Hiányzik a history minta rekord"


def _configured_pump_io_numbers() -> set[str]:
    config = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
    return {
        str(pump["IOnumber"])
        for element_type in ("KAZAN", "PUFER", "RADIATOR")
        for element in config["HeatingSystem"][element_type]
        for pump in element.get("pumps", [])
    }


def _puffer_pid_values(kazan_retour: float) -> dict[str, float]:
    """Visszaadja a PID trendteszt egyetlen hőmérsékleti bemeneti állapotát."""
    return {
        **_configured_values(),
        "28CC526B000000EE": 55.0,
        "2881506B00000052": 55.0,
        "2832B643D42F2A25": kazan_retour,
        "28000B96A00000029": 55.0,
        "2814576B00000059": 20.0,
        "286AD6940000003F": 20.0,
        "286E068300000003": 20.0,
        "287EDB9400000054": 20.0,
        "2826DE9400000009": 20.0,
    }


def _puffer_discharge_pid_values() -> dict[str, float]:
    """Visszaadja a DISCHARGE állapothoz szükséges bemeneti állapotot."""
    return {
        **_configured_values(),
        "28CC526B000000EE": 20.0,
        "2881506B00000052": 20.0,
        # 45 °C alatt marad, hogy a kazán ne váltson HEATING állapotba.
        "2832B643D42F2A25": 20.0,
        "28000B96A00000029": 55.0,
        "2814576B00000059": 55.0,
        "286AD6940000003F": 55.0,
        "286E068300000003": 55.0,
        "287EDB9400000054": 55.0,
        "2826DE9400000009": 55.0,
    }


def _assert_puffer_pump_role(
    state: dict,
    active_channel: str,
    inactive_channel: str,
    read_state=None,
) -> float:
    """Ellenőrzi, hogy csak a puffer állapotához tartozó pumpa aktív."""

    if read_state is not None and not _puffer_pump_role_is_valid(
        state, active_channel, inactive_channel
    ):
        state = _read_state_after_failed_check(read_state)

    active_output = state[active_channel]
    inactive_output = state[inactive_channel]

    active_passed = isinstance(active_output, (int, float)) and 0 <= active_output <= 100
    inactive_passed = (
        isinstance(inactive_output, (int, float))
        and inactive_output == pytest.approx(0, abs=1.0)
    )
    session = getattr(read_state, "_trace_session", None)
    if session is not None:
        trace_index = getattr(read_state, "_last_trace_index", None)
        session.record_assertion(
            f"IO{active_channel} pumpa kimenetének ellenőrzése",
            "aktív (0..100)",
            active_output,
            active_passed,
            element=f"IO{active_channel}",
            trace_index=trace_index,
        )
        session.record_assertion(
            f"IO{inactive_channel} pumpa kimenetének ellenőrzése",
            "inaktív (= 0)",
            inactive_output,
            inactive_passed,
            element=f"IO{inactive_channel}",
            trace_index=trace_index,
        )

    assert active_passed
    assert inactive_passed
    return float(active_output)


def _puffer_pump_role_is_valid(
    state: dict, active_channel: str, inactive_channel: str
) -> bool:
    active_output = state.get(active_channel)
    inactive_output = state.get(inactive_channel)
    return (
        isinstance(active_output, (int, float))
        and isinstance(inactive_output, (int, float))
        and 0 <= active_output <= 100
        and inactive_output == pytest.approx(0, abs=1.0)
    )


def _read_state_after_failed_check(read_state) -> dict:
    """Wait once, then request /state again after an unexpected result."""
    time.sleep(float(os.getenv("STATE_SETTLE_SECONDS", "3")))
    return read_state(retry=True)


def _read_state_with_expected_value(read_state, element: str, expected: str) -> dict:
    state = read_state()
    if state.get(element, {}).get("state") != expected:
        state = _read_state_after_failed_check(read_state)
    return state


def _api_stress_iterations() -> int:
    value = os.getenv(
        "API_STRESS_ITERATIONS",
        os.getenv("STATE_STRESS_ITERATIONS", "100"),
    )
    iterations = int(value)
    if iterations < 1:
        pytest.fail("API_STRESS_ITERATIONS értéke legalább 1 legyen")
    return iterations


def _assert_repeated_successful_requests(
    request_factory,
    endpoint_name: str,
    iterations: int | None = None,
) -> None:
    request_count = iterations if iterations is not None else _api_stress_iterations()
    for request_number in range(1, request_count + 1):
        response = request_factory()
        assert response.status_code == 200, (
            f"{endpoint_name} sikertelen a(z) {request_number}. lekérésnél: "
            f"HTTP {response.status_code}"
        )


@pytest.mark.integration
def test_set_temperature_api_accepts_configured_sensor_values(set_sensor_values):
    """A puffer szenzorértékeinek elküldése és sikeres API-válasz ellenőrzése."""
    result = set_sensor_values(_puffer_values())
    assert result["status"] == "success"


@pytest.mark.integration
def test_state_api_returns_sensor_and_element_state(set_sensor_values, read_state):
    """A beállított értékek és a puffer állapotának lekérdezése."""
    values = _puffer_values()
    set_sensor_values(values)
    state = read_state()

    for sensor_id, expected in values.items():
        assert state[sensor_id] == pytest.approx(expected)

    assert "Puffer950l" in state
    assert "state" in state["Puffer950l"]
    assert "iA" in state["Puffer950l"]


@pytest.mark.integration
def test_state_api_contains_all_configured_sensor_ids(set_sensor_values, read_state):
    """Az összes konfigurációban szereplő szenzorazonosító ellenőrzése."""
    set_sensor_values(_configured_values())
    state = read_state()
    configured_ids = set(_sensor_ids("KAZAN")) | set(_sensor_ids("PUFER"))
    assert configured_ids <= state.keys()


@pytest.mark.integration
def test_state_api_contains_all_configured_pump_outputs(
    set_sensor_values, read_state
):
    """A /state minden konfigurált pumpa kimeneti IO-ját visszaadja."""
    set_sensor_values(_configured_values())
    state = read_state()
    pump_io_numbers = _configured_pump_io_numbers()

    assert pump_io_numbers <= state.keys()
    for io_number in pump_io_numbers:
        assert isinstance(state[io_number], (int, float))
        assert 0 <= state[io_number] <= 100


@pytest.mark.integration
@pytest.mark.slow
def test_state_history_rotates_and_raw_files_are_downloadable(
    set_sensor_values, api_session, api_base_url
):
    """Sok mintát ír, megvárja a history-szegmens rotációját és letölti a fájlokat.

    A firmware legalább egy másodpercenként ír mintát, ezért ez a teszt
    szándékosan lassú. A timeout és az egyes minták közötti várakozás
    környezeti változóval állítható.
    """
    timeout_seconds = float(
        os.getenv("STATE_HISTORY_ROTATION_TIMEOUT_SECONDS", "1800")
    )
    sample_interval = max(
        1.05, float(os.getenv("STATE_HISTORY_SAMPLE_INTERVAL_SECONDS", "1.2"))
    )
    if timeout_seconds <= 0:
        pytest.fail("STATE_HISTORY_ROTATION_TIMEOUT_SECONDS legyen pozitív")

    before = _state_history_files(api_session, api_base_url)
    date = before["currentDate"]
    prefix = f"/state_{date}"
    before_files = {
        entry["name"]: int(entry["size"])
        for entry in before["files"]
        if isinstance(entry, dict)
        and str(entry.get("name", "")).startswith(prefix)
        and str(entry.get("name", "")).endswith(".bin")
    }
    target_file_count = len(before_files) + 1

    deadline = time.monotonic() + timeout_seconds
    iteration = 0
    latest_files = before_files
    configured = _configured_values()
    while time.monotonic() < deadline:
        # Minden konfigurált kazán- és puffer-szenzor változzon, így minden
        # mintavételi ciklusban tényleges history rekord keletkezik.
        value = 20.0 + float(iteration % 70)
        set_sensor_values({sensor_id: value for sensor_id in configured})
        iteration += 1
        time.sleep(sample_interval)

        if iteration % 5 != 0:
            continue
        listing = _state_history_files(api_session, api_base_url)
        latest_files = {
            entry["name"]: int(entry["size"])
            for entry in listing["files"]
            if isinstance(entry, dict)
            and str(entry.get("name", "")).startswith(prefix)
            and str(entry.get("name", "")).endswith(".bin")
        }
        if len(latest_files) >= target_file_count:
            # A frissen megnyitott szegmensbe is írjunk több mintát, ne csak a
            # rotáció által létrehozott schema rekordot ellenőrizzük.
            for extra in range(3):
                extra_value = 20.0 + float((iteration + extra + 1) % 70)
                set_sensor_values(
                    {sensor_id: extra_value for sensor_id in configured}
                )
                time.sleep(sample_interval)
            refreshed = _state_history_files(api_session, api_base_url)
            latest_files = {
                entry["name"]: int(entry["size"])
                for entry in refreshed["files"]
                if isinstance(entry, dict)
                and str(entry.get("name", "")).startswith(prefix)
                and str(entry.get("name", "")).endswith(".bin")
            }
            break

    assert len(latest_files) == target_file_count, (
        f"Hibás history-rotáció: pontosan egy új szegmensnek kell létrejönnie; "
        f"elvárt fájlszám: {target_file_count}, tényleges: {len(latest_files)}, "
        f"fájlok: {latest_files}"
    )
    # A két legfrissebb szegmenst a raw API-n keresztül is ellenőrizzük.
    for file_name in sorted(latest_files)[-2:]:
        assert latest_files[file_name] > 8, f"Üres history fájl: {file_name}"
        response = api_session.get(
            f"{api_base_url}/api/state-history/raw",
            params={"name": file_name},
            timeout=(2, 120),
        )
        _assert_raw_history_file(response, expected_size=latest_files[file_name])


@pytest.mark.integration
def test_puffer_pid_pumps_produce_pwm_output(
    set_sensor_values, read_state
):
    """A puffer két PID vezérelt pumpájának PWM kimenetét ellenőrzi.

    A konfigurációban a puffer PID pumpái a 8-as és 9-es IO csatornát
    használják. Töltéskor az IO9 PWM kimenetét, kisütéskor az IO8 kimenetét
    kell ellenőrizni.
    """
    values = {
        **_configured_values(),
        # Kazán hőt termel, a puffer alsó rétegei hidegek, ezért a vezérlőnek
        # töltési helyzetbe kell kerülnie, és a puffer pumpáit frissítenie kell.
        "28CC526B000000EE": 55.0,
        "2881506B00000052": 55.0,
        "2832B643D42F2A25": 50.0,
        "28000B96A00000029": 55.0,
        "2814576B00000059": 20.0,
        "286AD6940000003F": 20.0,
        "286E068300000003": 20.0,
        "287EDB9400000054": 20.0,
        "2826DE9400000009": 20.0,
    }

    result = set_sensor_values(values)
    assert result["status"] == "success"

    state = read_state()

    assert state["Puffer950l"]["state"] == "CHARGE"

    assert isinstance(state["9"], (int, float))
    assert 0 <= state["9"] <= 100
    assert state["8"] == pytest.approx(0, abs=1.0)


@pytest.mark.integration
def test_puffer_pid_output_follows_temperature_error(
    set_sensor_values, read_state, api_session, api_base_url
):
    """A PID kimeneti trendjét ellenőrzi a hiba előjelének változásakor.

    A töltési PID a kazán retour szenzorát használja, és az IO9 PWM kimenetet
    ellenőrizzük; az IO4 csak az ON/OFF töltőpumpa kimenete.
    """
    # A konfiguráció negatív P/I együtthatói miatt a magasabb retour-hőmérséklet
    # nagyobb töltési PWM-et eredményez.
    api_session.get(
        f"{api_base_url}/heatingRequest",
        params={"name": "PadloKozos", "setTemp": "false"},
        timeout=(2, 15),
    )

    set_sensor_values(_puffer_pid_values(kazan_retour=80.0))
    high_temperature_state = read_state()
    assert high_temperature_state["Puffer950l"]["state"] == "CHARGE"
    high_temperature_output = high_temperature_state["9"]
    assert isinstance(high_temperature_output, (int, float))
    assert 0 <= high_temperature_output <= 100

    # 0 °C retour -> a negatív P tag miatt kisebb PWM kimenet.
    set_sensor_values(_puffer_pid_values(kazan_retour=0.0))
    positive_error_state = read_state()
    assert positive_error_state["Puffer950l"]["state"] == "CHARGE"
    low_temperature_output = positive_error_state["9"]
    assert isinstance(low_temperature_output, (int, float))
    assert 0 <= low_temperature_output <= 100
    assert low_temperature_output < high_temperature_output

    # A konfiguráció szerinti setpoint környékén is érvényes PWM-et kell kapnunk.
    set_sensor_values(_puffer_pid_values(kazan_retour=60.0))
    zero_error_state = read_state()
    assert zero_error_state["Puffer950l"]["state"] == "CHARGE"
    zero_error_output = zero_error_state["9"]
    assert isinstance(zero_error_output, (int, float))
    assert 0 <= zero_error_output <= 100

    # Ismételt magas retour-hőmérsékleten is érvényes PWM-et kell kapnunk.
    set_sensor_values(_puffer_pid_values(kazan_retour=80.0))
    negative_error_state = read_state()
    assert negative_error_state["Puffer950l"]["state"] == "CHARGE"
    negative_error_output = negative_error_state["9"]
    assert isinstance(negative_error_output, (int, float))
    assert 0 <= negative_error_output <= 100


@pytest.mark.integration
def test_puffer_discharge_uses_only_discharge_pid_pump(
    set_sensor_values, read_state, api_session, api_base_url
):
    """A DISCHARGE pumpa kizárólagos működését ellenőrzi."""
    api_session.get(
        f"{api_base_url}/heatingRequest",
        params={"name": "PadloKozos", "setTemp": "true"},
        timeout=(2, 15),
    )

    result = set_sensor_values(_puffer_discharge_pid_values())
    assert result["status"] == "success"

    state = read_state()
    assert state["Puffer950l"]["state"] == "DISCHARGE"
    assert "3" in state
    assert "4" in state
    _assert_puffer_pump_role(
        state, active_channel="3", inactive_channel="4", read_state=read_state
    )


@pytest.mark.integration
def test_device_info_api_returns_system_information(api_session, api_base_url):
    """A készülék információs végpontjának ellenőrzése."""
    response = api_session.get(f"{api_base_url}/deviceInfo", timeout=(2, 15))
    response.raise_for_status()
    info = response.json()

    assert "uptimeMs" in info
    assert "wifi" in info
    assert "chip" in info
    assert "heap" in info


@pytest.mark.integration
def test_invalid_temperature_payload_is_rejected(api_session, api_base_url):
    """Érvénytelen hőmérsékletadat elutasításának ellenőrzése."""
    response = api_session.post(
        f"{api_base_url}/setTemp",
        json={"2814576B00000059": "not-a-temperature"},
        timeout=(2, 15),
    )
    assert response.status_code == 400


@pytest.mark.integration
def test_state_api_stress_after_set_temperature(
    set_sensor_values, read_state
):
    """Egy setTemp hívás után ismételten lekéri a state API választ.

    A ciklusszám és a lekérések közötti várakozás környezeti változóval állítható:

    - ``STATE_STRESS_ITERATIONS``: alapértelmezésben 100 lekérés
    - ``STATE_STRESS_DELAY_SECONDS``: alapértelmezésben 0 másodperc
    """
    iterations = int(
        os.getenv(
            "API_STRESS_ITERATIONS",
            os.getenv("STATE_STRESS_ITERATIONS", "100"),
        )
    )
    delay_seconds = float(os.getenv("STATE_STRESS_DELAY_SECONDS", "0"))

    if iterations < 1:
        pytest.fail("STATE_STRESS_ITERATIONS értéke legalább 1 legyen")
    if delay_seconds < 0:
        pytest.fail("STATE_STRESS_DELAY_SECONDS nem lehet negatív")

    result = set_sensor_values(_configured_values())
    assert result["status"] == "success"

    for request_number in range(1, iterations + 1):
        # A read_state fixture az első /state előtt kivárja a setTemp utáni
        # beállási időt, és a kérést a részletes riportban is rögzíti.
        state = read_state()
        assert isinstance(state, dict), (
            f"/state válasza nem JSON objektum a(z) {request_number}. lekérésnél"
        )

        if delay_seconds:
            time.sleep(delay_seconds)


@pytest.mark.integration
def test_heating_request_api_stress(api_session, api_base_url):
    """A /heatingRequest végpont ismételt, érvényes lekérése."""
    _assert_repeated_successful_requests(
        lambda: api_session.get(
            f"{api_base_url}/heatingRequest",
            params={"name": "PadloKozos", "setTemp": "false"},
            timeout=(2, 15),
        ),
        "/heatingRequest",
    )


@pytest.mark.integration
def test_logs_api_stress(api_session, api_base_url):
    """A /logs végpont ismételt lekérése."""
    _assert_repeated_successful_requests(
        lambda: api_session.get(f"{api_base_url}/logs", timeout=(2, 15)),
        "/logs",
    )


@pytest.mark.integration
def test_configuration_api_stress(api_session, api_base_url):
    """A /configuration végpont ismételt konfiguráció-lekérése."""
    _assert_repeated_successful_requests(
        lambda: api_session.get(f"{api_base_url}/configuration", timeout=(2, 15)),
        "/configuration",
    )


@pytest.mark.integration
def test_device_info_api_stress(api_session, api_base_url):
    """A /deviceInfo végpont ismételt lekérése."""
    _assert_repeated_successful_requests(
        lambda: api_session.get(f"{api_base_url}/deviceInfo", timeout=(2, 15)),
        "/deviceInfo",
    )


@pytest.mark.integration
def test_manual_control_pumps_api_stress(api_session, api_base_url):
    """A /manualControlPumps végpont ismételt JSON kérése."""
    _assert_repeated_successful_requests(
        lambda: api_session.post(
            f"{api_base_url}/manualControlPumps",
            json={"3": {"manual": False}, "4": {"manual": False}},
            timeout=(2, 15),
        ),
        "/manualControlPumps",
    )


@pytest.mark.integration
def test_manual_control_pumps_override_each_io_independently(
    api_session, api_base_url, set_sensor_values, read_state
):
    """A háttérvezérlő csak a manuális mód kikapcsolása után írhatja az IO-t."""
    endpoint = f"{api_base_url}/manualControlPumps"
    try:
        api_session.get(
            f"{api_base_url}/heatingRequest",
            params={"name": "PadloKozos", "setTemp": "true"},
            timeout=(2, 15),
        )

        # DISCHARGE alatt a vezérlőnek az IO3-at kellene írnia.
        response = api_session.post(
            endpoint,
            json={"3": {"manual": True, "value": 37}},
            timeout=(2, 15),
        )
        response.raise_for_status()

        set_sensor_values(_puffer_discharge_pid_values())
        state = _read_state_with_expected_value(read_state, "Puffer950l", "DISCHARGE")
        assert state["3"] == pytest.approx(37, abs=1)

        # A manuális mód kikapcsolása után a vezérlőnek át kell vennie az IO3-at.
        api_session.post(
            endpoint,
            json={"3": {"manual": False}},
            timeout=(2, 15),
        ).raise_for_status()
        state = read_state()
        assert state["3"] != pytest.approx(37, abs=1)

        # CHARGE alatt ugyanezt ellenőrizzük az IO4 pumpával.
        api_session.get(
            f"{api_base_url}/heatingRequest",
            params={"name": "PadloKozos", "setTemp": "false"},
            timeout=(2, 15),
        )
        api_session.post(
            endpoint,
            json={"4": {"manual": True, "value": 37}},
            timeout=(2, 15),
        ).raise_for_status()

        set_sensor_values(_puffer_pid_values(kazan_retour=50.0))
        state = _read_state_with_expected_value(read_state, "Puffer950l", "CHARGE")
        assert state["4"] == pytest.approx(37, abs=1)

        api_session.post(
            endpoint,
            json={"4": {"manual": False}},
            timeout=(2, 15),
        ).raise_for_status()
        state = read_state()
        assert state["4"] != pytest.approx(37, abs=1)
    finally:
        api_session.post(
            endpoint,
            json={"3": {"manual": False}, "4": {"manual": False}},
            timeout=(2, 15),
        ).raise_for_status()


@pytest.mark.integration
def test_reboot_api_unauthorized_stress(api_session, api_base_url):
    """A /reboot végpont ismételt jogosulatlan kérése újraindítás nélkül."""
    iterations = _api_stress_iterations()
    for request_number in range(1, iterations + 1):
        response = api_session.get(
            f"{api_base_url}/reboot",
            params={"user": "invalid", "password": "invalid"},
            timeout=(2, 15),
        )
        assert response.status_code == 401, (
            f"/reboot nem 401 választ adott a(z) {request_number}. kérésnél: "
            f"HTTP {response.status_code}"
        )
