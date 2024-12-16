"""
Tests the schema helper methods found in ``cobra_lidar_api/api_schema/helpers.py``
"""
import pytest
from cobra_lidar_api.schema import load_usr_json_to_system_schema


@pytest.mark.parametrize("usr_in, expected", [
    pytest.param(
        {
            "angle_range": [[-42, 42, 1]],
            "fps_multiple": [1],
            "laser_power_percent": [50],
            "inte_time_us": [15],
            "max_range_m": [25.2],
            "binning": [4],
            "snr_threshold": [4.2],
            "nn_level": [3],
            "user_tag": [-1],
            "interleave": False,
            "frame_rate_hz": [960],
            "hdr_threshold": 4095,
            "hdr_laser_power_percent": [40],
            "hdr_inte_time_us": [5],
            "dsp_mode": 0,
        },
        {
            "angle_range": [[-42, 42, 1]],
            "fps_multiple": [1],
            "laser_power_percent": [50],
            "inte_time_us": [15],
            "max_range_m": [25.2],
            "binning": [4],
            "snr_threshold": [4.2],
            "nn_level": [3],
            "user_tag": [-1],
            "interleave": False,
            "frame_rate_hz": [960],
            "hdr_threshold": 4095,
            "hdr_laser_power_percent": [40],
            "hdr_inte_time_us": [5],
            "dsp_mode": 0,
        },
        id="all_fields_input==output",
    ),
    pytest.param(
        {
            "angle_range": [-42, 42, 1],
            "fps_multiple": 1,
            "laser_power_percent": 55,
            "inte_time_us": 15,
            "max_range_m": 25.2,
            "binning": 2,
            "snr_threshold": 4.2,
            "nn_level": 3,
            "user_tag": -1,
            "interleave": False,
            "frame_rate_hz": 850,
            "hdr_threshold": 4095,
            "hdr_laser_power_percent": [40],
            "hdr_inte_time_us": [5],
            "dsp_mode": 1,
        },
        {
            "angle_range": [[-42, 42, 1]],
            "fps_multiple": [1],
            "laser_power_percent": [55],
            "inte_time_us": [15],
            "max_range_m": [25.2],
            "binning": [2],
            "snr_threshold": [4.2],
            "nn_level": [3],
            "user_tag": [-1],
            "interleave": False,
            "frame_rate_hz": [850],
            "hdr_threshold": 4095,
            "hdr_laser_power_percent": [40],
            "hdr_inte_time_us": [5],
            "dsp_mode": 1,
        },
        id="all_fields_input==output",
    ),
])
def test_load_usr_json_to_system_schema(app, usr_in: dict, expected: dict):
    with app.test_request_context(json=usr_in):
        result = load_usr_json_to_system_schema()
    assert result == expected
