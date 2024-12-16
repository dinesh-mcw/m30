"""
Top-level configuration for all API testing.
"""


def pytest_addoption(parser):
    parser.addoption(
        "--hostname",
        action="store",
        default=None,
    )


def pytest_configure(config):
    if config.getoption("--hostname", None) is None:
        setattr(config.option, "markexpr", "not integration")
