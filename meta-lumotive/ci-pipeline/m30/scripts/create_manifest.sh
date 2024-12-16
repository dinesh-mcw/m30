#!/bin/bash
set -eo pipefail

# Usage: ./create_manifest.sh <IMAGE_DIR> <ARTIFACTS_DIR>

IMAGE_DIR="$1"
ARTIFACTS_DIR="$2"

# Create the manifest xml file if it does not exist
if [ ! -f "${ARTIFACTS_DIR}/m30-image-manifest.xml" ]; then
    touch "${ARTIFACTS_DIR}/m30-image-manifest.xml"
fi

echo "<manifest>" > "${ARTIFACTS_DIR}/m30-image-manifest.xml"
grep -A 1 "cobra-raw2depth armv8a" "${IMAGE_DIR}/m30-core-image-imx8qmmek.manifest" >> "${ARTIFACTS_DIR}/m30-image-manifest.xml"
grep -A 1 "python3-cobra-system-control armv8a" "${IMAGE_DIR}/m30-core-image-imx8qmmek.manifest" >> "${ARTIFACTS_DIR}/m30-image-manifest.xml"
grep -A 1 "python3-cobra-lidar-api armv8a" "${IMAGE_DIR}/m30-core-image-imx8qmmek.manifest" >> "${ARTIFACTS_DIR}/m30-image-manifest.xml"
echo "</manifest>" >> "${ARTIFACTS_DIR}/m30-image-manifest.xml"
