#!/bin/sh

cd ../..

./configure --prefix=/usr   \
  --with-distro-name=radix  \
  --with-distro-version=1.1 \
  --with-gpg2=yes   \
  --with-dialog=yes
