from __future__ import annotations

import pytest


pytestmark = pytest.mark.integration


def test_boiler_test_mode_toggle_and_restore(api_session, api_base_url):
    """Tesztfirmware-ben kapcsolható a mód; üzemi firmware-ben a bekapcsolás 403."""
    url = f"{api_base_url}/boiler/test-mode"
    response = api_session.get(url, timeout=(2, 15))
    response.raise_for_status()
    initial = response.json()
    assert isinstance(initial["available"], bool)
    assert isinstance(initial["enabled"], bool)
    assert isinstance(initial["healthConditions"], int)
    assert isinstance(initial["operationHealthConditions"], int)
    try:
        response = api_session.post(url, json={"enabled": True}, timeout=(2, 15))
        if initial["available"]:
            assert response.status_code == 200, response.text
            status = response.json()
            assert status["enabled"] is True
            assert status["operationHealthConditions"] == status["healthConditions"] & ~3
        else:
            assert response.status_code == 403, response.text
            assert isinstance(response.json().get("error"), str)
        response = api_session.post(url, json={"enabled": False}, timeout=(2, 15))
        assert response.status_code == 200, response.text
        status = response.json()
        assert status["enabled"] is False
        assert status["operationHealthConditions"] == status["healthConditions"]
    finally:
        response = api_session.post(url, json={"enabled": initial["enabled"]}, timeout=(2, 15))
        assert response.status_code == 200, response.text


@pytest.mark.parametrize("payload", [{}, {"enabled": "true"}, {"enabled": 1}, []])
def test_boiler_test_mode_rejects_invalid_enabled(api_session, api_base_url, payload):
    """A tesztmód API csak valódi JSON boolean enabled mezőt fogad el."""
    url = f"{api_base_url}/boiler/test-mode"
    before = api_session.get(url, timeout=(2, 15))
    before.raise_for_status()
    response = api_session.post(url, json=payload, timeout=(2, 15))
    assert response.status_code == 400, response.text
    after = api_session.get(url, timeout=(2, 15))
    after.raise_for_status()
    assert after.json()["enabled"] == before.json()["enabled"]
