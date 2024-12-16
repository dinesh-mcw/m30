SUMMARY = "Monitor system boot time"
DESCRIPTION = "Monitor the boot progress of the system control software and exit when the ENERGIZED state is returned"
LICENSE = "CLOSED"

inherit systemd

SRC_URI:append = " \
    file://monitor-boot.service \
    file://monitor_boot.py \
"

RDEPENDS:${PN} += "bootchart2"

SYSTEMD_AUTO_ENABLE = "enable"
SYSTEMD_SERVICE:${PN} = "monitor-boot.service"

S = "${WORKDIR}"

do_install:append() {
    install -d ${D}${datadir}/${PN}
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${S}/monitor-boot.service    ${D}${systemd_system_unitdir}
    install -m 0755 ${S}/monitor_boot.py         ${D}${datadir}/${PN}
    sed -i -e 's|@SCRIPTDIR@|${datadir}/${PN}|g' ${D}${systemd_system_unitdir}/monitor-boot.service
}
