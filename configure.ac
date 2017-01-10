m4_define([AX_CHECK_LIB_LINK], [{
    LIBS_backup="$LIBS"
    LIBS="-l$1 $2"
    AC_MSG_CHECKING([for -l$1])
    AC_LINK_IFELSE([
        AC_LANG_SOURCE(
            [[int main() { return 0; }]]
        )
    ], [AC_MSG_RESULT([yes])
        eval have_$( echo $1 )="yes"
    ], [AC_MSG_RESULT([no])
        eval have_$( echo $1 )="no"
    ])
    LIBS="$LIBS_backup"
}])

m4_include([include/ac/ax_check_cl.m4])
m4_include([include/ac/ax_cxx_compile_stdcxx.m4])
m4_include([include/ac/ax_check_gnu_make.m4])
m4_include([include/ac/ax_check_compile_flag.m4])

AC_INIT([vapoursynth-extra-plugins], [0], [https://github.com/darealshinji/vapoursynth-plugins])
AC_CONFIG_AUX_DIR([include/build-aux])

AC_CANONICAL_HOST

: ${CFLAGS="-O3"}
: ${CXXFLAGS="-O3"}
: ${CPPFLAGS="-D_FORTIFY_SOURCE=2"}


# programs
AC_PROG_CXX
AC_PROG_CC

AC_CHECK_TOOL([AR], [ar])
AS_IF([test "x$AR" = "x"], [exit 1])

AC_CHECK_TOOL([RANLIB], [ranlib])
AS_IF([test "x$RANLIB" = "x"], [exit 1])

AC_PROG_INSTALL
AC_PROG_MAKE_SET

AX_CHECK_GNU_MAKE
AS_IF([test "x$ax_cv_gnu_make_command" = "x"], [exit 1],
      [AC_SUBST([MAKE], [$ax_cv_gnu_make_command])])

AC_CHECK_TOOL([NASM], [nasm])
AS_IF([test "x$NASM" = "x"], [have_nasm="no"], [have_nasm="yes"])

AC_CHECK_TOOL([YASM], [yasm])
AS_IF([test "x$YASM" = "x"], [have_yasm="no"], [have_yasm="yes"])


# build flags
AX_CXX_COMPILE_STDCXX([14], [ext], [optional])
AS_IF([test "x$ax_cv_cxx_compile_cxx14" != "xyes"], [
    AX_CXX_COMPILE_STDCXX([11], [ext], [mandatory])
])

_sps="-fstack-protector-strong"
_sp="-fstack-protector --param=ssp-buffer-size=4"
AX_CHECK_COMPILE_FLAG([$_sps], [CXXFLAGS="$CXXFLAGS $_sps" CFLAGS="$CFLAGS $_sps"], [
    AX_CHECK_COMPILE_FLAG([$_sp], [CXXFLAGS="$CXXFLAGS $_sp" CFLAGS="$CFLAGS $_sp"])
])


# architecture
AS_CASE([$host_cpu],
    [i?86], [have_i686="yes" bits="32"],
    [x86_64], [have_x86_64="yes" bits="64"], [
    AC_MSG_ERROR([your CPU is currently not supported by this buildsystem: $host_cpu])
])
AS_IF([test "x$have_i686" = "xyes"],   [AC_SUBST([HAVE_I686], [yes])])
AS_IF([test "x$have_x86_64" = "xyes"], [AC_SUBST([HAVE_X86_64], [yes])])
AS_IF([test "x$build" != "x$host"], [
    CFLAGS="-m$bits $CFLAGS"
    CXXFLAGS="-m$bits $CXXFLAGS"
    LDFLAGS="-m$bits $LDFLAGS"
])


# libraries
AC_CHECK_LIB([m], [sqrt], [], [exit 1])
AC_CHECK_LIB([pthread], [pthread_create], [], [exit 1])

AX_CHECK_LIB_LINK([z])
AX_CHECK_LIB_LINK([png])
AX_CHECK_LIB_LINK([sndfile])
AX_CHECK_LIB_LINK([xvidcore])

# FFTW3
AX_CHECK_LIB_LINK([fftw3f])
AS_IF([test "x$have_fftw3f" != "xno"], [
    fftw3_minimal="3.3.4"
    printf "checking for fftw >= $fftw3_minimal... "
    fftw3_version=$($srcdir/include/ac/check-fftw3-version.sh $fftw3_minimal "$CC $CFLAGS $LDFLAGS")
    AS_IF([test "x$fftw3_version" = "xyes"],
          [have_fftw3_newer="yes"], [have_fftw3_newer="no"])
    printf "$have_fftw3_newer\n"
])

AX_CHECK_LIB_LINK([opencv_core])
AX_CHECK_CL


# config
AS_IF([test "x$have_nasm" != "xno"],        [AC_SUBST([HAVE_NASM], [yes])])
AS_IF([test "x$have_yasm" != "xno"],        [AC_SUBST([HAVE_YASM], [yes])])
AS_IF([test "x$no_cl" != "xyes"],           [AC_SUBST([HAVE_OPENCL], [yes])])
AS_IF([test "x$have_png" != "xno"],         [AC_SUBST([HAVE_PNG], [yes])])
AS_IF([test "x$have_zlib" != "xno"],        [AC_SUBST([HAVE_ZLIB], [yes])])
AS_IF([test "x$have_sndfile" != "xno"],     [AC_SUBST([HAVE_SNDFILE], [yes])])
AS_IF([test "x$have_opencv" != "xno"],      [AC_SUBST([HAVE_OPENCV], [yes])])
AS_IF([test "x$have_xvidcore" != "xno"],    [AC_SUBST([HAVE_XVIDCORE], [yes])])
AS_IF([test "x$have_fftw3f" != "xno"],      [AC_SUBST([HAVE_FFTW3F], [yes])])
AS_IF([test "x$have_fftw3_newer" != "xno"], [AC_SUBST([HAVE_FFTW3F_NEWER], [yes])])


dnl # Optional YASM: NNEDI3 TComb
dnl opt_yasm_plugins="nnedi3 tcomb"
dnl AS_IF([test "x$have_yasm" != "xno"],
dnl       [build_with_yasm="with" opt_yasm_plugins="nnedi3 tcomb"],
dnl       [build_with_yasm="without"])

dnl # ImageReader
dnl AS_IF([test "x$have_png" != "xno" -a "x$have_zlib" != "xno"],
dnl       [build_plugins="$build_plugins imagereader"],
dnl       [no_build_plugins="$no_build_plugins imagereader"])
dnl # MVTools
dnl AS_IF([test "x$have_fftw3f" != "xno" -a "x$have_yasm" != "xno"],
dnl       [build_plugins="$build_plugins mvtools"],
dnl       [no_build_plugins="$no_build_plugins mvtools"])
dnl # FFMS2
dnl AS_IF([test "x$have_zlib" != "xno"],
dnl       [build_plugins="$build_plugins ffms2"],
dnl       [no_build_plugins="$no_build_plugins ffms2"])
dnl # Damb
dnl AS_IF([test "x$have_sndfile" != "xno"],
dnl       [build_plugins="$build_plugins damb"],
dnl       [no_build_plugins="$no_build_plugins damb"])
dnl # DePan, DFTTest, FFT3DFilter
dnl AS_IF([test "x$have_fftw3" != "xno"],
dnl       [build_plugins="$build_plugins depan dfttest fft3dfilter"],
dnl       [no_build_plugins="$no_build_plugins depan dfttest fft3dfilter"])
dnl # BM3D
dnl AS_IF([test "x$have_fftw3f" != "xno" -a "x$have_fftw3_newer" != "xno"],
dnl       [build_plugins="$build_plugins bm3d"],
dnl       [no_build_plugins="$no_build_plugins bm3d"])
dnl # scxvid
dnl AS_IF([test "x$have_xvidcore" != "xno"],
dnl       [build_plugins="$build_plugins scxvid"],
dnl       [no_build_plugins="$no_build_plugins scxvid"])
dnl # KNLMeansCL
dnl AS_IF([test "x$no_cl" != "xno"],
dnl       [build_plugins="$build_plugins knlmeanscl"],
dnl       [no_build_plugins="$no_build_plugins knlmeanscl"])
dnl # waifu2x-w2xc
dnl AS_IF([test "x$no_cl" != "xno" -a "x$have_opencv_core" = "xyes"],
dnl       [build_plugins="$build_plugins waifu2x-w2xc"],
dnl       [no_build_plugins="$no_build_plugins waifu2x-w2xc"])

dnl # plugins without special dependencies
dnl build_plugins="$build_plugins $opt_yasm_plugins addgrain awarpsharp2 bifrost bilateral combmask convo2d ctmf d2vsource dctfilter deblock delogo eedi2 fieldhint fillborders flash3kyuu_deband fmtconv genericfilters histogram it lsmashsource msmoosh rawsource reduceflicker retinex sangnommod scenechange scrawl ssiq tc2cfr tcanny tcomb tdeintmod templinearapproximate temporalsoften temporalsoften2 tnlmeans vaguedenoiser vautodeint videoscope waifu2x-opt wwxd yadifmod zimg"

dnl TODO: update this^^^^


AC_CONFIG_FILES([config.mak])
AC_OUTPUT


dnl build_plugins="$(echo $build_plugins | tr ' ' '\n' | sort | tr '\n' ' ')"
dnl no_build_plugins="$(echo $no_build_plugins | tr ' ' '\n' | sort | tr '\n' ' ')"

dnl AC_MSG_RESULT([
dnl Plugins to build:
dnl $build_plugins

dnl Build with optional YASM assembler: $opt_yasm_plugins

dnl Plugins that won't be build: $no_build_plugins
dnl ])

