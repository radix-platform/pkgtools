
# [Pkgtools](https://radix.pro/build-system/pkgtool/)

**Pkgtools** is a set of utilities to create, install, remove, and update
packages on the root file system.

## Table of contents

* [Bootstrap script](#user-content-bootstrap-script)
* [Install](#user-content-install)
* [Configurations](#user-content-configurations)
* [Cross compilation example](#user-content-cross-compilation-example)
* [Dialog](#user-content-dialog)
* [License](#user-content-license)


## Bootstrap script

The bootstrap script aspecialy created for autotools install automation
To install autotools into source directory on build machine (i.e. when
*build == host*) the bootstrap script can be run without arguments. In this
case autotools will be installed from current root file system.

For the cross environment the *--target-dest-dir* options allows to install
some stuf from development root file system:

```Bash
$ TARGET_DEST_DIR=/home/developer/prog/trunk-672/dist/.s9xx-glibc/enybox-x2 \
  ./bootstrap --target-dest-dir=${TARGET_DEST_DIR}
```

For example, in this case the dialog.m4 script will be taken from the
${TARGET_DEST_DIR}/usr/share/aclocal directory.


## Install

On the build machine the installation process seems like that

```Bash
$ tar xJvf pkgtools-0.0.9.tar.xz
$ mkdir build
$ cd build
$ ../pkgtools-0.0.9/configure --prefix=/usr
$ make
$ make install DESTDIR=$PKG exec_prefix=/

Note that the exec_prefix=/ used for canonical installation of
pkgtools utilities into ${DESTDIR}/sbin directory instead of
${DESTDIR}/usr/sbin/ which is not corresponds to FHS.
```


## Configurations

Pkgtools support OpenPGP signing of packages and also simple
user interface based on dialog library.

### OpenPGP support options:

```Bash
  --with-gpg2=no
  --with-gpg2=yes
  --with-gpg2=${TARGET_DEST_DIR}/usr
```

If the *--with-gpg2* option is not specified then dialog support
is disabled.

### Dialog options:

```Bash
  --with-dialog=no
  --with-dialog=yes
  --with-dialog=${TARGET_DEST_DIR}/usr
  --with-dialog-test=no
  --with-dialog-test=yes
```

Dialog support is enabled by default. The option *--with-dialog=no*
disables the dialog support.


### Distribution options:

```Bash
  --with-distro-name[=NAME]        The name of distribution
  --with-distro-version[=VERSION]  The distribution version
```

To show all available options you can make use of

```Bash
$ ./configure --help
```


## Cross compilation example

```Bash
TARGET_DEST_DIR=/home/developer/prog/trunk-672/dist/.s9xx-glibc/enybox-x2
TOOLCHAIN_PATH=/opt/toolchains/aarch64-S9XX-linux-glibc/1.1.4/bin
TARGET=aarch64-s9xx-linux-gnu

DIALOG_CONFIG=${TARGET_DEST_DIR}/usr/bin/dialog-config \
STRIP="${TOOLCHAIN_PATH}/${TARGET}-strip" \
CC="${TOOLCHAIN_PATH}/${TARGET}-gcc --sysroot=${TARGET_DEST_DIR}" \
./configure --prefix=/usr
  --build=x86_64-pc-linux-gnu \
  --host=${TARGET} \
  --with-gpg2=${TARGET_DEST_DIR}/usr \
  --with-dialog=${TARGET_DEST_DIR}/usr \
  --with-dialog-test=yes

Also we can make use of additional variables such as CFLAGS, LDFLAGS:

LDFLAGS="-L${TARGET_DEST_DIR}/lib -L${TARGET_DEST_DIR}/usr/lib"
TARGET_INCPATH="-L${TARGET_DEST_DIR}/usr/include"
CFLAGS="${TARGET_INCPATH}"
CPPFLAGS="${TARGET_INCPATH}"
```


## [Dialog](https://invisible-island.net/dialog/dialog.html)

The original dialog sources have some bugs such as memory leaks and also
the dialog package doesn't have correct autotools scripts. If you want to
use libdialog with pkgtools then you have to install the dialog package
with our [patch](doc/dialog/dialog-1.3-20190211.patch). This patch provides
*dialog.m4* and more convenient *dialog-config* script for
[dialog-1.3-20190211.tgz](ftp://ftp.invisible-island.net/dialog/dialog-1.3-20190211.tgz)
source package.


## [License](https://radix.pro/legal/licenses/)

Code and documentation copyright 2009-2019 Andrey V. Kosteltsev.<br/>
Code and documentation released under [the **Radix.pro** License](LICENSE).

**The text of this license can be found on our website at:**

> [https://radix.pro/licenses/LICENSE-1.0-en_US.txt](https://radix.pro/licenses/LICENSE-1.0-en_US.txt)<br/>
> [https://radix.pro/licenses/LICENSE-1.0-en_US.txt](https://radix.pro/licenses/LICENSE-1.0-ru_RU.txt)
