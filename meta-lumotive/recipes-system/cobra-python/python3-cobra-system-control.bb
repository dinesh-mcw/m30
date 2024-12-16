SUMMARY = "Python cobra_system_control repo"
LICENSE = "CLOSED"

inherit python_flit_core systemd

RDEPENDS:${PN}:append = " bash python3"

PV = "1.0+git${SRCPV}"

BRANCH_COBRA_SYSTEM_CONTROL ?= "develop"
SRCREV_COBRA_SYSTEM_CONTROL ?= "b7b9c46b8855701a941d52c1775d422a44dedaa8"

SRCREV = "${SRCREV_COBRA_SYSTEM_CONTROL}"
SRC_URI = "git://git@bitbucket.org/lumotive/cobra_system_control.git;protocol=ssh;lfs=1;branch=${BRANCH_COBRA_SYSTEM_CONTROL}"

SYSTEMD_AUTO_ENABLE = "enable"
SYSTEMD_SERVICE:${PN} = "remote.service monitor.service"

S = "${WORKDIR}/git"

FILES:${PN} += " \
    ${systemd_system_unitdir}/remote.service \
    ${systemd_system_unitdir}/monitor.service \
    ${bindir}/run_remote \
    ${bindir}/run_monitor \
    ${sysconfdir}/m30_sha_cobra_m20_fw.txt \
    "

do_install:append() {
    install -d ${D}${systemd_system_unitdir}
    install -m 644 ${S}/cobra_system_control/boot_scripts/systemd_services/remote.service ${D}${systemd_system_unitdir}
    install -m 644 ${S}/cobra_system_control/boot_scripts/systemd_services/monitor.service ${D}${systemd_system_unitdir}
    install -d ${D}${bindir}
    install -m 755 ${S}/cobra_system_control/boot_scripts/run_remote ${D}${bindir}
    install -m 755 ${S}/cobra_system_control/boot_scripts/run_monitor ${D}${bindir}
    install -d ${D}${sysconfdir}
    echo ${SRCPV} > ${D}${sysconfdir}/m30_sha_cobra_system_control.txt
}
