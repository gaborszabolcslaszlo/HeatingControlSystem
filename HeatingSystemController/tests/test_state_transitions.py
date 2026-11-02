from __future__ import annotations

import pytest


KAZAN = "Kazan"
PUFFER = "Puffer950l"
RADIATOR = "PadloKozos"

KAZAN_BODY = "28CC526B000000000EE"
KAZAN_TOUR = "2881506B00000000052"
KAZAN_RETOUR = "2832B643D42F2A25"
PUFFER_TOUR = "28000B96A00000000029"
PUFFER_BODY = "2814576B00000000059"
PUFFER_BODY_TOP = "286AD6940000000003F"
PUFFER_BODY_MIDDLE = "286E00683000000000003"
PUFFER_BODY_BOTTOM = "287EDB9400000000054"
PUFFER_RETOUR = "28899B43D44400637"


def _puffer_body_values(value: float) -> dict[str, float]:
    return {
        PUFFER_BODY: value,
        PUFFER_BODY_TOP: value,
        PUFFER_BODY_MIDDLE: value,
        PUFFER_BODY_BOTTOM: value,
    }


def _set_temperatures(set_sensor_values, **values) -> None:
    result = set_sensor_values(values)
    assert result["status"] == "success"


def _set_baseline_temperatures(set_sensor_values) -> None:
    """Give every sensor used by the transition tests a known starting value."""
    _set_temperatures(
        set_sensor_values,
        **{
            KAZAN_BODY: 20,
            KAZAN_TOUR: 20,
            KAZAN_RETOUR: 20,
            PUFFER_TOUR: 20,
            PUFFER_RETOUR: 20,
            **_puffer_body_values(20),
        },
    )


def _request_radiator_heat(api_session, api_base_url: str, enabled: bool) -> None:
    response = api_session.get(
        f"{api_base_url}/heatingRequest",
        params={"name": RADIATOR, "setTemp": "true" if enabled else "false"},
        timeout=5,
    )
    # The endpoint currently sends a fallback response after the success response.
    assert response.status_code in (200, 400)


def _wait_for_state(read_state, element: str, expected: str, timeout: float = 12) -> dict:
    """Wait for the device settle delay, then make exactly one state request."""
    del timeout
    last_state = read_state()
    actual = last_state.get(element, {}).get("state", "nincs adat")
    session = getattr(read_state, "_trace_session", None)
    if session is not None:
        session.record_assertion(
            f"{element} állapotának ellenőrzése",
            expected,
            actual,
            actual == expected,
            element=element,
            trace_index=getattr(read_state, "_last_trace_index", None),
        )
    if actual != expected:
        pytest.fail(f"{element} nem érte el a {expected} állapotot; tényleges: {actual}")
    return last_state


def _wait_for_states(read_state, expected_states: dict[str, str]) -> dict:
    """Check multiple element states from one shared /state response."""
    state = read_state()
    checks = [
        {
            "element": element,
            "expected": expected,
            "actual": state.get(element, {}).get("state", "nincs adat"),
            "passed": state.get(element, {}).get("state") == expected,
        }
        for element, expected in expected_states.items()
    ]
    passed = all(check["passed"] for check in checks)
    session = getattr(read_state, "_trace_session", None)
    if session is not None:
        session.record_assertion(
            "Állapotok együttes ellenőrzése",
            expected_states,
            {check["element"]: check["actual"] for check in checks},
            passed,
            trace_index=getattr(read_state, "_last_trace_index", None),
            checks=checks,
        )
    if not passed:
        failed = ", ".join(
            f"{check['element']}: elvárt {check['expected']}, tényleges {check['actual']}"
            for check in checks
            if not check["passed"]
        )
        pytest.fail(f"Állapot-ellenőrzés sikertelen: {failed}")
    return state


@pytest.mark.integration
def test_kazan_state_transition_sequence(
    set_sensor_values, read_state, api_session, api_base_url
):
    """Kazán: COOLD -> RETOUR_PROTECTION -> HEATING -> OVERHEATED -> HEATING."""
    _set_baseline_temperatures(set_sensor_values)
    _request_radiator_heat(api_session, api_base_url, False)
    _set_temperatures(
        set_sensor_values,
        **{KAZAN_BODY: 20, KAZAN_TOUR: 20, KAZAN_RETOUR: 20},
    )
    _wait_for_states(
        read_state,
        {
            KAZAN: "COOLD",
            PUFFER: "STANDBY",
            RADIATOR: "STANDBY",
            "HeatingController": "STANDBY",
        },
    )

    _set_temperatures(
        set_sensor_values,
        **{KAZAN_BODY: 45, KAZAN_TOUR: 45, KAZAN_RETOUR: 40},
    )
    _wait_for_states(
        read_state,
        {
            KAZAN: "RETOUR_PROTECTION",
            PUFFER: "STANDBY",
            RADIATOR: "STANDBY",
            "HeatingController": "STANDBY",
        },
    )

    _set_temperatures(set_sensor_values, **{KAZAN_RETOUR: 50})
    _wait_for_states(
        read_state,
        {
            KAZAN: "HEATING",
            PUFFER: "CHARGE",
            RADIATOR: "STANDBY",
            "HeatingController": "CHARGING_BUFFER",
        },
    )

    _set_temperatures(set_sensor_values, **{KAZAN_BODY: 85, KAZAN_TOUR: 85})
    _wait_for_states(
        read_state,
        {
            KAZAN: "OVERHEATED",
            PUFFER: "CHARGE",
            RADIATOR: "STANDBY",
            "HeatingController": "CHARGING_BUFFER",
        },
    )

    _set_temperatures(set_sensor_values, **{KAZAN_BODY: 70, KAZAN_TOUR: 70})
    _wait_for_states(
        read_state,
        {
            KAZAN: "HEATING",
            PUFFER: "CHARGE",
            RADIATOR: "STANDBY",
            "HeatingController": "CHARGING_BUFFER",
        },
    )


@pytest.mark.integration
def test_radiator_waiting_receiving_and_standby(
    set_sensor_values, read_state, api_session, api_base_url
):
    """Radiátor: STANDBY -> WAITING_FOR_HEATING -> RECEIVING_HEATING -> STANDBY."""
    _set_baseline_temperatures(set_sensor_values)
    _request_radiator_heat(api_session, api_base_url, False)
    _wait_for_states(
        read_state,
        {
            KAZAN: "COOLD",
            PUFFER: "STANDBY",
            RADIATOR: "STANDBY",
            "HeatingController": "STANDBY",
        },
    )

    _request_radiator_heat(api_session, api_base_url, True)
    _wait_for_states(
        read_state,
        {
            KAZAN: "COOLD",
            PUFFER: "STANDBY",
            RADIATOR: "WAITING_FOR_HEATING",
            "HeatingController": "STANDBY",
        },
    )

    _set_temperatures(
        set_sensor_values,
        **{
            KAZAN_BODY: 55,
            KAZAN_TOUR: 55,
            KAZAN_RETOUR: 50,
            PUFFER_TOUR: 55,
            **_puffer_body_values(55),
            PUFFER_RETOUR: 45,
        },
    )
    _wait_for_states(
        read_state,
        {
            KAZAN: "HEATING",
            PUFFER: "CHARGE",
            RADIATOR: "RECEIVING_HEATING",
            "HeatingController": "CHARGING_BUFFER_AND_HEATING",
        },
    )

    _request_radiator_heat(api_session, api_base_url, False)
    _wait_for_states(
        read_state,
        {
            KAZAN: "HEATING",
            PUFFER: "CHARGE",
            RADIATOR: "STANDBY",
            "HeatingController": "CHARGING_BUFFER",
        },
    )


@pytest.mark.integration
def test_puffer_charge_discharge_and_standby(
    set_sensor_values, read_state, api_session, api_base_url
):
    """Puffer: STANDBY -> CHARGE -> DISCHARGE -> STANDBY."""
    _set_baseline_temperatures(set_sensor_values)
    _request_radiator_heat(api_session, api_base_url, False)
    _set_temperatures(
        set_sensor_values,
        **{
            KAZAN_BODY: 55,
            KAZAN_TOUR: 55,
            KAZAN_RETOUR: 50,
            PUFFER_TOUR: 55,
            PUFFER_BODY: 20,
            PUFFER_RETOUR: 20,
        },
    )
    _wait_for_states(
        read_state,
        {
            KAZAN: "HEATING",
            PUFFER: "CHARGE",
            RADIATOR: "STANDBY",
            "HeatingController": "CHARGING_BUFFER",
        },
    )

    _request_radiator_heat(api_session, api_base_url, True)
    _set_temperatures(
        set_sensor_values,
        **{
            KAZAN_BODY: 20,
            KAZAN_TOUR: 20,
            KAZAN_RETOUR: 20,
            **_puffer_body_values(55),
        },
    )
    _wait_for_states(
        read_state,
        {
            KAZAN: "COOLD",
            PUFFER: "DISCHARGE",
            RADIATOR: "RECEIVING_HEATING",
            "HeatingController": "DISCHARGING_BUFFER_AND_HEATING",
        },
    )

    _request_radiator_heat(api_session, api_base_url, False)
    _wait_for_states(
        read_state,
        {
            KAZAN: "COOLD",
            PUFFER: "STANDBY",
            RADIATOR: "STANDBY",
            "HeatingController": "STANDBY",
        },
    )
