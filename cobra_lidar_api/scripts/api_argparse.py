"""
Module to configure the argument parsing for CLI usage of the
``api_sample.py`` script in the same folder.
"""

import argparse


def parse_args(argv, description=None):
    """Argument parsing for using this file with the command line.

    Note that if a getter and setter are both called,
    the getter will be called first, then the setter will be applied.
    """
    parser = argparse.ArgumentParser(description=description or __doc__)

    # Get the sensor IP from the command line
    parser.add_argument(
        "hostname",
        help="Hostname of the M20 sensor for API communication without the 'http://' prepend.",
    )

    start_stop_group = parser.add_mutually_exclusive_group()
    start_stop_group.add_argument(
        "-s",
        "--start",
        action="store_true",
        help="Stops the lidar.",
    )
    start_stop_group.add_argument(
        "-x",
        "--stop",
        action="store_true",
        help="Stops the lidar.",
    )
    start_stop_group.add_argument(
        "--restart",
        action="store_true",
        help="Restarts all sensor heads",
    )
    start_stop_group.add_argument(
        "--disable",
        action="store_true",
        help="Disables all sensor heads.",
    )

    # FOV and single angle cannot be set at the same time
    fov_group = parser.add_mutually_exclusive_group()

    # Setters
    parser.add_argument(
        "--sh",
        type=str,
        action="append",
        choices=("A", "B", "C", "D"),
        help="Specify the sensor head(s) on which to operate.",
    )
    fov_group.add_argument(
        "-a",
        "--angle",
        default=None,
        action="append",
        nargs=2,
        type=int,
        help="Specify the lower and upper angles ([-45, 45], in deg) for scanning.",
    )
    parser.add_argument(
        "-t",
        "--snr",
        default=None,
        type=float,
        help="Specify the SNR threshold level.",
    )
    parser.add_argument(
        "-n",
        "--nn",
        default=None,
        type=int,
        choices=tuple(range(8)),
        help="Specify the NN threshold level.",
    )
    parser.add_argument(
        "-b",
        "--bin",
        default=None,
        type=int,
        choices=(1, 2),
        help="Specify the binning level.",
    )

    # Getters - will be called _before_ the setters
    parser.add_argument(
        "--sha",
        action="store_true",
        help="Returns the git SHA.",
    )
    parser.add_argument(
        "--state",
        action="store_true",
        help="Returns the sensor state.",
    )
    parser.add_argument(
        "--get-angle-range",
        action="store_true",
        help="Returns the integer angle range setting.",
    )
    parser.add_argument(
        "--get-snr",
        action="store_true",
        help="Returns the SNR threshold level.",
    )
    parser.add_argument(
        "--get-nn",
        action="store_true",
        help="Returns the NN threshold level.",
    )
    parser.add_argument(
        "--get-bin",
        action="store_true",
        help="Returns the binning level.",
    )

    args = parser.parse_args(argv)

    # Value-check args that are continuous but bounded, or have a large number
    # of options in their domain (avoid showing 90-value arrays in the help).
    if args.angle is not None:
        if not all(-45 <= angle <= 45 for angle in args.angle[0]):
            raise ValueError("Angle values are bound to [-45, 45].")

    if args.snr is not None:
        if not (0.0 <= args.snr <= 20.47):
            raise ValueError("SNR values are bound to [0.00, 20.47].")

    return args
