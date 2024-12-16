FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://0001-Switch-to-redundant-environment-storage.patch \
            file://0003-Update-deployed-etc-fw_env.config.patch \
            file://0004-Changing-to-ext4-kernel-loading.patch \
            file://0005-Removing-safety-on-mmcroot-env-variable-that-prevent.patch \
            file://0006-Adding-main-recovery-switch-bootscript.patch \
            file://0007-Disabling-netboot.patch \
            file://0008-Adding-automatic-reboot-on-boot-failure.patch \
            file://0009-Prevent-boot-into-valid-but-partially-installed-main.patch \
            file://0010-Add-additional-pins-to-GPIO-hog.patch \
            file://0011-Get-boot-files-from-boot-directory.patch \
            file://0012-Support-8-GB-DRAM.patch \
            file://0013-Final-uboot-gpio-hog-settings.patch \
            file://0014-Use-the-m30-dtb-file-by-default.patch \
            file://0015-M30-277-set-u-boot-boot-delay-to-0.patch \
            file://0016-M30-278-add-quiet-to-kernel-params.patch \
            file://0017-Change-boot-order-to-MAIN-first-for-combined-wic-image.patch \
            file://0018-Enable-physical-journaling-on-the-root-fs.patch \
            "
