#!/bin/sh

# If system survives for 120 seconds, consider boot successful
sleep 120
fw_setenv BOOT_MAIN_LEFT 5
fw_setenv BOOT_RESCUE_LEFT 3

exit 0
