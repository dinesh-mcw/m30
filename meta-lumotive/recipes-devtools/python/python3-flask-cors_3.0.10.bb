SUMMARY = "A Flask extension adding a decorator for CORS support"
HOMEPAGE = "https://github.com/corydolphin/flask-cors"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=118fecaa576ab51c1520f95e98db61ce"

SRC_URI[md5sum] = "647ff0632b960ba063a077fb4063077e"
SRC_URI[sha256sum] = "b60839393f3b84a0f3746f6cdca56c1ad7426aa738b70d6c61375857823181de"

PYPI_PACKAGE = "Flask-Cors"
PYPI_PACKAGE_EXT = "tar.gz"

inherit pypi setuptools3

RDEPENDS_${PN} = "\
  python3-six \
  python3-flask \
  "
