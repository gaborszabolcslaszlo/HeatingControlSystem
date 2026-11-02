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
def test_device_info_api_returns_system_information(api_session, api_base_url):
    """A készülék információs végpontjának ellenőrzése."""
    response = api_session.get(f"{api_base_url}/deviceInfo", timeout=5)
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
        timeout=5,
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
            timeout=5,
        ),
        "/heatingRequest",
    )


@pytest.mark.integration
def test_logs_api_stress(api_session, api_base_url):
    """A /logs végpont ismételt lekérése."""
    _assert_repeated_successful_requests(
        lambda: api_session.get(f"{api_base_url}/logs", timeout=5),
        "/logs",
    )


@pytest.mark.integration
def test_configuration_api_stress(api_session, api_base_url):
    """A /configuration végpont ismételt konfiguráció-lekérése."""
    _assert_repeated_successful_requests(
        lambda: api_session.get(f"{api_base_url}/configuration", timeout=5),
        "/configuration",
    )


@pytest.mark.integration
def test_device_info_api_stress(api_session, api_base_url):
    """A /deviceInfo végpont ismételt lekérése."""
    _assert_repeated_successful_requests(
        lambda: api_session.get(f"{api_base_url}/deviceInfo", timeout=5),
        "/deviceInfo",
    )


@pytest.mark.integration
def test_manual_control_pumps_api_stress(api_session, api_base_url):
    """A /manualControlPumps végpont ismételt érvényes JSON kérése."""
    _assert_repeated_successful_requests(
        lambda: api_session.get(
            f"{api_base_url}/manualControlPumps",
            json={},
            timeout=5,
        ),
        "/manualControlPumps",
    )


@pytest.mark.integration
def test_reboot_api_unauthorized_stress(api_session, api_base_url):
    """A /reboot végpont ismételt jogosulatlan kérése újraindítás nélkül."""
    iterations = _api_stress_iterations()
    for request_number in range(1, iterations + 1):
        response = api_session.get(
            f"{api_base_url}/reboot",
            params={"user": "invalid", "password": "invalid"},
            timeout=5,
        )
        assert response.status_code == 401, (
            f"/reboot nem 401 választ adott a(z) {request_number}. kérésnél: "
            f"HTTP {response.status_code}"
        )
