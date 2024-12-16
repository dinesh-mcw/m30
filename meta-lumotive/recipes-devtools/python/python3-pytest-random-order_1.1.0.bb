SUMMARY = "pytest plugin to randomize the order of tests"
HOMEPAGE = "https://github.com/jbasko/pytest-random-order"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=58b73e71c0b2a32ae498f39f92c8d16c"

PV = "1.1.0"
PYPI_PACKAGE = "pytest-random-order"
BPN = "pytest-random-order"
SRC_URI = "https://files.pythonhosted.org/packages/source/${BPN:0:1}/${BPN}/${BP}.tar.gz"

inherit pypi setuptools3

SRC_URI[md5sum] = "1701109bacfe9901e5fc3c614a3ca83f"
SRC_URI[sha256sum] = "dbe6debb9353a7af984cc9eddbeb3577dd4dbbcc1529a79e3d21f68ed9b45605"

RDEPENDS_${PN} = " \
    python3-pytest \
"