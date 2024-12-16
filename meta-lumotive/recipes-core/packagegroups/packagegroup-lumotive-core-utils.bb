SUMMARY = "Core utilities common to all Lumotive sensor types"

PACKAGE_ARCH = "${MACHINE_ARCH}"

inherit packagegroup

PROVIDES = "${PACKAGES}"
PACKAGES = "\
     packagegroup-lumotive-core-utils \
     packagegroup-lumotive-extra-python \
"

RDEPENDS:packagegroup-lumotive-core-utils = " \
    cmake \
    git \
    git-lfs \
    googletest \
    htop \
    i2c-tools \
    man \
    repo \
    stress \
    tmux \
    u-boot-fw-utils \
    u-boot-markgoodboot \
    vim \
"

RDEPENDS:packagegroup-lumotive-extra-python = " \
    python3-aniso8601 \
    python3-attrs \
    python3-bitstring \
    python3-certifi \
    python3-charset-normalizer \
    python3-click \
    python3-cycler \
    python3-dateutil \
    python3-flask \
    python3-flask-cors \
    python3-flask-httpauth \
    python3-flask-restful \
    python3-gunicorn \
    python3-idna \
    python3-importlib-metadata \
    python3-importlib-resources \
    python3-iniconfig \
    python3-intelhex \
    python3-itsdangerous \
    python3-jinja2 \
    python3-markupsafe \
    python3-marshmallow \
    python3-numpy \
    python3-packaging \
    python3-pandas \
    python3-parameterized \
    python3-peewee \
    python3-pillow \
    python3-pip \
    python3-pkg-about \
    python3-pybind11 \
    python3-pycryptodomex \
    python3-pyparsing \
    python3-pyro5 \
    python3-pytest \
    python3-pytest-random-order \
    python3-tomli \
    python3-pytz \
    python3-pyyaml \
    python3-pyzipper \
    python3-requests \
    python3-serpent \
    python3-setuptools \
    python3-six \
    python3-toml \
    python3-typing-extensions \
    python3-urllib3 \
    python3-werkzeug \
    python3-zipp \
"
