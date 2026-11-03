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


def _puffer_pid_values(kazan_retour: float) -> dict[str, float]:
    """Visszaadja a PID trendteszt egyetlen hőmérsékleti bemeneti állapotát."""
    return {
        **_configured_values(),
        "28CC526B000000000EE": 55.0,
        "2881506B00000000052": 55.0,
        "2832B643D42F2A25": kazan_retour,
        "28000B96A00000000029": 55.0,
        "2814576B00000000059": 20.0,
        "286AD6940000000003F": 20.0,
        "286E00683000000000003": 20.0,
        "287EDB9400000000054": 20.0,
        "28899B43D44400637": 20.0,
    }


def _puffer_discharge_pid_values() -> dict[str, float]:
    """Visszaadja a DISCHARGE állapothoz szükséges bemeneti állapotot."""
    return {
        **_configured_values(),
        "28CC526B000000000EE": 20.0,
        "2881506B00000000052": 20.0,
        # 45 °C alatt marad, hogy a kazán ne váltson HEATING állapotba.
        "2832B643D42F2A25": 20.0,
        "28000B96A00000000029": 55.0,
        "2814576B00000000059": 55.0,
        "286AD6940000000003F": 55.0,
        "286E00683000000000003": 55.0,
        "287EDB9400000000054": 55.0,
        "28899B43D44400637": 55.0,
    }


def _assert_puffer_pump_role(
    state: dict, active_channel: str, inactive_channel: str
) -> float:
    """Ellenőrzi, hogy csak a puffer állapotához tartozó pumpa aktív."""
    active_output = state[active_channel]
    inactive_output = state[inactive_channel]

    assert isinstance(active_output, (int, float))
    assert isinstance(inactive_output, (int, float))
    assert 0 <= active_output <= 100
    assert inactive_output == pytest.approx(0, abs=1.0)
    return float(active_output)


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
def test_puffer_pid_pumps_produce_pwm_output(
    set_sensor_values, read_state
):
    """A puffer két PID vezérelt pumpájának PWM kimenetét ellenőrzi.

    A konfigurációban a puffer PID pumpái a 3-as és 4-es IO csatornát
    használják. A /state válaszban kapott értéknek számszerű, 0..100 közötti
    PWM értéknek kell lennie.
    """
    values = {
        **_configured_values(),
        # Kazán hőt termel, a puffer alsó rétegei hidegek, ezért a vezérlőnek
        # töltési helyzetbe kell kerülnie, és a puffer pumpáit frissítenie kell.
        "28CC526B000000000EE": 55.0,
        "2881506B00000000052": 55.0,
        "2832B643D42F2A25": 50.0,
        "28000B96A00000000029": 55.0,
        "2814576B00000000059": 20.0,
        "286AD6940000000003F": 20.0,
        "286E00683000000000003": 20.0,
        "287EDB9400000000054": 20.0,
        "28899B43D44400637": 20.0,
    }

    result = set_sensor_values(values)
    assert result["status"] == "success"

    state = read_state()

    assert state["Puffer950l"]["state"] == "CHARGE"

    assert "3" in state
    assert "4" in state
    _assert_puffer_pump_role(state, active_channel="4", inactive_channel="3")


@pytest.mark.integration
def test_puffer_pid_output_follows_temperature_error(
    set_sensor_values, read_state, api_session, api_base_url
):
    """A PID kimeneti trendjét ellenőrzi a hiba előjelének változásakor.

    A puffer PID-je a kazán retour szenzorát használja, 60 °C-os setpointtal.
    A négy mérési pont szándékosan egy teszten belül készül, hogy az integráló
    tag állapota megmaradjon a lépések között.
    """
    # Először negatív hibával az alsó kimeneti tartomány felé visszük az
    # integrált. Így a pozitív hiba trendje a korábbi tesztállapottól kevésbé
    # függően látható.
    api_session.get(
        f"{api_base_url}/heatingRequest",
        params={"name": "PadloKozos", "setTemp": "false"},
        timeout=(2, 15),
    )

    set_sensor_values(_puffer_pid_values(kazan_retour=80.0))
    low_error_state = read_state()
    assert low_error_state["Puffer950l"]["state"] == "CHARGE"
    low_error_output = _assert_puffer_pump_role(
        low_error_state, active_channel="4", inactive_channel="3"
    )

    # 0 °C retour -> pozitív hiba a 60 °C setpointtal szemben. A CHARGE alatt
    # aktív 4-es pumpa kimenetének növekednie kell.
    set_sensor_values(_puffer_pid_values(kazan_retour=0.0))
    positive_error_state = read_state()
    assert positive_error_state["Puffer950l"]["state"] == "CHARGE"
    positive_error_output = _assert_puffer_pump_role(
        positive_error_state, active_channel="4", inactive_channel="3"
    )
    assert positive_error_output > low_error_output

    # 60 °C retour -> nulla hiba. Az integráló tag nem változik, ezért a PID
    # kimenete lényegében tartja az előző értéket.
    set_sensor_values(_puffer_pid_values(kazan_retour=60.0))
    zero_error_state = read_state()
    assert zero_error_state["Puffer950l"]["state"] == "CHARGE"
    zero_error_output = _assert_puffer_pump_role(
        zero_error_state, active_channel="4", inactive_channel="3"
    )
    assert zero_error_output == pytest.approx(positive_error_output, abs=1.0)

    # 100 °C retour -> negatív hiba. Az integráló tag csökken, ezért a
    # kimenetnek vissza kell fordulnia.
    set_sensor_values(_puffer_pid_values(kazan_retour=80.0))
    negative_error_state = read_state()
    assert negative_error_state["Puffer950l"]["state"] == "CHARGE"
    negative_error_output = _assert_puffer_pump_role(
        negative_error_state, active_channel="4", inactive_channel="3"
    )
    assert negative_error_output < zero_error_output


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
    _assert_puffer_pump_role(state, active_channel="3", inactive_channel="4")


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
        json={"2814576B00000000059": "not-a-temperature"},
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
        state = read_state()
        assert state["Puffer950l"]["state"] == "DISCHARGE"
        assert state["3"] == pytest.approx(37, abs=1)

        # A manuális mód kikapcsolása után a vezérlőnek át kell vennie az IO3-at.
        api_session.post(
            endpoint,
            json={"3": {"manual": False}},
            timeout=(2, 15),
        ).raise_for_status()
        time.sleep(1)
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
        state = read_state()
        assert state["Puffer950l"]["state"] == "CHARGE"
        assert state["4"] == pytest.approx(37, abs=1)

        api_session.post(
            endpoint,
            json={"4": {"manual": False}},
            timeout=(2, 15),
        ).raise_for_status()
        time.sleep(1)
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
