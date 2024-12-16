"""
Helper methods for testing to reduce duplicated code in unit tests.
"""
import random
from typing import Callable, Dict

import numpy as np

from cobra_lidar_api import api
from cobra_lidar_api.schema import VALID_PARAMETERS, precision_floor, floor10


def check_modes_updated(changes: dict):
    """Helper method to validate that ``MODES`` was appropriately updated
    with the expected values input.

    The expected input should be in the same format as the ``SystemInfo``
    dictionary form (``{<sh_id>: {*params: <assoc_value>}}``)
    """
    __tracebackhide__ = True

    # Get current mode settings stored in the API
    api_modes = api.CURRENT_SYSTEM_INFO.settings_dict()

    # Build the expected settings kwargs
    # Merge the expected changes with the defaults (previous)
    updated = {**api.defaults, **changes}
    expected = api.SystemInfo(**updated).settings_dict()
    assert api_modes.keys() == expected.keys()
    for (k0, v0), (k1, v1) in zip(sorted(api_modes.items()), sorted(expected.items())):
        assert k0 == k1, f'k0={k0}, k1={k1}'
        assert v0 == v1, f'v0={v0}, v1={v1}'


def rand_angle_range(step: bool = False) -> list:
    # Get two unique angles without duplication
    fov = random.sample(list(np.arange(-45, 45.1, 0.1)), k=2)
    step = random.sample(list(np.arange(0.3, 10, 0.1)), k=1)
    fov = [precision_floor(x, 1) for x in fov]
    step = [precision_floor(x, 1) for x in step]
    fov.extend(step)
    return fov


def rand_fps_multiple() -> int:
    bounds = VALID_PARAMETERS["fps_multiple"]
    # While the register can handle v. large values, the firmware cannot
    return random.randint(bounds["min"], 2)


def rand_laser_power_percent() -> int:
    bounds = VALID_PARAMETERS["laser_power_percent"]
    return random.randint(bounds["min"], bounds["max"])


def rand_inte_time_us():
    bounds = VALID_PARAMETERS["inte_time_us"]
    return random.randint(bounds["min"], bounds["max"])


def rand_max_range_m():
    """This parameter cannot be mixed between multiple Virtual Sensors
    so only one value is returned.
    """
    return 25.2


def rand_binning():
    return random.choice(VALID_PARAMETERS["binning"]["choices"])


def rand_snr_threshold():
    bounds = VALID_PARAMETERS["snr_threshold"]
    return precision_floor(random.uniform(bounds["min"], bounds["max"]), 1)


def rand_nn_level():
    return random.choice(VALID_PARAMETERS["nn_level"]["choices"])


def rand_user_tag():
    return random.randint(0, 7)


def rand_interleave():
    return random.choice(VALID_PARAMETERS["interleave"]["choices"])


def rand_dsp_mode():
    return random.choice(VALID_PARAMETERS["dsp_mode"]["choices"])


def rand_frame_rate():
    bounds = VALID_PARAMETERS["frame_rate_hz"]
    return floor10(random.randint(bounds["min"], bounds["max"]))


def rand_hdr_threshold():
    bounds = VALID_PARAMETERS["hdr_threshold"]
    return random.randint(bounds["min"], bounds["max"])


def rand_hdr_laser_power_percent() -> int:
    bounds = VALID_PARAMETERS["hdr_laser_power_percent"]
    return random.randint(bounds["min"], bounds["max"])


def rand_hdr_inte_time_us():
    bounds = VALID_PARAMETERS["hdr_inte_time_us"]
    return random.randint(bounds["min"], bounds["max"])


random_lookup: Dict[str, Callable] = {
    "angle_range": rand_angle_range,
    "fps_multiple": rand_fps_multiple,
    "laser_power_percent": rand_laser_power_percent,
    "inte_time_us": rand_inte_time_us,
    "max_range_m": rand_max_range_m,
    "binning": rand_binning,
    "snr_threshold": rand_snr_threshold,
    "nn_level": rand_nn_level,
    "user_tag": rand_user_tag,
    "interleave": rand_interleave,
    "frame_rate_hz": rand_frame_rate,
    "hdr_threshold": rand_hdr_threshold,
    "hdr_laser_power_percent": rand_hdr_laser_power_percent,
    "hdr_inte_time_us": rand_hdr_inte_time_us,
    "dsp_mode": rand_dsp_mode,
}
