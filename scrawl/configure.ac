# stolen from Aegisub
AC_DEFUN([AC_CXX_FLAG], [{
         AC_LANG_PUSH(C++)
         ac_cxx_flag_save="$CXXFLAGS"
         CXXFLAGS="$CXXFLAGS $1"
         AC_MSG_CHECKING([[whether $CXX supports $1]])
         AC_COMPILE_IFELSE(
                           [AC_LANG_PROGRAM([[]])],
                           [AC_MSG_RESULT([yes])],
                           [
                            CXXFLAGS="$ac_cxx_flag_save"
                            AC_MSG_RESULT([no])
                            $2
                            ])
         AC_LANG_POP(C++)
         }])


AC_INIT([Scrawl], [1], [https://github.com/dubhater/vapoursynth-scrawl/issues], [scrawl], [https://github.com/dubhater/vapoursynth-scrawl/])

AC_LANG([C++])

: ${CXXFLAGS="-O2"}

AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-xz subdir-objects no-define])
AM_SILENT_RULES([yes])

LT_INIT([disable-static win32-dll])

AC_PROG_CXX

AC_CXX_FLAG([-std=c++11])

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
