"""
Common unit test setup for all unit tests under ``api/``
"""

import dataclasses as dc
import logging
from copy import deepcopy
from unittest.mock import MagicMock
import pytest
from cobra_lidar_api import api
from cobra_system_control.cobra import Cobra
from cobra_system_control.laser import LaserPowerPercentMappedOvFactory
from cobra_system_control.metasurface import LcmAssembly
from cobra_system_control.pixel_mapping import DEFAULT_PIXEL_MAPPING
from cobra_system_control.roi_mapping import RoiMapping
from cobra_system_control.state import State

log = logging.getLogger(__name__)
log.addHandler(logging.NullHandler())


@pytest.fixture(autouse=True)
def modes(request):
    """Automatic fixture to ensure the ``CURRENT_SYSTEM_INFO``
    global variable in the API
    remains unchanged between tests.
    """

    default = deepcopy(api.CURRENT_SYSTEM_INFO)

    # Apply changes if we have them
    try:
        api.CURRENT_SYSTEM_INFO = dc.replace(api.CURRENT_SYSTEM_INFO, **request. param)
    except AttributeError:
        pass
    except TypeError:
        pass
    logging.debug(f"Starting with CURRENT_SYSTEM_INFO f{api.CURRENT_SYSTEM_INFO}")

    yield api.CURRENT_SYSTEM_INFO
    api.CURRENT_SYSTEM_INFO = default


@pytest.fixture(scope="session")
def roi_mapping() -> RoiMapping:
    return RoiMapping(
        a2a_coefficients=[0, 0, 0, 0],
        pixel_mapping=DEFAULT_PIXEL_MAPPING,
        lcm_assembly=LcmAssembly(),
    )


@pytest.fixture
def cobra_states(request) -> dict:
    """Fixture to set the starting states of sensor head objects.
    Set this via indirect parameterization.
    """
    yield State.ENERGIZED


@pytest.fixture
def mock_cobra(roi_mapping, cobra_states) -> Cobra:
    cobra = Cobra(whoami='m30', board_type='nxp')

    # Set full system mocks
    cobra.connect = MagicMock()
    cobra.disconnect = MagicMock()
    cobra.setup = MagicMock()
    cobra.enable = MagicMock()

    def set_state(sh_, state_):
        # Don't set the property, since it will apply to _all_ sensor heads at once.
        sh_._state = state_

    def debug_read_fields(field):
        if field == "git_sha":
            return int("0xdeadbeef", base=16)
        else:
            raise ValueError("Unsupported fields for debug read_fields mock")

    def scan_read_fields(*args, use_mnemonic=False):
        if args == ("scan_state",) and use_mnemonic:
            return False
        else:
            raise ValueError("Unsupported args & kwargs for scan read_fields mock")
    cobra._system_version = {
        "api_version": "4.2.0",
        "firmware_sha": "0xdeadbeef",
        "os_build_number": "0xf00dd00d",
        "os_build_version": "0xc0011000",
        "os_build_sha": "0x8badcafe",
        "manifest": "test.xml",
        "manifest_sha": "0xbaad1add"}
    cobra.compute.os_build_number = "0xf00dd00d"
    cobra.compute.os_build_version = "0xc0011000"
    cobra.compute.os_build_sha = "0x8badcafe"
    cobra.compute.manifest = "test.xml"
    cobra.compute.manifest_sha = "0xbaad1add"
    cobra.sen.sensor_sn = 999

    cobra.disable = MagicMock()
    cobra.shutdown_sh_power = MagicMock()

    # Make sensor head available
    sh = cobra._sen
    sh._state = State.ENERGIZED
    sh._roi_mapping = roi_mapping
    lov = LaserPowerPercentMappedOvFactory()
    sh.laser_power_percent_mapped_ov = lov("m30", 0)
    sh._random_access_scan = MagicMock()
    sh._random_access_scan.ras_scan_parameters = MagicMock()

    # Set sensor head-specific mocks
    sh.start = MagicMock(side_effect=lambda sh_=sh, start_fe_streaming=True: set_state(sh_, State.SCANNING))
    sh.stop = MagicMock(side_effect=lambda sh_=sh, stop_fe_streaming=True: set_state(sh_, State.ENERGIZED))

    sh.apply_settings = MagicMock()

    # Set the debug read_fields side effect
    sh.debug.read_fields = MagicMock(side_effect=debug_read_fields)

    # Set the scan read_fields side effect
    sh.scan.read_fields = MagicMock(side_effect=scan_read_fields)

    return cobra


@pytest.fixture(autouse=True)
def mock_remote_ctx(mock_cobra) -> Cobra:
    """Fixture to mock the ``remote`` context manager in a way that it returns
    a ``MagicMock`` object of the typical ``Cobra`` return.
    This fixture yields the mocked ``Cobra`` class.
    """

    Cobra.remote = MagicMock()
    Cobra.remote.return_value.__enter__.return_value = mock_cobra
    yield Cobra
