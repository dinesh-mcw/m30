# Remove bootchart2-done timer/unit so monitor-boot can stop it

SYSTEMD_SERVICE:${PN}:remove = "bootchart2-done.service bootchart2-done.timer"

do_install:append() {
    rm -f ${D}${systemd_system_unitdir}/bootchart2-done*
}
