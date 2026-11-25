from __future__ import annotations

import os
import time

import pytest

from test_state_transitions import _assert_pump_outputs, _wait_for_states


pytestmark = pytest.mark.integration
ENDPOINT = "/boiler/buffer-operation"


@pytest.fixture(scope="module", autouse=True)
def enable_boiler_test_mode(api_session, api_base_url):
    """A modul tesztjeihez bekapcsolja, végül visszaállítja a tesztmódot."""
    url = f"{api_base_url}/boiler/test-mode"
    response = api_session.get(url, timeout=(2, 15))
    assert response.status_code == 200, (
        f"A tesztmód API nem érhető el; friss TESTINGOVER_API firmware szükséges: {response.text}"
    )
    initial = response.json()
    assert initial.get("available") is True, "A tesztek TESTINGOVER_API firmware-t igényelnek"
    assert isinstance(initial.get("enabled"), bool), initial

    def set_mode(enabled):
        result = api_session.post(url, json={"enabled": enabled}, timeout=(2, 15))
        passed = result.status_code == 200 and result.json().get("enabled") is enabled
        api_session.record_assertion(
            "Kazán tesztmód beállítása", enabled,
            result.json().get("enabled") if result.status_code == 200 else result.text,
            passed, trace_index=len(api_session.trace) - 1,
        )
        assert passed, result.text

    try:
        set_mode(True)
        yield
    finally:
        # A POST válaszának elvesztésekor is visszaállítjuk az eredeti módot.
        set_mode(initial["enabled"])


def _post_operation(session, base_url, boiler, action, expected_status=200):
    response = session.post(
        f"{base_url}{ENDPOINT}",
        json={"boiler": boiler, "action": action},
        timeout=(2, 15),
    )
    session.record_assertion(
        f"Kazán/puffer művelet: {action}, HTTP státusz",
        expected_status,
        response.status_code,
        response.status_code == expected_status,
        trace_index=len(session.trace) - 1,
    )
    assert response.status_code == expected_status, response.text
    payload = response.json()
    if expected_status == 200:
        assert payload == {
            "status": "cancelled" if action == "cancel" else "started",
            "boiler": boiler,
            "action": action,
        }
    else:
        assert isinstance(payload.get("error"), str) and payload["error"]
    return payload


def _temperatures(element, *, tour, retour, body):
    """A setTemp nyers értéket vár; a konfigurált offsetet kompenzáljuk."""
    by_position = {"tour": tour, "retour": retour, "body": body}
    return {
        sensor["id"]: float(by_position[sensor["position"]]) - sensor.get("offset", 0)
        for sensor in element.get("sensors", [])
    }


@pytest.fixture(params=["preheat", "cooling"])
def boiler_operation(request, api_session, api_base_url, set_sensor_values, read_state):
    """Élő konfigurációból választ kazánt/puffert, és mindig takarít maga után."""
    action = request.param
    key = "preheatFromPuffer" if action == "preheat" else "coolingFromPuffer"
    response = api_session.get(f"{api_base_url}/configuration", timeout=(2, 15))
    response.raise_for_status()
    system = response.json()["HeatingSystem"]
    boiler = next(
        (item for item in system["KAZAN"] if item.get(key, {}).get("enabled")),
        None,
    )
    if boiler is None:
        pytest.skip(f"Az eszköz konfigurációjában nincs engedélyezett {key}")
    config = boiler[key]
    buffer = next(
        (item for item in system.get("PUFER", []) if item["name"] == config["bufferName"]),
        None,
    )
    assert buffer is not None, "A művelethez konfigurált puffer nem található"
    # A normál teszteknek legyen idejük megfigyelni a futó állapotot is.
    settle = float(os.getenv("STATE_SETTLE_SECONDS", "5"))
    duration = config.get("maxDurationSeconds", 300)
    if duration <= settle + 2:
        pytest.skip("A futásidő legyen hosszabb a STATE_SETTLE_SECONDS + 2 értéknél")

    cold = min(boiler["activationThreshold"] - 5, config.get("targetTemp", boiler["retourTempProtValue"]) - 10)
    minimum_delta = config.get("minDeltaTemp", 2)
    warm = max(cold, config.get("targetTemp", boiler["retourTempProtValue"])) + minimum_delta + 5
    if cold < 10 or warm >= min(95, boiler["tourTempProtValue"]):
        pytest.skip("Ehhez a konfigurációhoz külön teszthőmérsékleteket kell választani")
    values = {
        sensor["id"]: 20.0 - sensor.get("offset", 0)
        for elements in system.values()
        for element in elements
        for sensor in element.get("sensors", [])
    }
    values.update(_temperatures(boiler, tour=cold, retour=cold - 5, body=cold))
    values.update(_temperatures(buffer, tour=warm, retour=cold - minimum_delta - 10, body=warm))
    expected_idle = {
        boiler["name"]: "COOLD", buffer["name"]: "STANDBY",
        "HeatingController": "STANDBY",
    }
    run_ids = tuple(
        str(pump["IOnumber"])
        for element in (boiler, buffer)
        for pump in element["pumps"]
        if not pump.get("dischargepump", False)
    )
    stopped_ids = tuple(str(io) for io in config.get("stopPumpIds", []))

    # Előző teszt félbemaradt művelete is megszakítható; a 409 itt azt jelenti,
    # hogy ezen a kazánon már nincs futó művelet.
    def cancel_if_running():
        result = api_session.post(
            f"{api_base_url}{ENDPOINT}",
            json={"boiler": boiler["name"], "action": "cancel"}, timeout=(2, 15),
        )
        assert result.status_code in (200, 409), result.text

    try:
        cancel_if_running()
        for radiator in system.get("RADIATOR", []):
            result = api_session.get(
                f"{api_base_url}/heatingRequest",
                params={"name": radiator["name"], "setTemp": "false"}, timeout=(2, 15),
            )
            result.raise_for_status()
        assert set_sensor_values(values)["status"] == "success"
        _wait_for_states(read_state, expected_idle)
        yield {
            "action": action, "boiler": boiler, "buffer": buffer, "config": config,
            "values": values, "idle": expected_idle, "run_ids": run_ids,
            "stopped_ids": stopped_ids,
            "running": {
                boiler["name"]: "PREHEAT_FROM_PUFFER" if action == "preheat" else "COOLING_FROM_PUFFER",
                buffer["name"]: "CHARGE",
                "HeatingController": "PREHEATING_BOILER" if action == "preheat" else "COOLING_BOILER",
            },
        }
    finally:
        cancel_if_running()
        assert set_sensor_values(values)["status"] == "success"
        _wait_for_states(read_state, expected_idle)


def _start(case, session, base_url, read_state):
    _post_operation(session, base_url, case["boiler"]["name"], case["action"])
    state = _wait_for_states(read_state, case["running"])
    _assert_pump_outputs(
        state, active=case["run_ids"], inactive=case["stopped_ids"], read_state=read_state,
    )
    expected_pwm = case["config"].get("bufferPumpSignal", case["config"].get("pumpSignal", 100))
    for pump in case["buffer"]["pumps"]:
        if pump.get("workingMode") == "PWM" and not pump.get("dischargepump", False):
            io = str(pump["IOnumber"])
            actual = state[io]
            passed = actual == pytest.approx(expected_pwm, abs=1)
            session.record_assertion(
                f"Puffer PWM beállított értéke: IO{io}", expected_pwm, actual, passed,
                element=f"IO{io}", trace_index=getattr(read_state, "_last_trace_index", None),
            )
            assert passed, f"IO{io}: elvárt {expected_pwm}%, tényleges {actual}%"


def test_boiler_buffer_start_and_cancel(boiler_operation, api_session, api_base_url, read_state):
    """Előfűtés/hűtés API-indítása, pumpaszerepei és megszakítása."""
    case = boiler_operation
    _start(case, api_session, api_base_url, read_state)
    _post_operation(api_session, api_base_url, case["boiler"]["name"], "cancel")
    state = _wait_for_states(read_state, case["idle"])
    _assert_pump_outputs(state, inactive=case["run_ids"], read_state=read_state)


def test_boiler_buffer_temperature_completion(
    boiler_operation, api_session, api_base_url, set_sensor_values, read_state,
):
    """Előfűtés célhőfokra, hűtés kiegyenlítődő előremenő/visszatérőre fejeződik be."""
    case = boiler_operation
    _start(case, api_session, api_base_url, read_state)
    if case["action"] == "preheat":
        target = case["config"].get("targetTemp", case["boiler"]["retourTempProtValue"])
        # A visszatérővédelem állapota maradjon stabilan megfigyelhető.
        if target > case["boiler"]["retourTempProtValue"] + 2:
            pytest.skip("A magas előfűtési cél miatt a RETOUR_PROTECTION csak átmeneti állapot")
        values = _temperatures(case["boiler"], tour=target + 1, retour=target, body=target)
        expected = {**case["idle"], case["boiler"]["name"]: "RETOUR_PROTECTION"}
    else:
        values = _temperatures(case["boiler"], tour=20, retour=20, body=20)
        expected = case["idle"]
    assert set_sensor_values(values)["status"] == "success"
    state = _wait_for_states(read_state, expected)
    _assert_pump_outputs(
        state, inactive=tuple(str(pump["IOnumber"]) for pump in case["buffer"]["pumps"]),
        read_state=read_state,
    )


def test_boiler_buffer_rejects_duplicate_request(boiler_operation, api_session, api_base_url, read_state):
    """Második indítás 409; az első művelet és pumpái tovább működnek."""
    case = boiler_operation
    _start(case, api_session, api_base_url, read_state)
    payload = _post_operation(api_session, api_base_url, case["boiler"]["name"], case["action"], 409)
    assert "already running" in payload["error"]
    state = _wait_for_states(read_state, case["running"])
    _assert_pump_outputs(state, active=case["run_ids"], inactive=case["stopped_ids"], read_state=read_state)


def test_boiler_buffer_rejects_unusable_buffer(
    boiler_operation, api_session, api_base_url, set_sensor_values, read_state,
):
    """Nem megfelelő pufferhőmérséklet mellett a kérés nem indít keringést."""
    case = boiler_operation
    buffer_temp = 5 if case["action"] == "preheat" else 60
    assert set_sensor_values(_temperatures(case["buffer"], tour=buffer_temp, retour=buffer_temp, body=buffer_temp))["status"] == "success"
    _wait_for_states(read_state, case["idle"])
    payload = _post_operation(api_session, api_base_url, case["boiler"]["name"], case["action"], 409)
    expected_error = "not warm enough" if case["action"] == "preheat" else "not cold enough"
    assert expected_error in payload["error"]
    state = _wait_for_states(read_state, case["idle"])
    _assert_pump_outputs(state, inactive=case["run_ids"], read_state=read_state)


def test_boiler_buffer_overheat_aborts_operation(
    boiler_operation, api_session, api_base_url, set_sensor_values, read_state,
):
    """Túlmelegedéskor megszűnik a kézi művelet, a kazán hőelvezetése aktív marad."""
    case = boiler_operation
    _start(case, api_session, api_base_url, read_state)
    hot = case["boiler"]["tourTempProtValue"] + 3
    if hot >= 100:
        pytest.skip("A túlmelegedési küszöb a tesztelhető szenzortartományon kívül esik")
    assert set_sensor_values(_temperatures(case["boiler"], tour=hot, retour=40, body=hot))["status"] == "success"
    state = _wait_for_states(read_state, {case["boiler"]["name"]: "OVERHEATED"})
    boiler_pumps = tuple(str(pump["IOnumber"]) for pump in case["boiler"]["pumps"] if not pump.get("dischargepump", False))
    _assert_pump_outputs(state, active=boiler_pumps, read_state=read_state)
    # A cancel 409 igazolja, hogy a vezérlő már felszabadította a műveletet.
    payload = _post_operation(api_session, api_base_url, case["boiler"]["name"], "cancel", 409)
    assert "No buffer operation" in payload["error"]


@pytest.mark.slow
def test_boiler_buffer_configured_timeout(boiler_operation, api_session, api_base_url, read_state):
    """Időtúllépés: COOLD/STANDBY, pumpák leállnak, a művelet nem indul újra."""
    case = boiler_operation
    started = time.monotonic()
    _start(case, api_session, api_base_url, read_state)
    duration = case["config"].get("maxDurationSeconds", 300)
    state = _wait_for_states(read_state, case["idle"], timeout=duration + 15)
    elapsed = time.monotonic() - started
    passed = elapsed >= duration - 1
    api_session.record_assertion(
        "A keringés nem állt le a konfigurált időkorlát előtt",
        f">= {duration - 1} másodperc", round(elapsed, 2), passed,
        trace_index=getattr(read_state, "_last_trace_index", None),
    )
    assert passed, f"Idő előtti leállás: {elapsed:.2f} s, konfigurált idő: {duration} s"
    inactive = tuple(dict.fromkeys(case["run_ids"] + case["stopped_ids"]))
    _assert_pump_outputs(state, inactive=inactive, read_state=read_state)

    # Nem a teszt állítja le: a firmware már magától lezárta a műveletet.
    payload = _post_operation(api_session, api_base_url, case["boiler"]["name"], "cancel", 409)
    assert "No buffer operation" in payload["error"]

    # Változatlan hőmérsékletekkel, új indítási kérés nélkül is leállva marad.
    for _ in range(3):
        time.sleep(max(float(os.getenv("STATE_SETTLE_SECONDS", "5")), 1))
        state = _wait_for_states(read_state, case["idle"], timeout=0)
        _assert_pump_outputs(state, inactive=inactive, read_state=read_state)


@pytest.mark.parametrize("payload", [
    {}, {"boiler": "Kazan"}, {"boiler": "", "action": "cooling"},
    {"boiler": "Kazan", "action": "invalid"}, {"boiler": "Kazan", "action": 1},
])
def test_boiler_buffer_rejects_invalid_json_fields(api_session, api_base_url, payload):
    """Hiányzó vagy hibás boiler/action mezőkre 400-as JSON hiba érkezik."""
    response = api_session.post(f"{api_base_url}{ENDPOINT}", json=payload, timeout=(2, 15))
    assert response.status_code == 400, response.text
    assert isinstance(response.json().get("error"), str)
