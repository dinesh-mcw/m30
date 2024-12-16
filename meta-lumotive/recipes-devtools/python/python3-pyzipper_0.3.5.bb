SUMMARY = "AES encryption for zipfile."
HOMEPAGE = "https://github.com/danifus/pyzipper"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=4a6ce89f8836606b2fa79b7d8e898868"

SRC_URI[md5sum] = "02ebe358da73ea0db486d464be9a7335"
SRC_URI[sha256sum] = "6040069654dad040cf8708d4db78ce5829238e2091ad8006a47d97d6ffe275d6"

PYPI_PACKAGE = "pyzipper"
PYPI_PACKAGE_EXT = "tar.gz"

inherit pypi setuptools3

RDEPENDS_${PN} = "\
  python3-pycryptodomex \
  "
