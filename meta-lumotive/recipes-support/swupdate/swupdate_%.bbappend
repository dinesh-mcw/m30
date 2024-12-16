FILESEXTRAPATHS:append := "${THISDIR}/${PN}:"
FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}-pre:"

PACKAGECONFIG_CONFARGS = ""

DEPENDS:append = " openssl"

SRC_URI:append = " \
    file://09-swupdate-args \
    file://swupdate.cfg \
    file://swupdate.pub.pem \
"

do_install:append() {
    install -m 0644 ${WORKDIR}/09-swupdate-args      ${D}${libdir}/swupdate/conf.d/
    install -m 0644 ${WORKDIR}/swupdate.pub.pem      ${D}${libdir}/swupdate

    install -d ${D}${sysconfdir}
    install -m 644 ${WORKDIR}/swupdate.cfg           ${D}${sysconfdir}
    sed -i -e 's|@SWUPDATEDIR@|${libdir}/swupdate|g' ${D}${sysconfdir}/swupdate.cfg
}
