#!/bin/sh

cd ../..

TARGET_DEST_DIR=/home/kx/prog/scm/svn/platform/trunk-672/dist/.s9xx-glibc/enybox-x2
TOOLCHAIN_PATH=/opt/toolchain/aarch64-S9XX-linux-glibc/1.1.4/bin
TARGET=aarch64-s9xx-linux-gnu

TARGET_INCPATH="-L${TARGET_DEST_DIR}/usr/include"
CFLAGS="${TARGET_INCPATH}"
CPPFLAGS="${TARGET_INCPATH}"
LDFLAGS="-L${TARGET_DEST_DIR}/lib -L${TARGET_DEST_DIR}/usr/lib"


DIALOG_CONFIG=${TARGET_DEST_DIR}/usr/bin/dialog-config \
STRIP="${TOOLCHAIN_PATH}/${TARGET}-strip" \
CC="${TOOLCHAIN_PATH}/${TARGET}-gcc --sysroot=${TARGET_DEST_DIR}" \
./configure --prefix=/usr \
  --build=x86_64-pc-linux-gnu --host=${TARGET} \
  --with-gpg2=${TARGET_DEST_DIR}/usr \
  --with-dialog=${TARGET_DEST_DIR}/usr \
  --with-dialog-test=yes
