"""
Common configurations for all unit tests.
"""

import logging
import random
from logging.handlers import RotatingFileHandler

import numpy as np
import pytest
from cobra_lidar_api.web_server import create_app

log = logging.getLogger(__name__)
log.addHandler(logging.NullHandler())


def pytest_configure():
    # RotatingFileHandler seems to have permission issues when running
    # off the system, so temporarily remove it for testing.
    # Pytest grabs these with the config in ``pytest.ini`` anyway.
    cobra_logger = logging.getLogger("cobra_system_control.cobra_log")
    for handler in cobra_logger.handlers:
        if isinstance(handler, RotatingFileHandler):
            # Make sure to close it so the resource doesn't stay open
            handler.close()
            cobra_logger.removeHandler(handler)


def pytest_addoption(parser):
    parser.addoption(
        "--seed",
        action="store",
        type=int,
        default=0,
        help=("Seed to use for deterministic random number generation. "
              "Specify this to replicate unique test results."),
    )


@pytest.fixture
def seed(request):
    """Sets the seed for random number generation.
    """

    rand_seed = request.config.getoption("--seed")
    log.info(f"Setting random seed to `{rand_seed}`.")

    random.seed(rand_seed)
    np.random.seed(rand_seed)


@pytest.fixture(scope="session")
def app():
    app = create_app()
    yield app
