SUMMARY = "Read resources from Python packages"
HOMEPAGE = "https://github.com/python/importlib_resources"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=e81780ac4c0888aaef94a7cb49b55edc"

SRC_URI[md5sum] = "5db738106ca7c05340495c36357986a2"
SRC_URI[sha256sum] = "a65882a4d0fe5fbf702273456ba2ce74fe44892c25e42e057aca526b702a6d4b"

PYPI_PACKAGE = "importlib_resources"
PYPI_PACKAGE_EXT = "tar.gz"

inherit pypi setuptools3

RDEPENDS_${PN} = "\
  python3-zipp \
  "
