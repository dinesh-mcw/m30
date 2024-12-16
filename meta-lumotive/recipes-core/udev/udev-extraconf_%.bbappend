do_install:append () {
	install -d ${D}${sysconfdir}/udev
	echo "/dev/mmcblk0p1" >>  ${D}${sysconfdir}/udev/mount.blacklist
	echo "/dev/mmcblk1p1" >>  ${D}${sysconfdir}/udev/mount.blacklist
	echo "/dev/mmcblk0p3" >>  ${D}${sysconfdir}/udev/mount.blacklist
	echo "/dev/mmcblk1p3" >>  ${D}${sysconfdir}/udev/mount.blacklist
}
