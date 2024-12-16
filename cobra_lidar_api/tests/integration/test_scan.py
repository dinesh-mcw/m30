"""
Tests that endpoints controlling scan behavior work.
"""
import pytest
import requests

pytestmark = pytest.mark.integration


def get_states(host):
    return requests.get(f"{host}/state").json()


class TestStartScan:
    def test_from_ready(self, hostname):
        ret = requests.post(f"{hostname}/start_scan")

        assert ret.status_code == 200

        new_states = {"state": "SCANNING"}
        assert get_states(hostname) == new_states

    def test_from_scanning(self, hostname):
        requests.post(f"{hostname}/start_scan")
        ret = requests.post(f"{hostname}/start_scan")

        assert ret.status_code == 555

        new_states = {"state": "SCANNING"}
        assert get_states(hostname) == new_states


class TestStopScan:
    def test_from_ready(self, hostname):
        ret = requests.post(f"{hostname}/stop_scan")

        assert ret.status_code == 200

        new_states = {"state": "ENERGIZED"}
        assert get_states(hostname) == new_states

    def test_from_scanning(self, hostname):
        requests.post(f"{hostname}/start_scan")
        ret = requests.post(f"{hostname}/stop_scan")

        assert ret.status_code == 200

        new_states = {"state": "ENERGIZED"}
        assert get_states(hostname) == new_states
