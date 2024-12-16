DESCRIPTION = "Basic M30 core image"
LICENSE = "CLOSED"

inherit core-image
inherit swupdate-enc

IMAGE_BASENAME = "m30-core-image"
IMAGE_FSTYPES = "ext4.gz"

IMAGE_FEATURES:append = " \
    post-install-logging \
    tools-debug \
    tools-sdk \
    ssh-server-openssh \
"

IMAGE_INSTALL:append = " \
    kernel \
    kernel-devicetree \
    kernel-image \
    kernel-modules \
    packagegroup-imx-core-tools \
    packagegroup-imx-security \
    packagegroup-lumotive-core-utils \
    packagegroup-lumotive-system \
    packagegroup-lumotive-extra-python \
"

# Use correct u-boot-fw-utils
PREFERRED_PROVIDER_u-boot-fw-utils = "u-boot-imx-fw-utils"

cobra_fw_env_config() {
    cat > ${IMAGE_ROOTFS}/etc/fw_env.config <<-EOM
/dev/mmcblk0 0x400000 0x2000
/dev/mmcblk0 0x402000 0x2000
EOM
}

cobra_python37_config() {
    mkdir -p ${IMAGE_ROOTFS}/usr/local/bin
    ln -s /usr/bin/python3.10 ${IMAGE_ROOTFS}/usr/local/bin/python3.7
}

cobra_build_version_config() {
    echo "OS_SHA=${BUILD_OS_REV}" > ${IMAGE_ROOTFS}/etc/lumotive_fs_rev
    echo "BUILD_VERSION=${BUILD_VERSION}" >> ${IMAGE_ROOTFS}/etc/lumotive_fs_rev
    echo "BUILD_NUMBER=${BUILD_NUMBER}" >> ${IMAGE_ROOTFS}/etc/lumotive_fs_rev
    echo "BUILD_MANIFEST=imx-5.15.52-cobra.xml" >> ${IMAGE_ROOTFS}/etc/lumotive_fs_rev
    echo "MANIFEST_SHA=${BUILD_OS_REV}" >> ${IMAGE_ROOTFS}/etc/lumotive_fs_rev
}

cobra_set_root_password() {
    sed -i 's/root::/root:$6$3ZooMle.LhM3isG4$yLwfdUDqz8t3TtlVvtxA5T5jRtD3IcNztKN.VPLncNcuWTniTtGjOfGoXQyVCVsjTVlmEIFnZ6SAqqRMusU0s1:/' ${IMAGE_ROOTFS}/etc/shadow
}

ROOTFS_POSTPROCESS_COMMAND:append = " \
    cobra_fw_env_config; \
    cobra_python37_config; \
    cobra_build_version_config; \
    cobra_set_root_password; \
"
