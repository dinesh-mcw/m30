DESCRIPTION = "Build M30 SWUpdate compound image"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit swupdate

IMAGE_DEPENDS = "m30-core-image"

SRC_URI = " \
    file://sw-description \
"

# Hide these from swupdate class, otherwise they'll be copied into the swu file for accidental distribution!
SIGNING_SRC_URI = " \
    file://swupdate.priv.pem \
    file://swupdate.priv.pem.pass \
"

SWUPDATE_IMAGES = "m30-core-image-imx8qmmek"
SWUPDATE_IMAGES_FSTYPES[m30-core-image-imx8qmmek] = ".ext4.gz"

# Codesigning
SWUPDATE_SIGNING = "RSA"
SWUPDATE_PRIVATE_KEY = "${WORKDIR}/swupdate.priv.pem"
SWUPDATE_PASSWORD_FILE = "${WORKDIR}/swupdate.priv.pem.pass"

# And place them in the work dir
do_fetch:append() {
    signing_src_uri = (d.getVar('SIGNING_SRC_URI') or "").split()
    try:
        fetcher = bb.fetch2.Fetch(signing_src_uri, d)
        fetcher.download()
    except bb.fetch2.BBFetchException as e:
        bb.fatal(str(e))
}

do_unpack:append() {
    signing_src_uri = (d.getVar('SIGNING_SRC_URI') or "").split()
    if len(signing_src_uri) == 0:
        return

    try:
        fetcher = bb.fetch2.Fetch(signing_src_uri, d)
        fetcher.unpack(d.getVar('WORKDIR'))
    except bb.fetch2.BBFetchException as e:
        bb.fatal(str(e))
}
