SUMMARY = "Various Cobra filesystem customizations common to rescue and main partitions"
LICENSE = "CLOSED"

SRC_URI = " \
    file://20-eth-static.network \
    file://10-system-alias.sh \
"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${sysconfdir}/systemd/network
    install -m 0644 20-eth-static.network ${D}${sysconfdir}/systemd/network

    install -Dm 0644 ${S}/10-system-alias.sh ${D}${sysconfdir}/profile.d/10-system-alias.sh
}
