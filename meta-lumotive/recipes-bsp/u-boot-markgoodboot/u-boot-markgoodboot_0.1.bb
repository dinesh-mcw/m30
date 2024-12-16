SUMMARY = "Mark good boot in u-boot environment"
DESCRIPTION = "Reset boot counters in u-boot environment to mark good boot of main image"
LICENSE = "CLOSED"

inherit systemd
SYSTEMD_AUTO_ENABLE = "enable"
SYSTEMD_SERVICE:${PN} = "markgoodboot.service"

SRC_URI:append = " \
    file://markgoodboot.service \
    file://markgoodboot.sh \
"

RDEPENDS_${PN}:append = "bash"

S = "${WORKDIR}"

FILES:${PN} = "${datadir}/${PN}/*"
FILES:${PN}:append = " ${systemd_system_unitdir}/markgoodboot.service"

do_install:append() {
    install -d ${D}${datadir}/${PN}
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 \${WORKDIR}/markgoodboot.service \${D}/\${systemd_unitdir}/system
    install -m 0755 ${WORKDIR}/*.sh ${D}${datadir}/${PN}
    sed -i -e 's|@SCRIPTDIR@|${datadir}/${PN}|g' ${D}${systemd_system_unitdir}/markgoodboot.service
}
