SUMMARY = "Application group(s) for Lumotive sensors"

PACKAGE_ARCH = "${MACHINE_ARCH}"

inherit packagegroup

RDEPENDS:packagegroup-lumotive-system = " \
    cobra-fs-common \
    cobra-fs-main \
    cobra-gui \
    cobra-raw2depth \
    hwrevision \
    python3-cobra-system-control \
    python3-cobra-lidar-api \
"
