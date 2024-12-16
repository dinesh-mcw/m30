SUMMARY = "Remote object communication library, fifth major version"
HOMEPAGE = "https://github.com/irmen/Pyro5"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=c1c9ccd5f4ca5d0f5057c0e690a0153d"

SRC_URI[md5sum] = "3c22a47a98fc0f8e02aa3f5c7aa6f606"
SRC_URI[sha256sum] = "616e6957c341da0ca26f947805c9c97b42031941f59ca5613537d1420ff4f2e2"

SRC_URI += " file://fix-queue-expose.patch"

PYPI_PACKAGE = "Pyro5"
PYPI_PACKAGE_EXT = "tar.gz"

inherit pypi setuptools3

RDEPENDS_${PN} = "\
  python3-serpent \
  "
