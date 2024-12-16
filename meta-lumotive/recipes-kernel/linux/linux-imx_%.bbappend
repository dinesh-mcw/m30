FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://0001-Updating-pinctrl-for-TEC-GPIO.patch \
            file://0002-Making-mek-rpmsg-DTB-the-same-as-mek.patch \
            file://0003-Enabling-both-ADCs-muxing-inputs-and-adding-SCU-cloc.patch \
            file://0004-Disabling-unused-I2C.patch \
            file://0005-Hog-GPIO-lines.patch \
            file://0006-Support-ADIN-PHY.patch \
            file://0007-Add-m30-camera-driver.patch \
            file://0008-Disable-PCIE.patch \
            file://0009-DTB-fixes.patch \
            file://0010-Fix-setting-of-fps-in-M30-driver.patch \
            file://0011-Fix-DATA_LED-by-hogging-its-GPIO.patch \
            file://0012-Speed-up-MIPI-I2C-to-1Mbps.patch \
            file://0013-Quiet-down-messages.patch \
            file://0014-Dump-MIPI-regs-to-info-level.patch \
            file://0015-Add-GPIO-for-PPS.patch \
            file://0016-Get-the-1PPS-from-FEC1.patch \
            file://0017-Enable-pull-ups-for-pcb-rev-gpios.patch \
            "

KERNEL_DEVICETREE:append:imx8qm-mek = " freescale/imx8qm-mek-m30.dtb"
