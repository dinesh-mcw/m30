#!/bin/sh

EXPAND_P=`/bin/df | /bin/grep '/dev/root' | /usr/bin/awk '{ print $2 / 1000 < 10000 }'`

# EXPAND_P == 1 if the rootfs needs to be expanded
if [ ${EXPAND_P} -ne 0 ] ; then
    logger "Expanding main partition filesystem to fill eMMC"
    /sbin/resize2fs /dev/mmcblk0p2
else
    logger "Main partition filesystem was not expanded because it is already at the maximum size"
fi

# Now make sure the rescue partition is also mounted
NETFILE=/etc/systemd/network/20-eth-static.network
RESCUE_ROOT=/media
/bin/mount -t ext4 /dev/mmcblk0p1 $RESCUE_ROOT
md1=`/usr/bin/md5sum $NETFILE | /usr/bin/awk '{ print $1}'`
md2=`/usr/bin/md5sum ${RESCUE_ROOT}${NETFILE} | /usr/bin/awk '{ print $1}'`
if [ $md1 != $md2 ] ; then
    logger "Network settings in rescue partition don't match those in main partition; updating rescue partition"
    /bin/cp ${NETFILE} ${RESCUE_ROOT}${NETFILE}
    sync
else
    logger "Network settings in rescue partition match those in main partition; no update performed"
fi
/bin/umount $RESCUE_ROOT
