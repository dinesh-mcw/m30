#!/bin/bash

set -euo pipefail

# Usage: ./upload_to_artifactory.sh <ARTIFACTORY_USER> <ARTIFACTORY_API_KEY> <ARTIFACTORY_REPO_URL> <BITBUCKET_TAG> <TAR_NAME> <OUTPUT_DIR>

ARTIFACTORY_USER="$1"
ARTIFACTORY_API_KEY="$2"
ARTIFACTORY_REPO_URL="$3"
BITBUCKET_TAG="$4"
TAR_NAME="$5"
OUTPUT_DIR="$6"

# if BITBUCKET_TAG does not exist then assign the environment variable BITBUCKET_BRANCH to BITBUCKET_TAG
if [ -z "$BITBUCKET_TAG" ]; then
  BITBUCKET_TAG="$BITBUCKET_BRANCH"
fi

if [[ $BITBUCKET_TAG =~ ^R[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  RESPONSE=$(curl -u "${ARTIFACTORY_USER}:${ARTIFACTORY_API_KEY}" -T "${TAR_NAME}" "${ARTIFACTORY_REPO_URL}/Release/Candidates/${BITBUCKET_TAG}-$(date +%Y%m%d%H%M%S).tgz")
else
  RESPONSE=$(curl -u "${ARTIFACTORY_USER}:${ARTIFACTORY_API_KEY}" -T "${TAR_NAME}" "${ARTIFACTORY_REPO_URL}/${BITBUCKET_TAG}-$(date +%Y%m%d%H%M%S).tgz")
fi

echo $RESPONSE > "${OUTPUT_DIR}/artifact_upload_response.json"

# Create a manifest file to be used by the system test pipeline
MANIFEST_CONTENT=$(cat <<EOF
{
  "artifactory_url": "${ARTIFACTORY_REPO_URL}/${BITBUCKET_TAG}-$(date +%Y%m%d%H%M%S).tgz", # Corrected URL structure
  "build_stats": {
    "refspec": "${BITBUCKET_TAG}",
    "upload_response": "$RESPONSE"
  }
}
EOF
)
echo "$MANIFEST_CONTENT" > "${OUTPUT_DIR}/system_test_manifest.json"