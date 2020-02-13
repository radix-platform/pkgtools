
dnl ============================================================
dnl  Process this file with autoconf to produce
dnl  a configure script.
dnl ============================================================

AC_PREREQ(2.69)dnl        dnl Minimum Autoconf version required.


AC_INIT([pkgtools], [0.1.5],
  [support@radix.pro], [pkgtools], [https://radix.pro])

dnl ============================================================
dnl m4's diversions:
dnl ---------------
dnl
dnl see: /use/share/autoconf/autoconf/general.m4
dnl ============================================================
m4_divert_push([M4SH-INIT])
DISTRO_NAME=radix
DISTRO_CAPTION=Radix
DISTRO_VERSION=1.1
DISTRO_LICENSE=Radix-1.0
m4_divert_pop([M4SH-INIT])

AC_PKGTOOLS_HEADLINE([pkgtools], dnl
  [Package Tools], [Copyright (c) 2009-2019 Andrey V.Kosteltsev])


dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl $$                                                        $$
dnl $$ PART: Init Automake environment                        $$
dnl $$                                                        $$
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
AC_MSG_CFG_PART(Init Automake environment)

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([subdir-objects foreign no-dist-gzip dist-xz])

AM_CONFIG_HEADER([config.h])

AC_PREFIX_DEFAULT(/usr/local)


dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl $$                                                        $$
dnl $$ PART: Test for Build Tools                             $$
dnl $$                                                        $$
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
AC_MSG_CFG_PART(Test for build tools)
AC_CHECK_TOOL([GCC], [gcc], [:])


dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl $$                                                        $$
dnl $$ PART: Test for Libraries                               $$
dnl $$                                                        $$
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
AC_MSG_CFG_PART(Test for libraries)
AC_CHECK_DIALOG([1.3.20190211],yes,yes,yes,CFLAGS="$CFLAGS -DHAVE_DIALOG")
AM_CONDITIONAL([USE_DIALOG], [test "x$HAVE_DIALOG" = "x1"])


dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl $$                                                        $$
dnl $$ PART: Test for Runtime Tools                           $$
dnl $$                                                        $$
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
AC_MSG_CFG_PART(Test for runtime tools)

dnl ============================================================
dnl Check for GPG2 utility:
dnl ----------------------
dnl Usage:
dnl   not defined --with-gpg2            - Disable OpenPGP support by default
dnl   --with-gpg2=no                     - Disable OpenPGP support by --with-gpg2=no option
dnl   --with-gpg2, --with-gpg2=yes       - Check whether the gpg2 program exists in path on the build machine
dnl   --with-gpg2=${TARGET_DEST_DIR}/usr - Check whether the gpg2 program already installed on the target rootfs
dnl ============================================================
AC_ARG_WITH([gpg2], [AS_HELP_STRING([--with-gpg2],
  [support OpenPGP signatures (default=no)])],
  [GPG2=
   AS_IF([test "x$with_gpg2" != "xno"],
     [AS_IF([test "x$with_gpg2" != "xyes"],
       [AC_PATH_PROG([GPG2], [gpg2], [no], [${with_gpg2}/bin])],
       [AC_PATH_PROG([GPG2], [gpg2], [no], [/usr/local/bin:/usr/bin:/bin:$PATH])])
     ],
     [AC_MSG_CHECKING(for gpg2)
      AC_MSG_RESULT([OpenPGP disabled by the --with-gpg2=no option])
      AC_SUBST(GPG2, [no])
     ])
   if test "x$GPG2" != "xno" ; then
     AC_DEFINE([HAVE_GPG2], [1], [Define if you have OpenPGP program])
   else
     if test "x$with_gpg2" != "xno" ; then
       AC_MSG_FAILURE([--with-gpg2 was given, but test for gpg2 program failed], [1])
     fi
   fi
  ],
  [AC_MSG_CHECKING(for gpg2)
   AC_MSG_RESULT([OpenPGP disabled by default])
   AC_SUBST(GPG2, [no])
  ])

dnl ============================================================
dnl m4's diversions:
dnl ---------------
dnl
dnl see: /use/share/autoconf/autoconf/general.m4
dnl ============================================================
m4_divert_push([HELP_WITH])dnl
_ACEOF

  cat <<_ACEOF

Distribution Features:
m4_divert_pop([HELP_WITH])dnl

AC_ARG_WITH([distro-name], [AS_HELP_STRING([--with-distro-name@<:@=NAME@:>@],
  [Distribution Name @<:@default=${DISTRO_NAME}@:>@. Please note that the distribution NAME should not contain spaces],[30],[74])],
  [AS_IF([test "x$with_distro_name" != "x"],
     [DISTRO_NAME=${with_distro_name}
      DISTRO_CAPTION=`echo ${with_distro_name:0:1} | tr '[a-z]' '[A-Z]'`${with_distro_name:1}
      AC_DEFINE_UNQUOTED([DISTRO_NAME], ["$with_distro_name"], [Define the distribution name])
      AC_DEFINE_UNQUOTED([DISTRO_CAPTION], "$DISTRO_CAPTION", [Define the caption of the distribution])
     ],
     [AC_DEFINE_UNQUOTED([DISTRO_NAME], ["$DISTRO_NAME"], [Define the distribution name])
      AC_DEFINE_UNQUOTED([DISTRO_CAPTION], ["$DISTRO_CAPTION"], [Define the caption of the distribution])
     ])
  ],
  [AC_DEFINE_UNQUOTED([DISTRO_NAME], ["$DISTRO_NAME"], [Define the distribution name])
   AC_DEFINE_UNQUOTED([DISTRO_CAPTION], ["$DISTRO_CAPTION"], [Define the caption of the distribution])
  ])

AC_ARG_WITH([distro-version], [AS_HELP_STRING([--with-distro-version@<:@=VERSION@:>@],
  [Distribution Version @<:@default=${DISTRO_VERSION}@:>@],[30],[79])],
  [AS_IF([test "x$with_distro_version" != "x"],
     [AC_DEFINE_UNQUOTED([DISTRO_VERSION], ["$with_distro_version"], [Define the version of distribution])
     ],
     [AC_DEFINE_UNQUOTED([DISTRO_VERSION], ["$DISTRO_VERSION"], [Define the version of distribution])
     ])
  ],
  [AC_DEFINE_UNQUOTED([DISTRO_VERSION], ["$DISTRO_VERSION"], [Define the version of distribution])
  ])

m4_divert_push([HELP_WITH])dnl
_ACEOF

  cat <<\_ACEOF
m4_divert_pop([HELP_WITH])dnl

AC_SUBST(DISTRO_NAME)
AC_SUBST(DISTRO_CAPTION)
AC_SUBST(DISTRO_VERSION)
AC_SUBST(DISTRO_URL,[${PACKAGE_URL}])
AC_SUBST(DISTRO_LICENSE,[${DISTRO_LICENSE}])
AC_SUBST(PROGRAM_VERSION,[${PACKAGE_VERSION}])

AC_DEFINE_UNQUOTED([DISTRO_URL], ["$DISTRO_URL"], [Define the bug report URL])
AC_DEFINE_UNQUOTED([DISTRO_LICENSE], ["$DISTRO_LICENSE"], [Define the bug report URL])
AC_DEFINE_UNQUOTED([PROGRAM_VERSION], ["$PROGRAM_VERSION"], [Define the version of all programs in this package])


dnl ============================================================
dnl Environment Variables:
dnl ---------------------
dnl  For 'Some influential environment variables:' help section
dnl ============================================================
AC_ARG_VAR([STRIP], [strip command])


dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl $$                                                        $$
dnl $$ PART: Test for Auxiliary (my be version sensitive)     $$
dnl $$       programs                                         $$
dnl $$                                                        $$
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
AC_MSG_CFG_PART(Test for aux programs)
AC_PATH_PROG_LN_S
AC_PATH_PROG([TAR],       [tar],       [no], [/usr/local/bin:/usr/bin:/bin:$PATH])
AC_PATH_PROG([SED],       [sed],       [no], [/usr/local/bin:/usr/bin:/bin:$PATH])
AC_PATH_PROG([FIND],      [find],      [no], [/usr/local/bin:/usr/bin:/bin:$PATH])
AC_PATH_PROG([CAT],       [cat],       [no], [/usr/local/bin:/usr/bin:/bin:$PATH])
AC_PATH_PROG([CP],        [cp],        [no], [/usr/local/bin:/usr/bin:/bin:$PATH])
AC_PATH_PROG([MV],        [mv],        [no], [/usr/local/bin:/usr/bin:/bin:$PATH])
AC_PATH_PROG([RM],        [rm],        [no], [/usr/local/bin:/usr/bin:/bin:$PATH])
AC_PATH_PROG([CHMOD],     [chmod],     [no], [/usr/local/bin:/usr/bin:/bin:$PATH])
AC_PATH_PROG([SHA256SUM], [sha256sum], [no], [/usr/local/bin:/usr/bin:/bin:$PATH])


dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl $$                                                        $$
dnl $$ PART: OUTPUT Substitution                              $$
dnl $$                                                        $$
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
dnl ============================================================
AC_MSG_CFG_PART(OUTPUT)

AC_CONFIG_FILES([
src/Makefile
Makefile
])
AC_OUTPUT
