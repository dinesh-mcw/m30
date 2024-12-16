#!/bin/sh

if [ ! -e /dev/mmcblk0p2 ] ; then
    logger Making main partition and creating ext4 fs
    /usr/sbin/parted /dev/mmcblk0 unit s mkpart p ext4 540672 62160895
    # we make the filesystem in case there was an old one on a system that has been reflashed
    /sbin/mkfs.ext4 /dev/mmcblk0p2
else
    logger Main partition was not created because it already exists
fi
