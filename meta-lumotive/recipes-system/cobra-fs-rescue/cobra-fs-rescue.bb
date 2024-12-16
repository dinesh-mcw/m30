SUMMARY = "Customizations to rescue filesystem"
LICENSE = "CLOSED"

inherit systemd

SRC_URI = "file://cobra_mk_main.sh \
           file://cobra_mk_main.service "

SYSTEMD_AUTO_ENABLE = "enable"
SYSTEMD_SERVICE:${PN} = "cobra_mk_main.service"

FILES:${PN} += "${bindir}/cobra_mk_main.sh "
FILES:${PN} += "${systemd_system_unitdir}/cobra_mk_main.service "

S = "${WORKDIR}"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 cobra_mk_main.sh ${D}${bindir}
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 cobra_mk_main.service ${D}${systemd_system_unitdir}
}
