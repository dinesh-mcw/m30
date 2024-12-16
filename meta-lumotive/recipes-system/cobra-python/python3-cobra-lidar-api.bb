SUMMARY = "M30 Python API"
DESCRIPTION = "M30 Python source code for Cobra API"
LICENSE = "CLOSED"

inherit python_flit_core systemd

RDEPENDS:${PN}:append = " bash python3 sudo"

PV = "1.0+git${SRCPV}"

BRANCH_COBRA_LIDAR_API ?= "master"
SRCREV_COBRA_LIDAR_API ?= "db55b3747bbb8ef2c4ab316f710c10706d93ba32"

SRCREV = "${SRCREV_COBRA_LIDAR_API}"
SRC_URI = "git://git@bitbucket.org/lumotive/cobra_lidar_api.git;protocol=ssh;branch=${BRANCH_COBRA_LIDAR_API}"

SYSTEMD_AUTO_ENABLE = "enable"
SYSTEMD_SERVICE:${PN} = "cb_api.service"

S = "${WORKDIR}/git"

FILES:${PN} += " ${systemd_system_unitdir}/cb_api.service ${bindir}/run_cb_api"

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 644 ${S}/cobra_lidar_api/service/cb_api.service ${D}${systemd_system_unitdir}
    install -d ${D}${bindir}
    install -m 755 ${S}/cobra_lidar_api/service/run_cb_api ${D}${bindir}
}
