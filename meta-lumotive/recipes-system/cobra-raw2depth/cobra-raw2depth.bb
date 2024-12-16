SUMMARY = "C++ Raw to Depth Pipeline"
LICENSE = "CLOSED"

inherit cmake systemd

DEPENDS:append = " googletest"

PV = "1.0+git${SRCPV}"

BRANCH_COBRA_RAW2DEPTH ?= "master"
SRCREV_COBRA_RAW2DEPTH ?= "9383df60bc1f527c43b204e931c1fbdae03a3010"

SRCREV = "${SRCREV_COBRA_RAW2DEPTH}"
SRC_URI = "git://git@bitbucket.org/lumotive/cobra_raw2depth.git;protocol=ssh;lfs=1;branch=${BRANCH_COBRA_RAW2DEPTH}"

FILES:${PN}:append = " ${systemd_system_unitdir} /home/root/cobra"

S = "${WORKDIR}/git"

SYSTEMD_AUTO_ENABLE = "enable"
SYSTEMD_SERVICE:${PN} = "frontend.service"
