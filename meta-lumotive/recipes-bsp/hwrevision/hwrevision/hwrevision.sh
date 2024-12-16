#!/bin/sh

HW_REV_FILE="/etc/hw_rev"

rev3=`gpioget 6 19`
rev2=`gpioget 6 20`
rev1=`gpioget 6 21`
rev0=`gpioget 6 22`

err=0

function write_to_file() {
    echo $1 > $HW_REV_FILE
}

[ -f "$HW_REV_FILE" ] && {
    echo "hwrevision file exists, skipping"
    exit $err
}

case ${rev3}${rev2}${rev1}${rev0} in
    1111)
        write_to_file "NCB Rev 2"
        ;;
    *)
        echo "Unknown HW revision" >&2
        write_to_file "unknown"
        err=1
        ;;
esac

exit $err
