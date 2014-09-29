AC_INIT([tcomb], [1], [https://github.com/dubhater/vapoursynth-tcomb/issues], [tcomb], [https://github.com/dubhater/vapoursynth-tcomb/])

: ${CFLAGS="-O2"}

AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-xz subdir-objects no-define])
AM_SILENT_RULES([yes])

LT_INIT([disable-static win32-dll])

AC_CANONICAL_HOST

AC_PROG_CC_C99

AS_IF(
   [test "x$host_cpu" = "xx86_64"],
   [AC_CHECK_PROGS([YASM], [yasm]) AC_DEFINE([HAVE_ASM])],
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
  [x86_64], [BITS="64" ASFLAGS="$ASFLAGS -DARCH_X86_64=1 -DPIC -Dprivate_prefix=tcomb"],
  []
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

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
