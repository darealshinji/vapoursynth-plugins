AC_INIT([Videoscope], [1], [https://github.com/dubhater/vapoursynth-videoscope/issues], [videoscope], [https://github.com/dubhater/vapoursynth-videoscope/])

: ${CFLAGS="-O2"}

AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-xz subdir-objects no-define])
AM_SILENT_RULES([yes])

LT_INIT([disable-static win32-dll])

AC_PROG_CC

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
