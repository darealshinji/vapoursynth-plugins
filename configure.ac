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


AC_INIT([vapoursynth-plugins], [1], [https://github.com/darealshinji/vapoursynth-plugins/issues])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-xz subdir-objects no-define])
AM_SILENT_RULES([yes])

LT_INIT([disable-static])

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
AM_CONDITIONAL([x86_64], [test "x$host_cpu" = "xx86_64"])

AC_CHECK_PROGS([YASM], [yasm])
AS_IF(
   [test "x$YASM" = "x"],
   [AC_MSG_ERROR([yasm required but not found])],
   [AS="$YASM"]
)

AC_SUBST([ASFLAGS])

# Checks for libraries.
AC_CHECK_LIB([m], [main], [], [exit 1])
AC_CHECK_LIB([png], [main], [], [exit 1])
AC_CHECK_LIB([z], [main], [], [exit 1])
AC_CHECK_LIB([xvidcore], [main], [], [exit 1])
PKG_CHECK_MODULES([VapourSynth], [vapoursynth])
PKG_CHECK_MODULES([libavcodec], [libavcodec])
PKG_CHECK_MODULES([libavformat], [libavformat])
PKG_CHECK_MODULES([libavutil], [libavutil])
PKG_CHECK_MODULES([fftw3f], [fftw3])
PKG_CHECK_MODULES([SNDFILE], [sndfile])

# Checks for header files.
AC_CHECK_HEADERS([float.h inttypes.h malloc.h stdint.h stdlib.h string.h png.h])

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
                 addgrain/Makefile \
                 bifrost/Makefile \
                 bilateral/Makefile \
                 combmask/Makefile \
                 convo2d/Makefile \
                 ctmf/Makefile \
                 d2vsource/Makefile \
                 damb/Makefile \
                 dctfilter/Makefile \
                 deblock/Makefile \
                 delogo/Makefile \
                 dfttest/Makefile \
                 eedi2/Makefile \
                 fieldhint/Makefile \
                 fillborders/Makefile \
                 flash3kyuu_deband/Makefile \
                 fmtconv/Makefile \
                 genericfilters/Makefile \
                 histogram/Makefile \
                 imagereader/Makefile \
                 imagereader/src/Makefile \
                 it/Makefile \
                 msmoosh/Makefile \
                 rawsource/Makefile \
                 retinex/Makefile \
                 sangnommod/Makefile \
                 scenechange/Makefile \
                 scrawl/Makefile \
                 scxvid/Makefile \
                 ssiq/Makefile \
				 tc2cfr/Makefile \
                 tdeintmod/Makefile \
                 templinearapproximate/Makefile \
                 temporalsoften/Makefile \
                 temporalsoften2/Makefile \
				 vaguedenoiser/Makefile \
                 vautodeint/Makefile \
                 videoscope/Makefile \
                 wwxd/Makefile \
                 yadifmod/Makefile \
                 zimg/Makefile
                 ])
AC_CONFIG_SUBDIRS([fluxsmooth
                   mvtools
                   nnedi3
                   tcomb
                   ])
AC_OUTPUT

