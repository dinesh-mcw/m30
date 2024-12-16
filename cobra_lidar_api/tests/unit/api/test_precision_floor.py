"""
Tests the ``precision_floor`` method to ensure it is flooring correctly.
"""

import pytest

from cobra_lidar_api.schema import precision_floor


@pytest.mark.parametrize("value, precision, expected", [
    pytest.param(2.123, 2, 2.12),
    pytest.param(4.89898, 0, 4),
    pytest.param(1.99999, 1, 1.9),
])
def test_precision_floor(value: float, precision: int, expected: float):
    assert precision_floor(value, precision) == expected
