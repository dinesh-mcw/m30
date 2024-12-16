SUMMARY = "M30 demo browser UI"
DESCRIPTION = "M30 nodejs source code for demo browser UI"
LICENSE = "CLOSED"

require cobra-gui.inc

# Depends on nodejs 16.14.2
DEPENDS = "nodejs-native"

PV = "1.0+git${SRCPV}"

BRANCH_COBRA_LIDAR_API ?= "master"
SRCREV_COBRA_LIDAR_API ?= "db55b3747bbb8ef2c4ab316f710c10706d93ba32"

SRCREV = "${SRCREV_COBRA_LIDAR_API}"
SRC_URI = "git://git@bitbucket.org/lumotive/cobra_lidar_api.git;protocol=ssh;branch=${BRANCH_COBRA_LIDAR_API}"

NPM_URI = " \
    npmsw://${S}/package-lock.json \
"

S = "${WORKDIR}/git"

# Necessary to work around an issue with npm failing to build
export OPENSSL_MODULES="${STAGING_LIBDIR_NATIVE}/ossl-modules"
export NODE_OPTIONS="--openssl-legacy-provider"

INSTALL_DIR = "${libdir}/python3.10/site-packages/cobra_lidar_api/m30_webapp"
FILES:${PN}:append = " ${INSTALL_DIR}/*"

# Fetch npm dependencies from the shrinkwrap file after git fetch is done
do_unpack[network] = "1"
do_unpack:append() {
    npm_uri = (d.getVar('NPM_URI') or "").split()
    if len(npm_uri) == 0:
        return
    try:
        fetcher = bb.fetch2.Fetch(npm_uri, d)
        fetcher.download()                     # Fetch
        fetcher.unpack(d.getVar('WORKDIR'))    # Unpack
    except bb.fetch2.BBFetchException as e:
        bb.fatal(str(e))
}

do_compile[network] = "1"
do_compile() {
    cd ${S}

    # changing the home directory to the working directory, the .npmrc will be created in this directory
    export HOME=${WORKDIR}

    npm --verbose install
    npm run build
}

do_install() {
    install -d ${D}${INSTALL_DIR}
    cp -r ${S}/cobra_lidar_api/m30_webapp/. ${D}${INSTALL_DIR}
}
