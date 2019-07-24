#!/bin/sh

TARGET_DEST_DIR=/home/kx/prog/scm/svn/platform/trunk-672/dist/.s9xx-glibc/enybox-x2

cd ../..
./bootstrap --target-dest-dir=${TARGET_DEST_DIR}
