"""
Tests the basic web server configuration.
"""

from pathlib import Path

import pytest

index_loc = Path(__file__).parents[2] / "cobra_lidar_api" / "m30_webapp" / "index.html"

pytestmark = pytest.mark.skipif(
    not index_loc.exists(),
    reason=("The 'm30_webapp' build folder and corresponding index.html file "
            "does not exist, so it does not make sense to run these tests."),
)


def test_index(client):
    r = client.get("/index", follow_redirects=True)

    assert r.status_code == 200
    assert r.content_type == "text/html; charset=utf-8"


def test_index_redirects(client):
    r1 = client.get("/", follow_redirects=True)
    r2 = client.get("/index", follow_redirects=True)

    assert r1.data == r2.data
