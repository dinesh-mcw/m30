# Lumotive Yocto Meta Layer
### Collection of packages and board support for Lumotive control boards

<br>

![Lumotive](https://lumotive.com/wp-content/uploads/2024/02/Lumotive_horizontal_logo_fullcolor_500x82.png)

<br>

![](https://img.shields.io/badge/compat-kirkstone-blue)

This README file contains information on the contents of the meta-lumotive layer.

Please see the corresponding sections below for instructions on how to build
for specific hardware.

# Prerequisites

You will need to add your workstation's SSH key in your git repository service
account. Generate an SSH key with `ssh-keygen`. The SSH public key will be
located at `$HOME/.ssh/id_rsa.pub`. Upload the public key to your account under
`Settings -> SSH and GPG keys`. Click "New SSH key", give it an optional title,
select "Authentication Key" and paste the public key, then save it.

Your build machine must be configured with either of the following RAM
configurations:

- 16GB+ RAM, 8GB+ swap
- 24GB+ RAM

# Building for imx8qmmek

The following instructions are for building meta-lumotive for the imx8qmmek
machine type.

## Dependencies

```
sudo apt install gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect xz-utils debianutils \
    iputils-ping python3-git python3-jinja2 libegl1-mesa libsdl1.2-dev pylint3 \
    xterm python3-subunit mesa-common-dev zstd liblz4-tool
```

## Repo tool

Obtain the Google `repo` tool:

```
mkdir -p ~/.bin
PATH="${HOME}/.bin:${PATH}"
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/.bin/repo
chmod a+rx ~/.bin/repo
```

Optional: Run the following command to allow `repo` to work in every new shell
automatically

```
echo 'export PATH="${HOME}/.bin:${PATH}"' >> ~/.bashrc
```

## Setup

### Setup build directory

1. Create workdir

    ```
    mkdir <builddir> && cd <builddir>
    ```

2. Initialize meta-lumotive manifest

    ```
    repo init -u ssh://git@bitbucket.org/lumotive/meta-lumotive.git -m manifests/imx-5.15.52-cobra.xml
    ```

3. Install NXP manifest

    ```
    mkdir -p .repo/local_manifests
    wget --directory-prefix .repo/local_manifests https://raw.githubusercontent.com/nxp-imx/imx-manifest/imx-linux-kirkstone/imx-5.15.52-2.1.0.xml
    ```

4. Sync all repos

    ```
    repo sync
    ```

### Sign the image

The process of creating a passphrase file, and then generating a public and
private key pair using the passphrase file, is an essential step to ensure the
security of the software update mechanism. The private key is used to sign the
software updates, and the public key is used to verify the signature on the
device.

1. Generate a private key using the passphrase file `<builddir>/sources/meta-lumotive/recipes-core/images/m30-core-swu/swupdate.priv.pem.pass`

    ```
    openssl genrsa -aes256 -passout file:swupdate.priv.pem.pass -out swupdate.priv.pem 4096
    ```

    Optional: Create your own secure passphrase that you will use to generate
    the private key

    ```
    openssl rand -base64 32 > <YOURNAMEHERE>.priv.pem.pass
    chmod 600 <YOURNAMEHERE>.priv.pem.pass
    ```

2. Generate a public key from the private key

    ```
    openssl rsa -in swupdate.priv.pem -passin file:swupdate.priv.pem.pass -outform PEM -pubout -out swupdate.pub.pem
    ```

3. Install the keys to the appropriate directory

    1. Install the public key to the swupdate directory

        ```
        mv swupdate.pub.pem sources/meta-lumotive/recipes-support/swupdate/swupdate
        ```

    2. Install the private key to the m30-core-swu directory

        ```
        mv swupdate.priv.pem sources/meta-lumotive/recipes-core/images/m30-core-swu
        ```

    3. Optional: If you generated a private key passphrase, install it to the m30-core-swu directory

        ```
        mv swupdate.priv.pem.pass sources/meta-lumotive/recipes-core/images/m30-core-swu
        ```

`Note:` To sign the code you need `swupdate.priv.pem` (must be securely
stored and copied to the above mentioned location from an external source) and
`swupdate.priv.pem.pass` (used to decrypt the `swupdate.priv.pem`).
These files can be tracked in version control and stored with the image files.
Both files are required to successfully sign the code. But they should NEVER be
stored together and either one must be copied only during build. For more
information on security and signing images in yocto, please see:
[Update images from verified source](https://sbabic.github.io/swupdate/signed_images.html)

## Build the image

1. Initialize the build environment

    ```
    source sources/poky/oe-init-build-env build
    ```

2. Build the image

    ```
    bitbake -k rescue-image m30-core-swu
    ```

    The `m30-core-swu` image depends on `m30-core-image` and will automatically
    build it.

### Build artifacts

#### Images
Compiled images are stored under `build/tmp/deploy/images/imx8qmmek/`

| Artifact                            | Description                              |
|-------------------------------------|------------------------------------------|
| rescue-image-imx8qmmek.ext4.gz      | Rescue image                             |
| m30-core-image-imx8qmmek.ext4.gz    | Base rootfs image                        |
| m30-factory-image-imx8qmmek.wic.zst | Partitioned image with rescue and rootfs |
| m30-core-swu-imx8qmmek.swu          | SWUpdate image (for field upgrades)      |
| Image                               | kernel binary (uncompressed 64-bit)      |
| imx-boot                            | bootloader binary                        |
| imx8qm-mek-m30.dtb                  | Device tree blob                         |

#### Packages
Each application is packaged into a .deb package and is available under
`build/tmp/deploy/deb/`

#### Licenses
License manifests are generated per package under `build/tmp/deploy/licenses`
with the global list of all licenses included in the build in
`build/tmp/deploy/licenses/m30-core-image-imx8qmmek/license.manifest`

## Factory Programming

`uuu` is a utility for your workstation that handles the USB OTG programming
for NXP devices.

Some basic documentation is available on the
[wiki](https://github.com/nxp-imx/mfgtools/wiki).

Download uuu version >= 1.5.165, Linux and Windows versions are available:

https://github.com/nxp-imx/mfgtools/releases/tag/uuu_1.5.165

Optional: Configure udev rules to allow `uuu` to run without `sudo`

```
sudo sh -c "uuu -udev >> /etc/udev/rules.d/70-uuu.rules"
sudo udevadm control --reload
```

### Programming steps

1. If this is not a brand new NCB, put a jumper on J4 on the NCB and power
   cycle the NCB. A soft reset/reboot is not sufficient.

2. Connect a Micro-USB to USB-A cable from J25 on the NCB to your laptop with
   `uuu`.

3. Verify you have connectivity using `uuu -lsusb`. You should see output like this:

    ```
    $ uuu -lsusb
    uuu (Universal Update Utility) for nxp imx chips -- libuuu_1.5.165-0-g7347a80

    Connected Known USB Devices
            Path     Chip    Pro     Vid     Pid     BcdVersion
            ==================================================
            2:833    MX8QM   SDPS:   0x1FC9 0x0129   0x0002
    ```

4. Build the factory image.

    ```
    bitbake -k m30-factory-image
    ```

5. Ensure that you have both `m30-factory-image-imx8qmmek.wic.zst` and
   `m30-factory-image-imx8qmmek.wic.bmap` together in the same directory, the -bamp
   option will automatically find the .bmap file.

6. Flash the factory image with:

    ```
    uuu -bmap -b emmc_all imx-boot-tagged m30-factory-image-imx8qmmek.wic.zst
    ```

7. Once programming is complete, remove the jumper on J4 if you installed it earlier.

8. Power cycle the NCB.

# Kernel configuration with config fragments (preferred)

Kernel config fragments are a great way to configure the kernel without needing
to use cumbersome, non-portable patches.

1. Prepare for the kernel configuration (take a snapshot)

    `bitbake -c kernel_configme virtual/kernel`

2. Edit the configuration (do your modifications)

    `bitbake -c menuconfig virtual/kernel`

3. Save the configuration differences (extract the differences)

    `bitbake -c diffconfig virtual/kernel`

The differences will be saved in **$WORKDIR/fragment.cfg**

Copy the **$WORKDIR/fragment.cfg** into your layer directory and add it in your
recipe or bbappend as in the example below.

### Examples

This is an example of the files involved in this operation

```
meta-lumotive
└── recipes-kernel
    └── linux
        ├── linux-imx
        │   └── enable-kernel-feature.cfg
        └── linux-imx_%.bbappend
```

Content of **linux-imx_%.bbappend**

```
FILESEXTRAPATHS:prepend := "${THISDIR}/linux-imx:"
SRC_URI:append = "file://enable-kernel-feature.cfg"
```

Content of linux-imx/enable-kernel-feature.cfg

```
CONFIG_SOUND=y
# CONFIG_USB_AUDIO is not set
# CONFIG_USB_MIDI_GADGET is not set
```

*Note: After integrating configuration fragments into the appended recipe, you
can check everything is fine by running*

`bitbake -c kernel_configcheck -f virtual/kernel`

If everything is fine, it will exit without errors and the kernel .config will
be reset to before your changes. Then you can run the following command to
rebuild the kernel:

`bitbake virtual/kernel`

Now the kernel .config will be updated with your changes from the config fragment.

# Patching recipes

Patching source code should only be done as a last resort as it creates a direct
dependency on the specific version of the recipe and makes it difficult to
upgrade the recipe in the future. More often than not there is a more elegant
solution that does not require patching, such as the kernel config fragment
example above or using built-in features like systemd's drop-in config feature.

### Simple example

If patching is necessary, it can be done using the following steps:

1. Checkout the recipe source code into `build/workspace/sources`

    ```
    devtool modify <recipe>
    ```

2. Make your changes in the new directory created in `build/workspace/sources`

3. Stage changes and commit (note: your message will become the patch file name)

    ```
    git add <file1> <file2> ...
    git commit -m "short descriptive message about the change"
    ```

4. Change directory back to build directory (if not already your current
   working directory), update the recipe and generate patch files

    ```
    devtool update-recipe -a ../sources/meta-lumotive/ <recipe>
    ```

5. Patch files and an updated bbappend will be generated in the recipe's
   directory in the meta-lumotive layer, remove any unnecessary patches that may
   be generated along with it

6. Reset the recipe and delete the checked out source code

    ```
    devtool reset <recipe>
    rm -rf build/workspace/sources/<recipe>
    ```
