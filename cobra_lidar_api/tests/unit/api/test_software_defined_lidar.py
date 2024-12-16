"""
Tests the ``SoftwareDefinedLidar`` resource

GET returns the full set of stored SWDL parameters per sensor head.

POST allows the user to set an arbitrary number of these parameters
for each sensor head. This will call ``apply_random_access_scan_settings``,
and successful applications will store the new values in ``MODES``.
"""
import dataclasses as dc

import pytest
from cobra_lidar_api import api
from cobra_lidar_api.schema import READ_ONLY_FIELDS
from tests.unit.api.helpers import check_modes_updated


ENDPOINT = "/scan_parameters"
value = {
    "angle_range": [[10, 20, 1]],
    "fps_multiple": [3],
    "laser_power_percent": [51],
    "inte_time_us": [15],
    "max_range_m": [32.4],
    "binning": [2],
    "snr_threshold": [2.2],
    "nn_level": [3],
    "user_tag": [1],
    "interleave": True,
    "hdr_threshold": 4095,
    "hdr_laser_power_percent": [40],
    "hdr_inte_time_us": [5],
    "frame_rate_hz": [900],
    "dsp_mode": 0,
}


@pytest.mark.xfail(reason='How to mock this? Works on real system')
def test_get(client):
    _ = client.post(ENDPOINT, json={ENDPOINT[1::]: value},
                    headers={'Content-Type': 'application/json'})
    resp = client.get(ENDPOINT)
    print(resp)
    expected = {}
    settings_dict = dc.asdict(api.CURRENT_SYSTEM_INFO)
    for field in READ_ONLY_FIELDS:
        settings_dict.pop(field)
    expected = settings_dict

    assert resp.status_code == 200
    assert resp.json == expected


@pytest.mark.parametrize("usr_json, mode_changes", [
    pytest.param(
        {
            "angle_range": [[10, 20, 1]],
            "fps_multiple": [3],
            "laser_power_percent": [40],
            "inte_time_us": [15],
            "max_range_m": [32.4],
            "binning": [2],
            "snr_threshold": [2.2],
            "nn_level": [3],
            "user_tag": [1],
            "interleave": True,
            "hdr_threshold": 4095,
            "hdr_laser_power_percent": [40],
            "hdr_inte_time_us": [5],
            "frame_rate_hz": [900],
            "dsp_mode": 0,
        },
        {
            "angle_range": [[10, 20, 1]],
            "fps_multiple": [3],
            "laser_power_percent": [40],
            "inte_time_us": [15],
            "max_range_m": [32.4],
            "binning": [2],
            "snr_threshold": [2.2],
            "nn_level": [3],
            "user_tag": [1],
            "interleave": True,
            "hdr_threshold": 4095,
            "hdr_laser_power_percent": [40],
            "hdr_inte_time_us": [5],
            "frame_rate_hz": [900],
            "dsp_mode": 0,
        },
    ),
    pytest.param(
        {
            "angle_range": [10, 20, 1],
            "fps_multiple": 3,
            "laser_power_percent": 40,
            "inte_time_us": 5,
            "max_range_m": 32.4,
            "binning": 2,
            "snr_threshold": 3.65,
            "nn_level": 3,
            "user_tag": 1,
            "interleave": True,
            "hdr_threshold": 1,
            "hdr_laser_power_percent": [45],
            "hdr_inte_time_us": 4,
            "frame_rate_hz": 900,
            "dsp_mode": 1,
        },
        {
            "angle_range": [[10, 20, 1]],
            "fps_multiple": [3],
            "laser_power_percent": [40],
            "inte_time_us": [5],
            "max_range_m": [32.4],
            "binning": [2],
            "snr_threshold": [3.6],
            "nn_level": [3],
            "user_tag": [1],
            "interleave": True,
            "hdr_threshold": 1,
            "hdr_laser_power_percent": [45],
            "hdr_inte_time_us": [4],
            "frame_rate_hz": [900],
            "dsp_mode": 1,
        },
    ),
    pytest.param(
        {
            "angle_range": [10, 20, 1],
            "fps_multiple": 3,
            "laser_power_percent": 40,
            "inte_time_us": 15.4,
            "max_range_m": 32.4,
            "binning": 2,
            "snr_threshold": 1.97,
            "nn_level": 3,
            "user_tag": 1,
            "interleave": True,
            "frame_rate_hz": 500.3,
            "hdr_threshold": 2001,
            "hdr_laser_power_percent": 50,
            "hdr_inte_time_us": 15,
            "dsp_mode": 0,
        },
        {
            "angle_range": [[10, 20, 1]],
            "fps_multiple": [3],
            "laser_power_percent": [40],
            "inte_time_us": [15],
            "max_range_m": [32.4],
            "binning": [2],
            "snr_threshold": [1.9],
            "nn_level": [3],
            "user_tag": [1],
            "interleave": True,
            "frame_rate_hz": [500],
            "dsp_mode": 0,
            "hdr_threshold": 2001,
            "hdr_laser_power_percent": [50],
            "hdr_inte_time_us": [15],
        },
    ),
    pytest.param(
        {
            "angle_range": [10, 20, 0.55],
            "fps_multiple": 3,
            "laser_power_percent": 40,
            "inte_time_us": 15.4,
            "max_range_m": 32.4,
            "binning": 2,
            "snr_threshold": 2.24,
            "nn_level": 3,
            "user_tag": 1,
            "interleave": True,
            "frame_rate_hz": 800,
            "dsp_mode": 0,
            "hdr_threshold": 2001,
            "hdr_laser_power_percent": [50],
            "hdr_inte_time_us": [15],
        },
        {
            "angle_range": [[10, 20, 0.5]],
            "fps_multiple": [3],
            "laser_power_percent": [40],
            "inte_time_us": [15],
            "max_range_m": [32.4],
            "binning": [2],
            "snr_threshold": [2.2],
            "nn_level": [3],
            "user_tag": [1],
            "interleave": True,
            "frame_rate_hz": [800],
            "hdr_threshold": 2001,
            "hdr_laser_power_percent": [50],
            "hdr_inte_time_us": [15],
            "dsp_mode": 0,
        },
    ),
])
def test_post_good(client, usr_json, mode_changes: dict):
    resp = client.post(ENDPOINT, json=usr_json)

    assert resp.status_code == 200
    assert resp.json == "SUCCESS"
    check_modes_updated(mode_changes)


@pytest.mark.parametrize("usr_json", [
    # Single or few parameters
    pytest.param(
         {"angle_range": [-10, 10, 1]},
         id='single parameter'
    ),
    pytest.param(
        {"angle_range": [-10, 10, 1],
         "binning": 2},
        id='not all parameters',
    ),
    pytest.param(
        {"inte_time_us": 15,
         "max_range_m": 1,
         "binning": 2,
         "snr_threshold": 2.22,
         "nn_level": 3,
         "user_tag": 1,
         "interleave": True,
         },
        id='not quite all',
    ),
])
def test_post_error_missing_data(client, usr_json):
    resp = client.post(ENDPOINT, json=usr_json)

    assert resp.status_code == 422
    assert 'Missing data for required field' in str(resp.json)


@pytest.mark.parametrize("usr_json", [
    pytest.param(
        {"angle_range": [[10, 20, 1]],
         "fps_multiple": [3],
         "laser_power_percent": [94.8],
         "inte_time_us": [15],
         "max_range_m": [32.4],
         "binning": [2],
         "snr_threshold": [2.22],
         "nn_level": [3],
         "user_tag": [1],
         "interleave": True,
         "frame_rate_hz": [900],
         "hdr_threshold": 4095,
         "hdr_laser_power_percent": [60.4],
         "hdr_inte_time_us": [15],
         "dsp_mode": 0,
         },
        id="float laser power percent input value",
    ),
    pytest.param(
        {"angle_range": [[10, 20, 1]],
         "fps_multiple": [3],
         "laser_power_percent": [94],
         "inte_time_us": [15.7],
         "max_range_m": [32.4],
         "binning": [2],
         "snr_threshold": [2.22],
         "nn_level": [3],
         "user_tag": [1],
         "interleave": True,
         "frame_rate_hz": [900],
         "hdr_threshold": 4095,
         "hdr_laser_power_percent": [60],
         "hdr_inte_time_us": [5.43],
         "dsp_mode": 0,
         },
        id="float inte time us input value",
    ),
    pytest.param(
        {"angle_range": [[10, 20, 1]],
         "fps_multiple": 3,
         "laser_power_percent": 40,
         "inte_time_us": [15],
         "max_range_m": [32.4],
         "binning": [2],
         "snr_threshold": [2.22],
         "nn_level": [3],
         "user_tag": [1],
         "interleave": True,
         "frame_rate_hz": 505.6,
         "hdr_threshold": 4095,
         "hdr_laser_power_percent": [50],
         "hdr_inte_time_us": [15],
         "dsp_mode": 0,
         },
        id="float frame rate input value",
    ),
])
def test_post_value_marshalling(client, usr_json):
    resp = client.post(ENDPOINT, json=usr_json)
    assert resp.status_code == 200


def test_get_opts(client):
    resp = client.get(f"{ENDPOINT}/opts")

    assert resp.status_code == 200
    assert resp.json == {
        "angle_range": {"angle_min": -45.0, "angle_max": 45.0,
                        "step_min": 0.3, "step_max": 10.0},
        "fps_multiple": {"low": 1, "high": 31},
        "laser_power_percent": {"low": 1, "high": 100},
        "inte_time_us": {"low": 1, "high": 20},
        "max_range_m": {"options": [25.2, 32.4]},
        "binning": {"options": [1, 2, 4]},
        "snr_threshold": {"low": 0.0, "high": 511.8},
        "nn_level": {"options": [0, 1, 2, 3, 4, 5]},
        "user_tag": {"low": 0, "high": 0xfff},
        "interleave": {"options": [True, False]},
        "frame_rate_hz": {"low": 300, "high": 960},
        "hdr_threshold": {"low": 0, "high": 4095},
        "hdr_laser_power_percent": {"low": 1, "high": 100},
        "hdr_inte_time_us": {"low": 1, "high": 20},
        "dsp_mode": {"options": [0, 1]},
    }
