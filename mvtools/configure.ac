AC_INIT([MVTools], [1],
        [https://github.com/dubhater/vapoursynth-mvtools/issues],
        [MVTools], [https://github.com/dubhater/vapoursynth-mvtools/])

: ${CXXFLAGS="-O2"}

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-xz subdir-objects no-define])
AM_SILENT_RULES([yes])

LT_INIT([disable-static win32-dll])

AC_PROG_CXX
AC_PROG_CC

AC_CANONICAL_HOST

if test "$lt_cv_file_magic_cmd" = "func_win32_libid" ; then
    deplibs_check_method='file_magic file format pei*-(i386|x86-64)|(.*architecture: i386)?'
    file_magic_cmd='$OBJDUMP -f'
fi

AM_CONDITIONAL([x86_64], [test "x$host_cpu" = "xx86_64"])

AC_CHECK_PROGS([YASM], [yasm])
AS_IF(
   [test "x$YASM" = "x"],
   [AC_MSG_ERROR([yasm required but not found])],
   [AS="$YASM"]
)

ASFLAGS="$ASFLAGS -Dprivate_prefix=mvtools -DHIGH_BIT_DEPTH=0 -DBIT_DEPTH=8"

AS_CASE(
  [$host_cpu],
  [i?86],   [BITS="32" ASFLAGS="$ASFLAGS -DARCH_X86_64=0"],
  [x86_64], [BITS="64" ASFLAGS="$ASFLAGS -DARCH_X86_64=1 -DPIC -m amd64"],
  [AC_MSG_ERROR([Currently MVTools can only be compiled for Intel CPUs.])]
)

AS_CASE(
   [$host_os],
   [darwin*],
   [
      ASFLAGS="$ASFLAGS -f macho$BITS -DPREFIX"
   ],
   [*linux*],
   [
      ASFLAGS="$ASFLAGS -f elf"
   ],
   [gnu*],
   [
      ASFLAGS="$ASFLAGS -f elf"
   ],
   [cygwin*],
   [
      ASFLAGS="$ASFLAGS -f win32"
      AS_IF(
         [test "x$BITS" = "x32"],
         [ASFLAGS="$ASFLAGS -DPREFIX" LDFLAGS="-Wl,--kill-at"]
      )
   ],
   [mingw*],
   [
      ASFLAGS="$ASFLAGS -f win32"
      AS_IF(
         [test "x$BITS" = "x32"],
         [ASFLAGS="$ASFLAGS -DPREFIX" LDFLAGS="-Wl,--kill-at"]
      )
   ],
   [AC_MSG_ERROR([Unknown host OS])]
)
AC_SUBST([ASFLAGS])

PKG_CHECK_MODULES([VapourSynth], [vapoursynth])
PKG_CHECK_MODULES([FFTW3F], [fftw3f])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset pow strchr])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
