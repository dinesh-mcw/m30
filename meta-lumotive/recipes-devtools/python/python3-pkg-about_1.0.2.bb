SUMMARY = "Shares Python package metadata at runtime."
HOMEPAGE = "https://pypi.org/project/pkg_about/"
LICENSE = "Zlib"
LIC_FILES_CHKSUM = "file://LICENSE;md5=ab643cc09dd9a3b6d5eed7bf5b4af833"

SRC_URI[md5sum] = "874780d9ca80a22a9a28610fc2ff279a"
SRC_URI[sha256sum] = "15304fc8dad03ae504a7865b256a201f67ead26fcdaa09163eeb34858d8adeea"

PYPI_PACKAGE = "pkg_about"
PYPI_PACKAGE_EXT = "zip"

inherit pypi setuptools3

RDEPENDS_${PN} = "\
  python3-setuptools \
  python3-packaging \
  python3-importlib-resources \
  python3-importlib-metadata \
  "
