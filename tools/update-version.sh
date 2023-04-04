#!/bin/sh
set -e
# Shell script to update HDR_HISTOGRAM_VERSION_H to a specific version

BASE_DIR=$(cd "$(dirname "$0")/.." && pwd)
HDR_DIR="$BASE_DIR/include/hdr"
HDR_HISTOGRAM_VERSION_H=$HDR_DIR/hdr_histogram_version.h
NEW_VERSION=$1

if [ "$#" -le 0 ]; then
  echo "Error: please provide a version to update to"
  exit 1
fi

CURRENT_VERSION=$(grep "#define HDR_HISTOGRAM_VERSION_H" $HDR_HISTOGRAM_VERSION_H | sed -n "s/^.*VERSION_H \"\(.*\)\"/\1/p")

echo "Comparing $NEW_VERSION with $CURRENT_VERSION"

if [ "$NEW_VERSION" = "$CURRENT_VERSION" ]; then
  echo "Skipped because is already on the latest version."
  exit 0
fi

echo "Replacing $CURRENT_VERSION with new version $NEW_VERSION"

sed -i '' -e "s/#define HDR_HISTOGRAM_VERSION_H \"$CURRENT_VERSION\"/#define HDR_HISTOGRAM_VERSION_H \"$NEW_VERSION\"/g" $HDR_HISTOGRAM_VERSION_H

echo "All done!"

echo "NEW_VERSION=$NEW_VERSION"
