AC_INIT([fluxsmooth], [1], [https://github.com/dubhater/vapoursynth-fluxsmooth/issues], [fluxsmooth], [https://github.com/dubhater/vapoursynth-fluxsmooth/])

: ${CFLAGS="-O2"}

AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-xz subdir-objects no-define])
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIR([m4])

LT_INIT([disable-static win32-dll])

AC_CANONICAL_HOST

AC_PROG_CC
AC_PROG_CC_C99

AS_IF(
   [test "x$host_cpu" = "xx86_64"],
   [AC_CHECK_PROGS([YASM], [yasm]) AC_DEFINE([USE_ASM])],
   [YASM="dummy"]
)

AM_CONDITIONAL([x86_64], [test "x$host_cpu" = "xx86_64"])

AS_IF(
   [test "x$YASM" = "x"],
   [AC_MSG_ERROR([yasm required but not found])],
   [AS="$YASM"]
)

AS_CASE(
  [$host_cpu],
  [i?86],   [BITS="32"],
  [x86_64], [BITS="64" ASFLAGS="$ASFLAGS -DARCH_X86_64=1 -DPIC"],
  [AC_MSG_ERROR([Currently nnedi3 can only be compiled for Intel CPUs.])]
)

AS_CASE(
   [$host_os],
   [darwin*],
   [
      ASFLAGS="$ASFLAGS -f macho$BITS -DPREFIX"
   ],
   [*linux*],
   [
      ASFLAGS="$ASFLAGS -f elf$BITS"
   ],
   [gnu*],
   [
      ASFLAGS="$ASFLAGS -f elf$BITS"
   ],
   [cygwin*],
   [
      ASFLAGS="$ASFLAGS -f win$BITS"
      AS_IF(
         [test "x$BITS" = "x32"],
         [LDFLAGS="-Wl,--kill-at"]
      )
   ],
   [mingw*],
   [
      ASFLAGS="$ASFLAGS -f win$BITS"
      AS_IF(
         [test "x$BITS" = "x32"],
         [LDFLAGS="-Wl,--kill-at"]
      )
   ],
   [AC_MSG_ERROR([Unknown host OS])]
)
AC_SUBST([ASFLAGS])

PKG_CHECK_MODULES([VapourSynth], [vapoursynth])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT16_T
AC_TYPE_INT64_T
AC_TYPE_UINT16_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
