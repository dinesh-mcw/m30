# git-lfs
SUMMARY = "GIT Large File System"
HOMEPAGE = "https://github.com/git-lfs"

GO_IMPORT = "github.com/git-lfs/git-lfs"
#GO_INSTALL = "${GO_IMPORT}/git-lfs"

export GO111MODULE="off"

SRC_URI = "git://${GO_IMPORT};protocol=https;branch=release-3.1;destsuffix=${BPN}-${PV}/src/${GO_IMPORT}"
SRC_URI[sha256sum] = "d26d9b0dd35a1e82d35af2c078cacb6178a8953a75948f7e9c0d3f3f57d40b6b"
SRCREV = "a00d0f8e963692e5ca2b79f4c80f3f6345be3a6c"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://src/${GO_IMPORT}/LICENSE.md;md5=3d26ad67cccc4a96ae13e957c57fdc6c"

inherit go

FILES:${PN} += "${GOBIN_FINAL}/*"

RDEPENDS:${PN}-dev += " make"
