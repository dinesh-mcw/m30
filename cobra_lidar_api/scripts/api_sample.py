"""
Sample API consumption script.
"""

import json
import pprint
import random
import sys
from pathlib import Path
from typing import Callable

import requests
from api_argparse import parse_args
from requests.auth import AuthBase

# Replace this path to where your credentials file lives (if different)
PUBKEY_PATH = Path.home() / ".lumotive" / "credentials"

# Read the pubkey from the credentials file if it exists.
# This may be different based on how the user wants to store their info.
if PUBKEY_PATH.exists():
    with open(PUBKEY_PATH, "r+") as f:
        _cfg = json.load(f)
        try:
            PUBKEY = _cfg["pubkey"]
        except KeyError:
            print("CAN'T GET PUBKEY")
            PUBKEY = ""
else:
    PUBKEY = ""


class BearerAuth(AuthBase):
    """Custom authorization class for Bearer authentication.

    The same effect can be achieved by simply passing
    ``headers={"Authorization": f"Bearer: {token}"}``
    into the request kwargs instead.
    """

    def __init__(self, token):
        self.token = token

    def __call__(self, r):
        r.headers["Authorization"] = f"Bearer {self.token}"
        return r


class SystemParameter:
    """System parameter class for simple access to parameter options,
    getters, and setters, with flexibility to specify individual sensor heads.

    This is intended to be composed into a system API object with GET and POST
    methods directly passed in to retain consistency.

    The GET method should have the signature:
    .. code:: python
        def get(self, endpoint: str, heads: list = None):

    The POST method should have the signature:
    .. code:: python
        def post(self, endpoint: str, data: dict = None, heads: list = None):

    Note that in both signatures, ``endpoint`` is accessed positionally,
    while ``data`` and ``heads`` are accessed by name.

    See sample methods in the ``M20`` class below.
    """

    def __init__(self, base: str, get_method: Callable, post_method: Callable):
        self.base = base
        self._get = get_method
        self._post = post_method

    def options(self):
        return self._get(f"{self.base}/opts")

    def get(self, heads: list = None):
        return self._get(self.base, heads=heads)

    def set(self, value, heads: list = None):
        data = value if isinstance(value, dict) else {"data": value}
        return self._post(self.base, data=data, heads=heads)


class M20:
    def __init__(self, hostname: str):
        self.hostname = f"http://{hostname}"

        # Reduce option/getter/setter boilerplate using composition
        self.angle_range = SystemParameter("angle_range", self._get, self._post)
        self.bin = SystemParameter("bin", self._get, self._post)
        self.nn_level = SystemParameter("nn_level", self._get, self._post)
        self.snr_threshold = SystemParameter("snr_threshold", self._get, self._post)

        self.fps_multiple = SystemParameter("fps_multiple", self._get, self._post)
        self.power_index = SystemParameter("power_index", self._get, self._post)
        self.inte_time_index = SystemParameter("inte_time_index", self._get, self._post)
        self.max_range_index = SystemParameter("max_range_index", self._get, self._post)
        self.time_boxcar_level = SystemParameter("time_boxcar_level", self._get, self._post)

        self.scan_parameters = SystemParameter("scan_parameters", self._get, self._post)

    @staticmethod
    def _parse_response(resp: requests.Response) -> dict:
        if resp.status_code == 200:
            return resp.json()
        else:
            raise RuntimeError(
                f"Response returned with status code {resp.status_code} "
                f"and contents {resp.text}"
            )

    def _get(self, endpoint: str, heads: list = None) -> dict:
        if heads:
            query_str = f"?{'&'.join([f'sens={h}' for h in heads])}"
        else:
            query_str = ""
        ret = requests.get(
            url=f"{self.hostname}/{endpoint}{query_str}",
            auth=BearerAuth(PUBKEY),
        )
        return M20._parse_response(ret)

    def _post(self, endpoint: str, data: dict = None, heads: list = None) -> dict:
        if heads:
            if data is None:
                data = dict()
            data["sens"] = heads
        ret = requests.post(
            url=f"{self.hostname}/{endpoint}",
            auth=BearerAuth(PUBKEY),
            json=data,
        )
        return M20._parse_response(ret)

    # System information
    @property
    def available_sensors(self):
        return self._get("sensors")

    def calibrated(self, heads: list = None):
        return self._get("calibrated", heads=heads)

    def git_sha(self, heads: list = None):
        return self._get("git_sha", heads=heads)

    def state(self, heads: list = None):
        return self._get("state", heads=heads)

    # Scan control
    def restart(self, heads: list = None):
        if heads is not None:
            raise ValueError("Action 'restart' operates on all sensor heads.")
        return self._post("restart")

    def disable(self, heads: list = None):
        if heads is not None:
            raise ValueError("Action 'restart' operates on all sensor heads.")
        return self._post("disable")

    def start_scan(self, heads: list = None):
        return self._post("start_scan", heads=heads)

    def stop_scan(self, heads: list = None):
        return self._post("stop_scan", heads=heads)


def sample():
    """This is a sample method to demonstrate programmatic access using
    the M20 class above within the context of a simple program that
    configures the sensor before and during scanning, then stops it.

    Please remember that the above class is just a utility wrapper
    around common HTTP request operations and is not required to
    perform any of the API operations shown below.
    If you can send an HTTP request, you can use the API.
    """

    # Initialize the API comms object
    hostname = "localhost:5001"  # Define your hostname, don't specify "http://"
    m20 = M20(hostname)

    # Get the available sensors
    available = m20.available_sensors
    if not available:
        raise RuntimeError("No sensor heads have been detected!")

    # Get a random selection of a random number of available sensor heads
    heads = random.sample(available, k=random.randint(1, len(available)))

    # Perform pre-scan configurations...

    # ... on all sensor heads
    m20.snr_threshold.set(2.4)
    m20.nn_level.set(3)

    # Start scanning
    m20.start_scan()

    # Update parameters during the scan (note: this restarts the scan)...

    # ... on random sensor heads
    m20.angle_range.set([-30, 30], heads)

    # Stop scanning
    m20.stop_scan()

    # Prepare the system for disable
    m20.restart()


def main(args):
    """CLI functionality utilizing the above ``M20`` api class.

    This uses the ``api_argparse`` module at the same level to parse
    arguments. Help can be displayed with the ``-h`` or ``--help`` flags.

    Args:
        args: Namespace generated from ``parse_args`` in the
            ``api_argparse`` module at the same level.

    Notes:
        * This prints the results of the commands specified via CLI flags.
        * If a getter and setter for the same parameter are called,
            the getter result will be displayed _first_, and the setter
            will be called after.
    """

    m20 = M20(args.hostname)
    pp = pprint.PrettyPrinter(indent=4)

    # Show sensor state information
    if args.sha:
        pp.pprint(f"Git sha is {m20.git_sha(heads=args.sh)}")
    if args.state:
        pp.pprint(f"State is {m20.state(heads=args.sh)}")
    if args.calibrated:
        pp.pprint(f"Calibrated is {m20.calibrated(heads=args.sh)}")

    # Get system parameters
    if args.get_angle_range:
        pp.pprint(f"Angle range is {m20.angle_range.get(args.sh)}")
    if args.get_snr:
        pp.pprint(f"SNR is {m20.snr_threshold.get(args.sh)}")
    if args.get_nn:
        pp.pprint(f"NN is {m20.nn_level.get(args.sh)}")
    if args.get_bin:
        pp.pprint(f"Binning level is {m20.bin.get(args.sh)}")

    # Set angle range
    if args.angle is not None:
        pp.pprint(m20.angle_range.set(args.angle, heads=args.sh))

    # Set filtering parameters
    if args.snr is not None:
        pp.pprint(m20.snr_threshold.set(args.snr, args.sh))
    if args.nn is not None:
        pp.pprint(m20.nn_level.set(args.nn, args.sh))
    if args.bin is not None:
        pp.pprint(m20.bin.set(args.bin, args.sh))

    # Scan
    if args.start:
        pp.pprint(m20.start_scan(heads=args.sh))
    elif args.stop:
        pp.pprint(m20.stop_scan(heads=args.sh))
    elif args.restart:
        pp.pprint(m20.restart())
    elif args.disable:
        pp.pprint(m20.disable())


if __name__ == '__main__':
    args_ = parse_args(sys.argv[1:], description=main.__doc__)
    main(args_)
