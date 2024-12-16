# From meta-swupdate's swupdate-image

USE_DEVFS = "1"

# This variable is triggered to check if sysvinit must be overwritten by a single rcS
# export SYSVINIT = "no"

VIRTUAL-RUNTIME_init_manager = "systemd"
VIRTUAL-RUNTIME_initscripts = ""

IMAGE_INSTALL = "base-files \
		base-passwd \
		busybox \
		mtd-utils \
		mtd-utils-ubifs \
		libconfig \
		swupdate \
		swupdate-www \
		systemd \
		util-linux-sfdisk \
		u-boot-fw-utils"

IMAGE_FSTYPES = "ext4.gz"

inherit image

IMAGE_LINGUAS = " "
