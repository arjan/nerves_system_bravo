#!/bin/sh

set -e

FWUP_CONFIG=$NERVES_DEFCONFIG_DIR/fwup.conf

# Run the common post-image processing for nerves
$BR2_EXTERNAL_NERVES_PATH/board/nerves-common/post-createfs.sh $TARGET_DIR $FWUP_CONFIG

# Keep the Android kernel and tools with the images so that scripts/bravo-image
# only depends on the system artifact.
cp -f $NERVES_DEFCONFIG_DIR/board/boot-stock.img $BINARIES_DIR/ 2>/dev/null || true
cp -rf $NERVES_DEFCONFIG_DIR/scripts $BINARIES_DIR/
