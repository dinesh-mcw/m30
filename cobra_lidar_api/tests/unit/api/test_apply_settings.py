"""
Tests the API ``apply_random_access_scan_settings`` method.
"""

import pytest
from cobra_lidar_api import api


@pytest.mark.parametrize("settings, kwargs_per_sh", [
    pytest.param(
        {},
        {},
        id="null_no_call",
    ),
    pytest.param(
        {**api.CURRENT_SYSTEM_INFO.settings_dict()},
        {**api.CURRENT_SYSTEM_INFO.settings_dict()},
        id="null_defaults_on_all",
    ),
    pytest.param(
        {"binning": 2},
        {**api.CURRENT_SYSTEM_INFO.settings_dict(), **{"binning": 2}},
        id="update_single_param_single_sh",
    ),
    pytest.param(
        {"snr_threshold": 2.46},
        {**api.CURRENT_SYSTEM_INFO.settings_dict(), **{"snr_threshold": 2.4}},
        id="update_single_same_param_multi_sh"
    ),
    pytest.param(
        {"user_tag": 2},
        {**api.CURRENT_SYSTEM_INFO.settings_dict(), **{"user_tag": 2}},
        id="update_single_different_param_multi_sh"
    ),
])
def test_apply_random_access_scan_settings_args(
        mock_cobra, settings, kwargs_per_sh: dict):
    """Tests that the call to the firmware
    ``apply_random_access_scan_settings``
    is performed with the correct set of kwargs.

    Args:
        mock_cobra: Mock cobra fixture with the following methods mocked:
            * ``state`` (property)
            * ``start``
            * ``stop``
            * ``apply_random_access_scan_settings``
        settings: Dictionary of new settings to apply.
        kwargs_per_sh: Dictionary containing the expected kwargs per-sensor head.
            The key is the sensor head ID (str), and the value is the kwargs dict.
            If a sensor head ID is omitted, then it is assumed not called.

    Notes:
        * The API method modifies the global ``CURRENT_SYSTEM_INFO``
          variable in the same module.
          As such, it is essential to reset that variable to its default value
          following testing. A fixture with ``autouse=True`` is suggested.
        * This test is intended to supplant this same test being repeated
          for all endpoints that modify settings (i.e., POST methods).
          For those tests, validate the internal changes to the
          CURRENT_SYSTEM_INFO object,
          or for consistency, check that the API method was called
          with the appropriate settings object. Or check the interior call
          anyway if you want to be really thorough (and redundant).
    """

    api.apply_random_access_scan_settings(settings, mock_cobra)

    sh = mock_cobra._sen  # noqa pylint: disable=protected-access
    ras_kwargs = {**api.defaults, **kwargs_per_sh}
    ras_kwargs["fov_angle_triplets"] = ras_kwargs.pop("angle_range")

    sh.apply_settings.assert_called_with(**sh.random_access_scan.appset_dict)


@pytest.mark.parametrize("binning, frame_rate_hz, inte_time, max_range_m", [
    pytest.param(bnx, frx, inx, mrix)
    for bnx in [0, 1, 4]
    for frx in [300, 500, 960]
    for inx in [5, 10, 20]
    for mrix in [25.2, 32.4]
])
def test_get_actual_frame_rate(binning, frame_rate_hz, inte_time, max_range_m):
    settings = {
        'angle_range': [[-45, 45, 1]],
        'binning': [binning],
        'frame_rate_hz': [frame_rate_hz],
        'inte_time_us': [inte_time],
        'max_range_m': [max_range_m],
    }
    _ = api.get_actual_frame_rate(settings)
