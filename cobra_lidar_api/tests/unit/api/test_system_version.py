"""
Tests the ``SystemVersion`` resource

GET returns the system version info per sensor head.

POST is disallowed (HTTP 405)
"""


def test_get(client, mock_cobra):
    resp = client.get("/system_version")

    assert resp.status_code == 200
    assert resp.json == {"system_version": mock_cobra._system_version}


def test_post(client):
    resp = client.post("/system_version")
    assert resp.status_code == 405
