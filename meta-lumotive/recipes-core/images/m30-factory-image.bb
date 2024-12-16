DESCRIPTION = "M30 factory image"
LICENSE = "CLOSED"

inherit image

# Skip installing rootfs features
IMAGE_FEATURES = ""

# Skip installing additional language support packages
IMAGE_LINGUAS = ""

IMAGE_BASENAME = "m30-factory-image"
IMAGE_FSTYPES = "wic.zst wic.bmap"

DEPENDS:append = " imx-boot rescue-image m30-core-image"
IMAGE_DEPENDS:append = " imx-boot rescue-image m30-core-image"

WKS_FILE = "${IMAGE_BASENAME}.wks.in"
