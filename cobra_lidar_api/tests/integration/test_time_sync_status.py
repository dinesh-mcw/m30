"""
Tests the ``system_time_synced`` resource

GET returns "yes" if the RTC clock has been synchronized
            "no" if the RTC clock has not been synchronized
"""
import pytest
import requests

pytestmark = pytest.mark.integration

@pytest.fixture
def psettings_url(hostname):
    yield f"{hostname}/time_sync_status"

def test_get(psettings_url):
    resp = requests.get(psettings_url)

    assert resp.status_code == 200
