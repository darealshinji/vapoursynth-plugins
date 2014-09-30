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


AC_INIT([vapoursynth-plugins], [1])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-xz subdir-objects no-define])
AM_SILENT_RULES([yes])

LT_INIT([disable-static])

CHECK_GNU_MAKE
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_CC_C99

AC_CXX_FLAG([-std=c++11])

AC_CANONICAL_HOST

AC_CHECK_PROGS([EXECSTACK], [execstack])
AS_IF(
   [test "x$EXECSTACK" = "x"],
   [AC_MSG_ERROR([execstack required but not found])],
   [AS="$EXECSTACK"]
)

AM_CONDITIONAL([x86_64], [test "x$host_cpu" = "xx86_64"])

AC_CHECK_PROGS([YASM], [yasm])
AS_IF(
   [test "x$YASM" = "x"],
   [AC_MSG_ERROR([yasm required but not found])],
   [AS="$YASM"]
)

AC_SUBST([ASFLAGS])

# Checks for libraries.
AC_CHECK_LIB([m], [main])
AC_CHECK_LIB([xvidcore], [main])
PKG_CHECK_MODULES([VapourSynth], [vapoursynth])
PKG_CHECK_MODULES([libavcodec], [libavcodec])
PKG_CHECK_MODULES([libavformat], [libavformat])
PKG_CHECK_MODULES([libavutil], [libavutil])

# Checks for header files.
AC_CHECK_HEADERS([float.h inttypes.h malloc.h stdint.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FSEEKO
AC_FUNC_MALLOC
AC_CHECK_FUNCS([floor memset pow sqrt strcasecmp strchr strspn strtoul])

AC_CONFIG_FILES([Makefile \
                 bifrost/Makefile \
                 combmask/Makefile \
                 convo2d/Makefile \
                 d2vsource/Makefile \
                 fieldhint/Makefile \
                 fillborders/Makefile \
                 genericfilters/Makefile \
                 histogram/Makefile \
                 msmoosh/Makefile \
                 rawsource/Makefile \
                 scrawl/Makefile \
                 scxvid/Makefile \
                 ssiq/Makefile \
                 temporalsoften/Makefile \
                 vautodeint/Makefile \
                 videoscope/Makefile \
                 wwxd/Makefile
                 ])
AC_CONFIG_SUBDIRS([fluxsmooth
                   mvtools
                   nnedi3
                   tcomb
                   ])
AC_OUTPUT

