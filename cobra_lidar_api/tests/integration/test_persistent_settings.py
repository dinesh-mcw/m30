"""
Tests the ``persistent_settings`` resource

GET returns the persistent settings and their current values

POST allows you to set one or more of the persistent settings
"""
import pytest
import requests

pytestmark = pytest.mark.integration

@pytest.fixture
def psettings_url(hostname):
    yield f"{hostname}/persistent_settings"

def test_get(psettings_url):
    resp = requests.get(psettings_url)

    assert resp.status_code == 200

def test_post(psettings_url):
    resp = requests.post(psettings_url, json={ "frontend_options": "-s none" })
    assert resp.status_code == 200

    resp = requests.post(psettings_url, json={ "bad_setting": 1 })
    assert resp.status_code == 400
