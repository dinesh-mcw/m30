FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SCFW_FILENAME = "MT53D1024M32D4_DRC0_scfw_tcm.bin"

SRC_URI += "file://${SCFW_FILENAME} \
           "

do_patch() {
    grep -q git-lfs ${SCFW_FILENAME} && \
        bberror "Please run 'git lfs install && git lfs pull' in meta-lumotive"
    cp ${SCFW_FILENAME} ${S}/mx8qm-mek-scfw-tcm.bin
}
