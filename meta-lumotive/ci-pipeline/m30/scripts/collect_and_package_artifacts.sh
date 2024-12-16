#!/bin/bash
set -euo pipefail

# Usage: ./collect_and_package_artifacts.sh <IMAGE_DIR> <ARTIFACTS_DIR> <OUTPUT_DIR> <TAR_NAME>

IMAGE_DIR="$1"
ARTIFACTS_DIR="$2"
OUTPUT_DIR="$3"
TAR_NAME="$4"
BUILD_DIR="$5"

cp "${IMAGE_DIR}/Image" "${ARTIFACTS_DIR}/"
cp "${IMAGE_DIR}/imx-boot" "${ARTIFACTS_DIR}/"
cp "${IMAGE_DIR}/imx8qm-mek-m30.dtb" "${ARTIFACTS_DIR}/"
cp "${IMAGE_DIR}/rescue-image-imx8qmmek.ext4.gz" "${ARTIFACTS_DIR}/"
cp "${IMAGE_DIR}/m30-core-image-imx8qmmek.ext4.gz" "${ARTIFACTS_DIR}/"
cp "${IMAGE_DIR}/m30-factory-image-imx8qmmek.wic.zst" "${ARTIFACTS_DIR}/"
cp "${IMAGE_DIR}/m30-core-swu-imx8qmmek.swu" "${ARTIFACTS_DIR}/"
cp "${IMAGE_DIR}/m30-factory-image-imx8qmmek.wic.bmap" "${ARTIFACTS_DIR}/"


# Copy the unittests directory for Cobra System Control
cp -r ${BUILD_DIR}/build/tmp/work/armv8a-poky-linux/python3-cobra-system-control/*/git/unittests ${OUTPUT_DIR}/
tar czvf ${OUTPUT_DIR}/system_control_unittests.tar.gz ${OUTPUT_DIR}/unittests

# Copy the tests directory for Cobra LIDAR API
cp -r ${BUILD_DIR}/build/tmp/work/armv8a-poky-linux/python3-cobra-lidar-api/*/git/tests ${OUTPUT_DIR}/
tar czvf ${OUTPUT_DIR}/lidar_api_tests.tar.gz ${OUTPUT_DIR}/tests

# Remove the unittests and tests directories
rm -rf ${OUTPUT_DIR}/unittests
rm -rf ${OUTPUT_DIR}/tests

# Download the specific version of pytest_random_order package
PACKAGE="pytest_random_order"
VERSION="1.1.1"

# Use pip to download the specific version of the package without its dependencies
pip download "${PACKAGE}==${VERSION}" --no-deps --no-binary :all: -d ${OUTPUT_DIR}/ && mv $OUTPUT_DIR/pytest-random-order-${VERSION}.tar.gz ${OUTPUT_DIR}/build_artifacts

# Copy all files in OUTPUT_DIR to build_artifacts excluding the build_artifacts directory in one command
rsync -a --exclude=build_artifacts ${OUTPUT_DIR}/ ${OUTPUT_DIR}/build_artifacts

# Create a tarball of the build_artifacts directory
tar czvf ${TAR_NAME} -C ${OUTPUT_DIR} build_artifacts