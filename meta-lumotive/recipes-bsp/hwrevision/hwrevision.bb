SUMMARY = "Set hwrevision file"
DESCRIPTION = "Set hwrevision file based on GPIOs"
LICENSE = "CLOSED"

inherit systemd

RDEPENDS:${PN}:append = "bash libgpiod-tools"

SRC_URI:append = " \
    file://hwrevision.sh \
    file://hwrevision.service \
"

S = "${WORKDIR}"

SYSTEMD_AUTO_ENABLE = "enable"
SYSTEMD_SERVICE:${PN} = "hwrevision.service"

do_install:append() {
    install -Dm 0755 ${S}/hwrevision.sh       ${D}${bindir}/hwrevision.sh
    install -Dm 0644 ${S}/hwrevision.service  ${D}${systemd_system_unitdir}/hwrevision.service
}
