"""
Module description here
"""

import json
from base64 import b64encode
from pathlib import Path
from secrets import token_bytes

CREDENTIAL_PATH = Path.home() / ".lumotive" / "credentials"
KEYS = ["pubkey", "logkey"]


def make_keys():
    CREDENTIAL_PATH.parent.mkdir(parents=True, exist_ok=True)
    tokens = {}
    for key in KEYS:
        tokens[key] = b64encode(token_bytes(64)).decode("utf-8")

    with open(CREDENTIAL_PATH, "w+") as f:
        json.dump(tokens, f)


if __name__ == '__main__':
    # Python always exits with nonzero exit codes on exceptions
    make_keys()
