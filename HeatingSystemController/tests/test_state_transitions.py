from __future__ import annotations

import os
import time

import pytest


KAZAN = "Kazan"
PUFFER = "Puffer950l"
RADIATOR = "PadloKozos"

KAZAN_BODY = "28CC526B000000EE"
KAZAN_TOUR = "2881506B00000052"
KAZAN_RETOUR = "2832B643D42F2A25"
PUFFER_TOUR = "28000B96A00000029"
PUFFER_BODY = "2814576B00000059"
PUFFER_BODY_TOP = "286AD6940000003F"
PUFFER_BODY_MIDDLE = "286E068300000003"
PUFFER_BODY_BOTTOM = "287EDB9400000054"
PUFFER_BODY_TOP_LEVEL = "28C6D394000000CC"
PUFFER_RETOUR = "2826DE9400000009"
STATE_CHECK_INTERVAL_SECONDS = float(os.getenv("STATE_SETTLE_SECONDS", "3"))


def _assert_pump_outputs(
    state: dict,
    *,
    active: tuple[str, ...] = (),
    inactive: tuple[str, ...] = (),
    read_state=None,
) -> None:
    """Ellenőrzi a pumpa-IO-k tényleges kimeneti értékét a /state válaszban."""
    if read_state is not None and not _pump_outputs_are_valid(
        state, active=active, inactive=inactive
    ):
        state = read_state(retry=True)

    for io_number in active:
        output = state[io_number]
        passed = isinstance(output, (int, float)) and 0 < output <= 100
        _record_pump_assertion(read_state, io_number, "aktív (> 0)", output, passed)
        assert passed, f"IO{io_number} pumpa kimenete nem aktív: {output}"

    for io_number in inactive:
        output = state[io_number]
        passed = output == pytest.approx(0, abs=1.0)
        _record_pump_assertion(read_state, io_number, "inaktív (= 0)", output, passed)
        assert passed, (
            f"IO{io_number} pumpa kimenete nem nulla: {output}"
        )


def _pump_outputs_are_valid(
    state: dict,
    *,
    active: tuple[str, ...],
    inactive: tuple[str, ...],
) -> bool:
    return all(
        isinstance(state.get(io_number), (int, float))
        and 0 < state[io_number] <= 100
        for io_number in active
    ) and all(
        state.get(io_number) == pytest.approx(0, abs=1.0)
        for io_number in inactive
    )


def _record_pump_assertion(read_state, io_number, expected, actual, passed) -> None:
    session = getattr(read_state, "_trace_session", None)
    if session is not None:
        session.record_assertion(
            f"IO{io_number} pumpa kimenetének ellenőrzése",
            expected,
            actual,
            passed,
            element=f"IO{io_number}",
            trace_index=getattr(read_state, "_last_trace_index", None),
        )


def _puffer_body_values(value: float) -> dict[str, float]:
    return {
        PUFFER_BODY: value,
        PUFFER_BODY_TOP: value,
        PUFFER_BODY_MIDDLE: value,
        PUFFER_BODY_BOTTOM: value,
        PUFFER_BODY_TOP_LEVEL: value,
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
        timeout=(2, 15),
    )
    # The endpoint currently sends a fallback response after the success response.
    assert response.status_code in (200, 400)


def _wait_for_state(read_state, element: str, expected: str, timeout: float = 12) -> dict:
    """Wait for the expected state, polling the state API at fixed intervals."""
    deadline = time.monotonic() + timeout
    last_state = {}
    actual = "nincs adat"
    while True:
        last_state = read_state(retry=bool(last_state))
        actual = last_state.get(element, {}).get("state", "nincs adat")
        if actual == expected or time.monotonic() >= deadline:
            break
        time.sleep(min(STATE_CHECK_INTERVAL_SECONDS, max(0, deadline - time.monotonic())))

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


def _wait_for_states(
    read_state, expected_states: dict[str, str], timeout: float = 12
) -> dict:
    """Wait for all expected states using one shared response per poll."""
    deadline = time.monotonic() + timeout
    state = {}
    checks = []
    while True:
        state = read_state(retry=bool(state))
        checks = [
            {
                "element": element,
                "expected": expected,
                "actual": state.get(element, {}).get("state", "nincs adat"),
                "passed": state.get(element, {}).get("state") == expected,
            }
            for element, expected in expected_states.items()
        ]
        if all(check["passed"] for check in checks) or time.monotonic() >= deadline:
            break
        time.sleep(min(STATE_CHECK_INTERVAL_SECONDS, max(0, deadline - time.monotonic())))

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
    state = _wait_for_states(
        read_state,
        {
            KAZAN: "COOLD",
            PUFFER: "STANDBY",
            RADIATOR: "STANDBY",
            "HeatingController": "STANDBY",
        },
    )
    _assert_pump_outputs(
        state, inactive=("3", "4", "5", "6", "7"),
        read_state=read_state,
    )

    _set_temperatures(
        set_sensor_values,
        **{KAZAN_BODY: 45, KAZAN_TOUR: 45, KAZAN_RETOUR: 40},
    )
    state = _wait_for_states(
        read_state,
        {
            KAZAN: "RETOUR_PROTECTION",
            PUFFER: "STANDBY",
            RADIATOR: "STANDBY",
            "HeatingController": "STANDBY",
        },
    )
    _assert_pump_outputs(
        state, active=("7",), inactive=("3", "4", "5", "6"),
        read_state=read_state,
    )

    _set_temperatures(set_sensor_values, **{KAZAN_RETOUR: 50})
    state = _wait_for_states(
        read_state,
        {
            KAZAN: "HEATING",
            PUFFER: "CHARGE",
            RADIATOR: "STANDBY",
            "HeatingController": "CHARGING_BUFFER",
        },
    )
    _assert_pump_outputs(
        state, active=("4", "7"), inactive=("3", "5", "6"),
        read_state=read_state,
    )

    _set_temperatures(set_sensor_values, **{KAZAN_BODY: 85, KAZAN_TOUR: 85})
    state = _wait_for_states(
        read_state,
        {
            KAZAN: "OVERHEATED",
            PUFFER: "CHARGE",
            RADIATOR: "STANDBY",
            "HeatingController": "CHARGING_BUFFER",
        },
    )
    _assert_pump_outputs(
        state, active=("4", "7"), inactive=("3", "5", "6"),
        read_state=read_state,
    )

    _set_temperatures(set_sensor_values, **{KAZAN_BODY: 70, KAZAN_TOUR: 70})
    state = _wait_for_states(
        read_state,
        {
            KAZAN: "HEATING",
            PUFFER: "CHARGE",
            RADIATOR: "STANDBY",
            "HeatingController": "CHARGING_BUFFER",
        },
    )
    _assert_pump_outputs(
        state, active=("4", "7"), inactive=("3", "5", "6"),
        read_state=read_state,
    )


@pytest.mark.integration
def test_radiator_waiting_receiving_and_standby(
    set_sensor_values, read_state, api_session, api_base_url
):
    """Radiátor: STANDBY -> WAITING_FOR_HEATING -> RECEIVING_HEATING -> STANDBY."""
    _set_baseline_temperatures(set_sensor_values)
    _request_radiator_heat(api_session, api_base_url, False)
    state = _wait_for_states(
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
    state = _wait_for_states(
        read_state,
        {
            KAZAN: "HEATING",
            PUFFER: "CHARGE",
            RADIATOR: "RECEIVING_HEATING",
            "HeatingController": "CHARGING_BUFFER_AND_HEATING",
        },
    )
    _assert_pump_outputs(
        state, active=("5", "6", "4", "7"), inactive=("3",),
        read_state=read_state,
    )

    _request_radiator_heat(api_session, api_base_url, False)
    state = _wait_for_states(
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
    state = _wait_for_states(
        read_state,
        {
            KAZAN: "HEATING",
            PUFFER: "CHARGE",
            RADIATOR: "STANDBY",
            "HeatingController": "CHARGING_BUFFER",
        },
    )
    _assert_pump_outputs(
        state, active=("4",), inactive=("3",),
        read_state=read_state,
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
    state = _wait_for_states(
        read_state,
        {
            KAZAN: "COOLD",
            PUFFER: "DISCHARGE",
            RADIATOR: "RECEIVING_HEATING",
            "HeatingController": "DISCHARGING_BUFFER_AND_HEATING",
        },
    )
    _assert_pump_outputs(
        state, active=("3",), inactive=("4",),
        read_state=read_state,
    )

    _request_radiator_heat(api_session, api_base_url, False)
    state = _wait_for_states(
        read_state,
        {
            KAZAN: "COOLD",
            PUFFER: "STANDBY",
            RADIATOR: "STANDBY",
            "HeatingController": "STANDBY",
        },
    )
    _assert_pump_outputs(
        state, inactive=("3", "4"), read_state=read_state
    )
