"""
Tests that parameter getting/setting works correctly
"""
import logging
import pprint
import random
import requests
import time
from typing import Optional

import pytest

from tests.unit.api.helpers import random_lookup


log = logging.getLogger("cobra_system_control.cobra_log")
log.addHandler(logging.NullHandler())

n_repeats = 2


def check_response(r: requests.Response, status: int,
                   json: Optional[dict], exp: dict):
    __tracebackhide__ = False
    api_vals = requests.get(r.url)
    try:
        assert r.status_code == status, r
        if json is not None:
            assert r.json() == json
        assert api_vals.json() == exp, r
    except AssertionError:
        pprint.pprint(f'Error: expected = {exp}, error: json= {json}, '
                      f'response = {api_vals.json()}')
        print(r.status_code)
        if r.status_code == 200:
            print(r.json())
        else:
            print(r.text)
        print(api_vals.json())
        raise


@pytest.fixture
def swdl_url(hostname):
    yield f"{hostname}/scan_parameters"


@pytest.mark.repeat(n_repeats)
@pytest.mark.parametrize(
    'interleave, dsp_mode',
    [
        pytest.param(intl, dsp)
        for intl in [True, False]
        for dsp in [0, 1]
    ])
def test_scalar_to_scalar_setting(swdl_url, interleave, dsp_mode):
    new_settings = {p: [rand()] for p, rand in random_lookup.items()
                    if ((p != "interleave") or (p != "hdr_threshold") or (p != "dsp_mode"))}
    new_settings["interleave"] = False
    new_settings["hdr_threshold"] = random_lookup["hdr_threshold"]()
    new_settings["dsp_mode"] = dsp_mode
    # The hdr_inte_time_us values need to shorter than the inte_time_us values
    new_settings["hdr_inte_time_us"] = [x-1 if x>1 else x for x in new_settings["inte_time_us"]]

    data = new_settings
    setting = requests.get(swdl_url)
    pprint.pprint(f'Swdl =  {setting.json()}', indent=2)
    pprint.pprint(f'posting {data}', indent=2)
    ret = requests.post(url=swdl_url, json=data)
    time.sleep(0.2)
    setting = requests.get(swdl_url)
    pprint.pprint(f'Swdl =  {setting.json()}', indent=2)
    check_response(ret, 200, "SUCCESS", data)


@pytest.mark.repeat(n_repeats)
@pytest.mark.parametrize(
    'interleave, dsp_mode',
    [
        pytest.param(intl, dsp)
        for intl in [True, False]
        for dsp in [0, 1]
    ])
def test_array_setting(swdl_url, interleave, dsp_mode):
    new_settings = {p: [rand() for _ in range(2)] for p, rand
                    in random_lookup.items()
                    if ((p != "interleave") or (p != "hdr_threshold") or (p != "dsp_mode"))}
    new_settings["interleave"] = False
    new_settings["hdr_threshold"] = random_lookup["hdr_threshold"]()
    new_settings["dsp_mode"] = dsp_mode
    # The hdr_inte_time_us values need to shorter than the inte_time_us values
    new_settings["hdr_inte_time_us"] = [x-1 if x>1 else x for x in new_settings["inte_time_us"]]

    data = new_settings
    pprint.pprint(data, indent=2)

    setting = requests.get(swdl_url)
    pprint.pprint(f'Swdl =  {setting.json()}', indent=2)
    pprint.pprint(f'posting {data}', indent=2)
    ret = requests.post(url=swdl_url, json=data)
    time.sleep(0.2)
    setting = requests.get(swdl_url)
    pprint.pprint(f'Swdl =  {setting.json()}', indent=2)
    check_response(ret, 200, "SUCCESS", data)


@pytest.mark.repeat(n_repeats)
@pytest.mark.parametrize(
    'interleave, dsp_mode',
    [
        pytest.param(intl, dsp)
        for intl in [True, False]
        for dsp in [0, 1]
    ])
def test_invalid_array_to_array_setting(swdl_url, interleave, dsp_mode):
    old = requests.get(swdl_url)

    new_settings = {p: [rand() for _ in range(3)] for p, rand
                    in random_lookup.items()
                    if ((p != "interleave") or (p != "hdr_threshold") or (p != "dsp_mode"))}
    new_settings["interleave"] = False
    new_settings["hdr_threshold"] = random_lookup["hdr_threshold"]()
    new_settings["dsp_mode"] = dsp_mode
    # The hdr_inte_time_us values need to shorter than the inte_time_us values
    new_settings["hdr_inte_time_us"] = [x-1 if x>1 else x for x in new_settings["inte_time_us"]]

    # Delete an entry from one setting and replace it with a different
    # length entry.
    choices = list(random_lookup.keys())
    choices.remove("interleave")
    choices.remove("hdr_threshold")
    choices.remove("dsp_mode")
    key = random.choice(choices)
    new_settings[key] = [random_lookup[key]() for _ in range(2)]

    data = new_settings
    pprint.pprint(new_settings, indent=2)

    ret = requests.post(url=swdl_url, json=data)
    check_response(ret, 422, None, old.json())


@pytest.mark.repeat(n_repeats)
@pytest.mark.parametrize(
    'interleave, dsp_mode',
    [
        pytest.param(intl, dsp)
        for intl in [True, False]
        for dsp in [0, 1]
    ])
def test_missing_field(swdl_url, interleave, dsp_mode):
    old = requests.get(swdl_url)

    new_settings = {p: [rand() for _ in range(3)] for p, rand
                    in random_lookup.items()
                    if ((p != "interleave") or (p != "hdr_threshold") or (p != "dsp_mode"))}
    new_settings["interleave"] = False
    new_settings["hdr_threshold"] = random_lookup["hdr_threshold"]()
    new_settings["dsp_mode"] = dsp_mode
    # The hdr_inte_time_us values need to shorter than the inte_time_us values
    new_settings["hdr_inte_time_us"] = [x-1 if x>1 else x for x in new_settings["inte_time_us"]]

    # Delete an entry from one setting
    key = random.choice(list(random_lookup.keys()))
    new_settings.pop(key)

    data = new_settings
    ret = requests.post(url=swdl_url, json=data)
    check_response(ret, 422, None, old.json())


@pytest.mark.repeat(n_repeats)
def test_too_long_hdr_inte(swdl_url):
    old = requests.get(swdl_url)
    new_settings = {p: [rand() for _ in range(3)] for p, rand
                    in random_lookup.items()
                    if ((p != "interleave") or (p != "hdr_threshold") or (p != "dsp_mode"))}
    new_settings["interleave"] = False
    new_settings["hdr_threshold"] = random_lookup["hdr_threshold"]()
    new_settings["dsp_mode"] = 0
    # Make the hdr_inte_time_us values too long
    new_settings["hdr_inte_time_us"] = [x+1 for x in new_settings["inte_time_us"]]

    data = new_settings
    ret = requests.post(url=swdl_url, json=data)
    check_response(ret, 555, None, old.json())
