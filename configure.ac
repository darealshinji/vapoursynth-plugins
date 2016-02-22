m4_include([include/ac/ax_check_cl.m4])
m4_include([include/ac/ax_cxx_compile_stdcxx_11.m4])
m4_include([include/ac/ax_check_gnu_make.m4])
m4_include([include/ac/ax_check_compile_flag.m4])

m4_include([include/ac/ax_check_pkg_lib.m4])
m4_include([include/ac/ax_check_lib_link.m4])

m4_include([include/ac/ax_boost_base.m4])
m4_include([include/ac/ax_boost_filesystem.m4])
m4_include([include/ac/ax_boost_iostreams.m4])
m4_include([include/ac/ax_boost_system.m4])

AC_INIT([vapoursynth-extra-plugins], [0], [https://github.com/darealshinji/vapoursynth-plugins])
AC_CONFIG_AUX_DIR([include/build-aux])

AC_CANONICAL_HOST

: ${CFLAGS="-O3"}
: ${CXXFLAGS="-O3"}


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
AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])

_sps="-fstack-protector-strong"
_sp="-fstack-protector --param=ssp-buffer-size=4"
AX_CHECK_COMPILE_FLAG([$_sps], [
    CXXFLAGS="$CXXFLAGS $_sps" CFLAGS="$CFLAGS $_sps"
], [AX_CHECK_COMPILE_FLAG([$_sp], [
        CXXFLAGS="$CXXFLAGS $_sp" CFLAGS="$CFLAGS $_sp"])
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


# headers
AC_CHECK_HEADER([cudnn.h], [AC_SUBST([HAVE_CUDNN_H], [yes])])


# libraries
AC_CHECK_LIB([m], [sqrt], [], [exit 1])
AC_CHECK_LIB([pthread], [pthread_create], [], [exit 1])

AX_CHECK_PKG_LIB([zlib],     [zlib],     [z],        [zlib.h])
AX_CHECK_PKG_LIB([png],      [png],      [png],      [png.h])
AX_CHECK_PKG_LIB([sndfile],  [sndfile],  [sndfile],  [sndfile.h])
AX_CHECK_PKG_LIB([xvidcore], [xvidcore], [xvidcore], [xvid.h])

# FFTW3
AX_CHECK_PKG_LIB([fftw3f],   [fftw3],    [fftw3f],   [fftw3.h])
AS_IF([test "x$have_fftw3f" != "xno"], [
    fftw3_minimal="3.3.4"
    printf "checking for fftw >= $fftw3_minimal... "
    fftw3_version=$($srcdir/include/ac/check-fftw3-version.sh $fftw3_minimal "$CC $CFLAGS $LDFLAGS")
    AS_IF([test "x$fftw3_version" = "xyes"],
          [have_fftw3_newer="yes"], [have_fftw3_newer="no"])
    printf "$have_fftw3_newer\n"
])

# OpenCV
AX_CHECK_PKG_LIB([opencv], [opencv], [opencv_core], [opencv2/opencv.hpp])
AS_IF([test "x$have_opencv" = "xyes"], [
    AX_CHECK_LIB_LINK([], [opencv_imgcodecs])
    AX_CHECK_LIB_LINK([], [opencv_imgproc])
])

AX_CHECK_CL

AX_CHECK_PKG_LIB([caffe],    [libcaffe],      [caffe],    [caffe/caffe.hpp])
AX_CHECK_PKG_LIB([cudart],   [cuda_runtime],  [cudart],   [cublas_v2.h])
AX_CHECK_PKG_LIB([gflags],   [libgflags],     [gflags],   [gflags/gflags.h])
AX_CHECK_PKG_LIB([glog],     [libglog],       [glog],     [glog/logging.h])
AX_CHECK_PKG_LIB([openblas], [blas-openblas], [openblas], [])
AX_CHECK_PKG_LIB([protobuf], [protobuf],      [protobuf], [google/protobuf/io/coded_stream.h])

# HDF5
AX_CHECK_LIB_LINK([], [hdf5_serial])
AS_IF([test "x$have_hdf5_serial" = "xyes"], [
    AX_CHECK_LIB_LINK([], [hdf5_serial_hl], [-lhdf5_serial])
])
AS_IF([test "x$have_hdf5_serial_hl" = "xyes"], [hdf5_libs="-lhdf5_serial -hdf5_serial_hl"], [
    AX_CHECK_LIB_LINK([], [hdf5])
    AS_IF([test "x$have_hdf5" = "xyes"], [
        AX_CHECK_LIB_LINK([], [hdf5_hl], [-lhdf5])
    ])
])
AS_IF([test "x$have_hdf5_hl" = "xyes"], [hdf5_libs="-lhdf5 -hdf5_hl"])
AC_SUBST([HDF5_LIBS], [$hdf5_libs])

# Boost
AX_BOOST_BASE([1.50], [
    AX_BOOST_FILESYSTEM
    AX_BOOST_IOSTREAMS
    AX_BOOST_SYSTEM
])


# config
AS_IF([test "x$have_nasm" != "xno"],        [AC_SUBST([HAVE_NASM], [yes])])
AS_IF([test "x$have_yasm" != "xno"],        [AC_SUBST([HAVE_YASM], [yes])])
AS_IF([test "x$no_cl" != "xyes"],           [AC_SUBST([HAVE_OPENCL], [yes])])
AS_IF([test "x$have_png" != "xno"],         [AC_SUBST([HAVE_PNG], [yes])])
AS_IF([test "x$have_zlib" != "xno"],        [AC_SUBST([HAVE_ZLIB], [yes])])
AS_IF([test "x$have_sndfile" != "xno"],     [AC_SUBST([HAVE_SNDFILE], [yes])])
AS_IF([test "x$have_opencv" != "xno"],      [AC_SUBST([HAVE_OPENCV], [yes])])
AS_IF([test "x$have_xvid" != "xno"],        [AC_SUBST([HAVE_XVID], [yes])])
AS_IF([test "x$have_fftw3f" != "xno"],      [AC_SUBST([HAVE_FFTW3F], [yes])])
AS_IF([test "x$have_fftw3_newer" != "xno"], [AC_SUBST([HAVE_FFTW3F_NEWER], [yes])])
AS_IF([test "x$have_caffe" != "xno"],       [AC_SUBST([HAVE_CAFFE], [yes])])
AS_IF([test "x$have_cudart" != "xno"],      [AC_SUBST([HAVE_CUDART], [yes])])
AS_IF([test "x$have_gflags" != "xno"],      [AC_SUBST([HAVE_GFLAGS], [yes])])
AS_IF([test "x$have_glog" != "xno"],        [AC_SUBST([HAVE_GLOG], [yes])])
AS_IF([test "x$have_openblas" != "xno"],    [AC_SUBST([HAVE_OPENBLAS], [yes])])
AS_IF([test "x$have_protobuf" != "xno"],    [AC_SUBST([HAVE_PROTOBUF], [yes])])
AS_IF([test "x$have_hdf5" != "xno"],        [AC_SUBST([HAVE_HDF5], [yes])])

# Optinal YASM assembler:
# FluxSmooth NNEDI3 TComb
opt_yasm_plugins="fluxsmooth nnedi3 tcomb"
AS_IF([test "x$have_yasm" != "xno"], [build_with_yasm="with"],
      [build_with_yasm="without"])

# ImageReader
AS_IF([test "x$have_png" != "xno" -a \
       "x$have_zlib" != "xno"],
      [build_plugins="$build_plugins imagereader"],
      [no_build_plugins="$no_build_plugins imagereader"])
# MVTools
AS_IF([test "x$have_fftw3f" != "xno" -a \
       "x$have_yasm" != "xno"],
      [build_plugins="$build_plugins mvtools"],
      [no_build_plugins="$no_build_plugins mvtools"])
# FFMS2
AS_IF([test "x$have_zlib"! = "xno"],
      [build_plugins="$build_plugins ffms2"],
      [no_build_plugins="$no_build_plugins ffms2"])
# Damb
AS_IF([test "x$have_sndfile" != "xno"],
      [build_plugins="$build_plugins damb"],
      [no_build_plugins="$no_build_plugins damb"])
# DFTTest, FFT3DFilter
AS_IF([test "x$have_fftw3" != "xno"],
      [build_plugins="$build_plugins dfttest fft3dfilter"],
      [no_build_plugins="$no_build_plugins dfttest fft3dfilter"])
# BM3D
AS_IF([test "x$have_fftw3f" != "xno" -a  \
       "x$have_fftw3_newer" != "xno"],
      [build_plugins="$build_plugins bm3d"],
      [no_build_plugins="$no_build_plugins bm3d"])
# scxvid
AS_IF([test "x$have_xvid" != "xno"],
      [build_plugins="$build_plugins scxvid"],
      [no_build_plugins="$no_build_plugins scxvid"])
# KNLMeansCL
AS_IF([test "x$no_cl" != "xno"],
      [build_plugins="$build_plugins knlmeanscl"],
      [no_build_plugins="$no_build_plugins knlmeanscl"])
# waifu2x-caffe
AS_IF([test "x$have_caffe" != "xno" -a \
       "x$have_cudart" != "xno" -a \
       "x$have_gflags" != "xno" -a \
       "x$have_glog" != "xno" -a \
       "x$have_openblas" != "xno" -a \
       "x$have_protobuf" != "xno" -a \
       "x$hdf5_libs" != "x" -a \
       "x$ax_cv_boost_filesystem" = "xyes" -a \
       "x$ax_cv_boost_iostreams" = "xyes" -a \
       "x$ax_cv_boost_system" = "xyes" -a \
       "x$have_opencv" != "xno"],
      [build_plugins="$build_plugins waifu2x-caffe"
       AC_SUBST([ENABLE_WAIFU2X_CAFFE], [yes])],
      [no_build_plugins="$no_build_plugins waifu2x-caffe"])
# waifu2x-w2xc
AS_IF([test "x$no_cl" != "xno" -a \
       "x$have_opencv" != "xno"],
      [build_plugins="$build_plugins waifu2x-w2xc"],
      [no_build_plugins="$no_build_plugins waifu2x-w2xc"])

# plugins without special dependencies
build_plugins="$build_plugins $opt_yasm_plugins addgrain awarpsharp2 bifrost bilateral combmask convo2d ctmf d2vsource dctfilter deblock delogo eedi2 fieldhint fillborders flash3kyuu_deband fmtconv genericfilters histogram it lsmashsource msmoosh rawsource reduceflicker retinex sangnommod scenechange scrawl ssiq tc2cfr tcanny tcomb tdeintmod templinearapproximate temporalsoften temporalsoften2 tnlmeans vaguedenoiser vautodeint videoscope waifu2x-opt wwxd yadifmod zimg"


AC_CONFIG_FILES([config.mak])
AC_OUTPUT


build_plugins="$(echo $build_plugins | tr ' ' '\n' | sort | tr '\n' ' ')"
no_build_plugins="$(echo $no_build_plugins | tr ' ' '\n' | sort | tr '\n' ' ')"

AC_MSG_RESULT([
Plugins to build:
    $build_plugins

Plugins that won't be build:
    $no_build_plugins

Build $build_with_yasm optional YASM assembler:
    $opt_yasm_plugins
])

