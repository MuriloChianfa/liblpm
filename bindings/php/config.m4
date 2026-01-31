dnl config.m4 for liblpm PHP extension
dnl
dnl High-performance PHP bindings for the liblpm C library.
dnl Provides IPv4 and IPv6 longest prefix match operations.
dnl
dnl Copyright (c) Murilo Chianfa
dnl Licensed under the Boost Software License 1.0

PHP_ARG_WITH([liblpm],
  [for liblpm support],
  [AS_HELP_STRING([--with-liblpm@<:@=DIR@:>@],
    [Include liblpm support. DIR is the prefix to liblpm installation directory.])])

if test "$PHP_LIBLPM" != "no"; then
  dnl Check for PHP version
  PHP_VERSION_ID=`$PHP_CONFIG --vernum`
  if test "$PHP_VERSION_ID" -lt "80100"; then
    AC_MSG_ERROR([liblpm extension requires PHP 8.1.0 or newer])
  fi

  dnl Search for liblpm headers and library
  SEARCH_PATH="/usr/local /usr /opt/local /opt"
  SEARCH_FOR="/include/lpm/lpm.h"

  if test -r $PHP_LIBLPM/$SEARCH_FOR; then
    LIBLPM_DIR=$PHP_LIBLPM
  else
    AC_MSG_CHECKING([for liblpm files in default path])
    for i in $SEARCH_PATH; do
      if test -r $i/$SEARCH_FOR; then
        LIBLPM_DIR=$i
        AC_MSG_RESULT(found in $i)
        break
      fi
    done
  fi

  if test -z "$LIBLPM_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall liblpm. Cannot find lpm/lpm.h header file.])
  fi

  dnl Check for liblpm library
  PHP_CHECK_LIBRARY(lpm, lpm_create_ipv4,
  [
    PHP_ADD_LIBRARY_WITH_PATH(lpm, $LIBLPM_DIR/$PHP_LIBDIR, LIBLPM_SHARED_LIBADD)
    AC_DEFINE(HAVE_LIBLPM, 1, [Whether you have liblpm])
  ],[
    AC_MSG_ERROR([liblpm library not found or wrong version. Please reinstall liblpm.])
  ],[
    -L$LIBLPM_DIR/$PHP_LIBDIR -lm
  ])

  dnl Add include path
  PHP_ADD_INCLUDE($LIBLPM_DIR/include)

  dnl Check for required functions
  PHP_CHECK_LIBRARY(lpm, lpm_create_ipv4_dir24,
  [
    AC_DEFINE(HAVE_LPM_DIR24, 1, [Whether liblpm has DIR-24-8 support])
  ],[
    AC_MSG_WARN([DIR-24-8 algorithm not available in this liblpm version])
  ],[
    -L$LIBLPM_DIR/$PHP_LIBDIR
  ])

  PHP_CHECK_LIBRARY(lpm, lpm_create_ipv6_wide16,
  [
    AC_DEFINE(HAVE_LPM_WIDE16, 1, [Whether liblpm has Wide-16 support])
  ],[
    AC_MSG_WARN([Wide-16 algorithm not available in this liblpm version])
  ],[
    -L$LIBLPM_DIR/$PHP_LIBDIR
  ])

  dnl Define the extension
  PHP_SUBST(LIBLPM_SHARED_LIBADD)
  
  PHP_NEW_EXTENSION(liblpm, liblpm.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi
