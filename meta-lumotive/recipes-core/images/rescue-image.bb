# From meta-swupdate's swupdate-image

SUMMARY = "Root filesystem for swupdate as rescue system"
DESCRIPTION = "Root FS to start swupdate in rescue mode"
LICENSE = "MIT"

require rescue-image.inc

# SWUpdate does better with gzip than bzip2
IMAGE_FSTYPES = "ext4.gz"

# Extra Packages
IMAGE_INSTALL:append = " cobra-fs-rescue cobra-fs-common hwrevision \
                         u-boot-fw-utils kernel-image kernel-devicetree \
                         curl openssh-sshd openssh-scp openssh-sftp-server \
                         parted e2fsprogs lrzsz imx-gpu-viv \
                       "

cobra_fw_env_config() {
    cat > ${IMAGE_ROOTFS}/etc/fw_env.config <<-EOM
/dev/mmcblk0 0x400000 0x2000
/dev/mmcblk0 0x402000 0x2000
EOM
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
    cobra_build_version_config; \
    cobra_set_root_password; \
"
