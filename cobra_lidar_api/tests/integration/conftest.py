"""
Common configurations for integration testing.

Notable is the ``api_defaults`` fixture, which will automatically
apply default settings both before and after individual unit tests.
"""

import dataclasses as dc
import pprint

import pytest
import requests
from cobra_lidar_api.api import SystemInfo, defaults
from cobra_lidar_api.schema import READ_ONLY_FIELDS


def dc_to_api_dict(dc_):
    d = dc.asdict(dc_)
    for field in READ_ONLY_FIELDS:
        d.pop(field)
    return d


def apply_new_settings(url: str, settings: dict):
    last = {}
    post_data = settings
    comp_data = SystemInfo(**settings)
    watchdog, limit = 0, 10

    while last != comp_data:
        ret = requests.post(url=url, json=post_data)
        if ret.status_code == 200:
            vals = requests.get(url=url).json()
            last = SystemInfo(**vals)
        else:
            print(ret.status_code)
            print(ret.text)
        print('last', last)
        print('comp', comp_data)

        watchdog += 1
        if watchdog == limit:
            raise RuntimeError("Unable to set new defaults in time.")

    return last


@pytest.fixture(scope="session")
def hostname(request):
    yield f"http://{request.config.getoption('--hostname', None)}"


@pytest.fixture(autouse=True)
def api_defaults(hostname, request):
    url = f"{hostname}/scan_parameters"

    try:
        # Applies the input (indirect) settings to the sensor
        settings = {**defaults, **request.param}
    except (AttributeError, TypeError):
        # Use the defaults settings
        settings = defaults

    # Always apply the new settings - this ensures we start in defaults
    last = apply_new_settings(url, settings)

    # Return the new settings to the user for comparison
    ret = dc_to_api_dict(last)
    pprint.pprint(ret, indent=2)
    yield ret

    # Always go back to defaults after testing
    apply_new_settings(url, defaults)
