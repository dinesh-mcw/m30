SUMMARY = "Pytest plugin for test configuration"
HOMEPAGE = "https://github.com/wojole/pytest-testconfig"

LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=24f7f7494d260efee6e0ca09f7323c22"

PV = "0.2.0"
PYPI_PACKAGE = "pytest-testconfig"
BPN = "pytest-testconfig"

SRC_URI = "https://files.pythonhosted.org/packages/source/${BPN:0:1}/${BPN}/${BP}.tar.gz"
SRC_URI[md5sum] = "7d81c53c330976f4e070e1564b3ac8b9"
SRC_URI[sha256sum] = "0fa9e210bc2dd83d7408470cea8fb6e607576551bb3b1f9524cb51d8554a3da6"

S = "${WORKDIR}/${BP}"

inherit pypi setuptools3

RDEPENDS_${PN} += "\
    python3-pytest \
"
