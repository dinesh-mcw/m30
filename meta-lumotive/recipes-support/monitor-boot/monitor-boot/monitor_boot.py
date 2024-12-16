#!/usr/bin/env python3

import sys
import time
import requests
import logging

MAX_WAIT_TIME   = 300                       # Maximum seconds to check for energized state
STATE_URL       = "http://localhost/state"  # API endpoint for sensor head state

STATE_ENERGIZED = "ENERGIZED"
STATE_READY     = "READY"
STATE_SCANNING  = "SCANNING"

def get_state():
    """Get the current state from the API."""
    try:
        response = requests.get(STATE_URL)
        return response.json().get('state')
    except requests.RequestException as e:
        logging.error(f"Request failed: {e}")
    except ValueError as e:
        logging.error(f"JSON decoding failed: {e}")
    return None

def main():
    for i in range(MAX_WAIT_TIME):
        state = get_state()
        if state == STATE_ENERGIZED:
            print(f"System boot time: {i} seconds")
            sys.exit(0)
        elif state in [STATE_READY, STATE_SCANNING]:
            pass  # Continue waiting for the state to change
        else:
            logging.warning(f"Unknown state: {state}")
        time.sleep(1)
    logging.error("System did not reach the energized state within the maximum wait time.")
    sys.exit(1)

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    main()
