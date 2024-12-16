SUMMARY = "Customizations to main filesystem"
LICENSE = "CLOSED"

inherit systemd

SRC_URI = "file://cobra_resize_fs.sh \
           file://cobra_resize_fs.service \
           file://fpga_upgrade_required \
           file://lcm_upgrade_required \
           file://credentials \
          "

SYSTEMD_AUTO_ENABLE = "enable"
SYSTEMD_SERVICE:${PN} = "cobra_resize_fs.service"

FILES:${PN} += "${bindir}/cobra_resize_fs.sh "
FILES:${PN} += "${systemd_system_unitdir}/cobra_resize_fs.service "
FILES:${PN} += "/home/root/cobra/fpga_upgrade_required "
FILES:${PN} += "/home/root/cobra/lcm_upgrade_required "
FILES:${PN} += "/home/root/.lumotive/credentials"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 cobra_resize_fs.sh ${D}${bindir}
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 cobra_resize_fs.service ${D}${systemd_system_unitdir}
    install -d ${D}/home
    install -d ${D}/home/root
    install -d ${D}/home/root/cobra
    install -d ${D}/home/root/.lumotive
    install -m 0644 fpga_upgrade_required ${D}/home/root/cobra
    install -m 0644 lcm_upgrade_required ${D}/home/root/cobra
    install -m 0644 credentials ${D}/home/root/.lumotive/credentials
}
