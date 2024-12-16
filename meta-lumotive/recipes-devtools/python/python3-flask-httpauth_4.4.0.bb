SUMMARY = "Basic and Digest HTTP authentication for Flask routes"
HOMEPAGE = "http://github.com/miguelgrinberg/flask-httpauth/"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=3b69377f79f3f48c661701236d5a6a85"

SRC_URI[md5sum] = "b321625f1dd77e5232083a659264d9ea"
SRC_URI[sha256sum] = "bcaaa7a35a3cba0b2eafd4f113b3016bf70eb78087456d96484c3c18928b813a"

PYPI_PACKAGE = "Flask-HTTPAuth"
PYPI_PACKAGE_EXT = "tar.gz"

inherit pypi setuptools3

RDEPENDS_${PN} = "\
  python3-flask \
  "
