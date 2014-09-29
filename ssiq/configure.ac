AC_INIT([SSIQ], [1], [https://github.com/dubhater/vapoursynth-ssiq/issues], [ssiq], [https://github.com/dubhater/vapoursynth-ssiq/])

: ${CFLAGS="-O2"}

AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-xz subdir-objects no-define])
AM_SILENT_RULES([yes])

LT_INIT([disable-static win32-dll])

AC_PROG_CC

AC_SEARCH_LIBS([sqrt], [m], [], [
   AC_MSG_ERROR([unable to find the sqrt() function])
])

AC_CANONICAL_HOST

AS_CASE(
   [$host_cpu], [i?86],
   [AS_CASE(
      [$host_os], [cygwin*|mingw*],
      [LDFLAGS="-Wl,--kill-at"]
   )]
)

PKG_CHECK_MODULES([VapourSynth], [vapoursynth])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
