from unittest.mock import call, patch


def test_get_update(client):
    resp = client.get("/update")
    assert resp.status_code == 405


def test_update_post(client):
    with patch("cobra_lidar_api.api.subprocess.run") as mock_run:
        resp = client.post("/update")
        mock_run.assert_has_calls([
            call(["fw_setenv", "BOOT_MAIN_LEFT", "0"], check=True),
            call(["fw_setenv", "BOOT_RESCUE_LEFT", "5"], check=True),
            call(["sync"], check=True),
            call(["reboot"], check=True),
        ])

    assert resp.status_code == 200
    assert resp.json == "Switching to Update Mode."


def test_mapping_post(client):
    resp = client.post("/mapping")
    assert resp.status_code == 405


def test_logs_post(client):
    resp = client.post("/logs")
    assert resp.status_code == 405
