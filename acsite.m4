
dnl ============================================================
dnl Display Configuration Headers
dnl 
dnl    configure.in:
dnl       AC_MSG_CFG_PART(<text>)
dnl ============================================================

define(AC_MSG_CFG_PART,[dnl
  AC_MSG_RESULT()
  AC_MSG_RESULT(${TB}$1:${TN})
])dnl

AC_DEFUN(AC_PKGTOOLS_HEADLINE,[dnl
  # Use a `Quadrigaph'. @<:@ gives you [ and @:>@ gives you ] :
  TB=`echo -n -e '\033@<:@1m'`
  TN=`echo -n -e '\033@<:@0m'`
  echo ""
  echo "Configuring ${TB}$1${TN} ($2), Version ${TB}${PACKAGE_VERSION}${TN}"
  echo "$3"
])dnl


dnl ============================================================
dnl  Test for build_host `ln -s' .
dnl  ============================
dnl
dnl Usage:
dnl -----
dnl    AC_PATH_PROG_LN_S
dnl    AC_SUBST(LN)
dnl    AC_SUBST(LN_S)
dnl
dnl ============================================================
AC_DEFUN(AC_PATH_PROG_LN_S,
[AC_PATH_PROG(LN, ln, no, /usr/local/bin:/usr/bin:/bin:$PATH)
AC_MSG_CHECKING(whether ln -s works on build host)
AC_CACHE_VAL(ac_cv_path_prog_LN_S,
[rm -f conftestdata
if $LN -s X conftestdata 2>/dev/null
then
  rm -f conftestdata
  ac_cv_path_prog_LN_S="$LN -s"
else
  ac_cv_path_prog_LN_S="$LN"
fi])dnl
LN_S="$ac_cv_path_prog_LN_S"
if test "$ac_cv_path_prog_LN_S" = "$LN -s"; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi
AC_SUBST(LN)dnl
AC_SUBST(LN_S)dnl
])

