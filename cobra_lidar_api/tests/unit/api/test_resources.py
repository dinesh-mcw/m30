"""
Tests the ``StartScan`` and ``StopScan`` resources.

GET is disallowed (HTTP 405)

POST will start all available sensors if they are initially in the ``READY`` state.
"""
from unittest.mock import call, patch

import pytest
from cobra_system_control.sensor_head import State


@pytest.mark.parametrize("endpoint", [
    pytest.param('/start_scan'),
    pytest.param('/stop_scan'),
    pytest.param('/disable'),
    pytest.param('/restart'),
])
def test_get(client, endpoint):
    resp = client.get(endpoint)
    assert resp.status_code == 405


@pytest.mark.parametrize("endpoint, final_state", [
    pytest.param('/start_scan', State.SCANNING),
    pytest.param('/stop_scan', State.ENERGIZED),
])
def test_post(client, mock_remote_ctx, endpoint, final_state):
    resp = client.post(endpoint)
    assert resp.status_code == 200

    with mock_remote_ctx.remote() as c:
        sh = c.sen
        assert sh.state is final_state


def test_disable_post(client, mock_remote_ctx):
    with patch("cobra_lidar_api.api.subprocess.run") as mock_run:
        resp = client.post('/disable')
        mock_run.assert_has_calls([
            call(["sudo", "systemctl", "stop", "remote"], check=True)
        ])
    assert resp.status_code == 200
    assert resp.json == "Sensor head disabled and powered down.\n"

    with mock_remote_ctx.remote() as c:
        c.sen.stop.assert_called_once()


def test_state_get(client, cobra_states: dict):
    resp = client.get('/state')
    assert resp.status_code == 200
    # INITIALIZED state doesn't return
    assert resp.json == {"state": cobra_states.name}


def test_restart_post(client, mock_remote_ctx):
    with patch("cobra_lidar_api.api.subprocess.run") as mock_run:
        resp = client.post('/restart')
        mock_run.assert_has_calls([
            call(["sudo", "systemctl", "restart", "remote"], check=True),
        ])
    assert resp.status_code == 200
    assert resp.json == "Success"
