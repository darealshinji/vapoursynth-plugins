m4_include([include/ac/ax_check_cl.m4])
m4_include([include/ac/ax_cxx_compile_stdcxx_11.m4])
m4_include([include/ac/ax_check_gnu_make.m4])
m4_include([include/ac/ax_prog_nasm.m4])
m4_include([include/ac/ax_prog_yasm.m4])

AC_INIT([vapoursynth-extra-plugins], [0], [https://github.com/darealshinji/vapoursynth-plugins])
AC_CONFIG_AUX_DIR([include/build-aux])

AC_CANONICAL_HOST

: ${CFLAGS="-O3"}
: ${CXXFLAGS="-O3"}

# programs
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AX_CHECK_GNU_MAKE
AS_IF([test "x$ax_cv_gnu_make_command" != "xmake"],
       AC_MSG_WARN([`make' is not a GNU make command]))
AC_PROG_LN_S
AX_PROG_NASM([nasm_avail="no"])
AX_PROG_YASM([yasm_avail="no"])

AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])

# architecture
AS_CASE([$host_cpu],
        [i?86], [have_i686="yes" bits="32"],
        [x86_64], [have_x86_64="yes" bits="64"])
AS_IF([test "x$have_i686" = "xyes"],   [AC_SUBST([HAVE_I686], [yes])])
AS_IF([test "x$have_x86_64" = "xyes"], [AC_SUBST([HAVE_X86_64], [yes])])
AS_IF([test $build != $host], [
  CFLAGS="-m$bits $CFLAGS"
  CXXFLAGS="-m$bits $CXXFLAGS"
  LDFLAGS="-m$bits $LDFLAGS"
])

# libraries
AX_CHECK_CL
AC_CHECK_LIB([m],          [sqrt], ,                   [exit 1])
AC_CHECK_LIB([jpeg],       [jpeg_start_decompress], ,  [jpeg_avail="no"])
AC_CHECK_LIB([png],        [png_create_read_struct], , [png_avail="no"])
AC_CHECK_LIB([z],          [inflate], ,                [zlib_avail="no"])
AC_CHECK_LIB([avcodec],    [avcodec_version], ,        [avcodec_avail="no"])
AC_CHECK_LIB([avformat],   [avformat_version], ,       [avformat_avail="no"])
AC_CHECK_LIB([avresample], [avresample_version], ,     [avresample_avail="no"])
AC_CHECK_LIB([avutil],     [avutil_version], ,         [avutil_avail="no"])
AC_CHECK_LIB([swscale],    [swscale_version], ,        [swscale_avail="no"])
AC_CHECK_LIB([sndfile],    [sf_seek], ,                [sndfile_avail="no"])
AC_CHECK_LIB([opencv_core],[main], ,                   [opencv_avail="no"])
AC_CHECK_LIB([xvidcore],   [xvid_encore], ,            [xvid_avail="no"])
AC_CHECK_LIB([fftw3f],     [fftwf_destroy_plan], ,     [fftw3f_avail="no"])
AS_IF([test "x$fftw3f_avail" != "xno"], [
  fftw3_minimal="3.3.4"
  printf "checking for fftw >= $fftw3_minimal... "
  fftw3_version=$($srcdir/include/ac/check-fftw3-version.sh $fftw3_minimal "$CC $CFLAGS $LDFLAGS")
  AS_IF([test "x$fftw3_version" = "xyes"],
        [fftw3f_newer_avail="yes"], [fftw3f_newer_avail="no"])
  printf "$fftw3f_newer_avail\n"
])

# config
AS_IF([test "x$nasm_avail" != "xno"],         [AC_SUBST([HAVE_NASM], [yes])])
AS_IF([test "x$yasm_avail" != "xno"],         [AC_SUBST([HAVE_YASM], [yes])])
AS_IF([test "x$no_cl" != "xyes"],             [AC_SUBST([HAVE_OPENCL], [yes])])
AS_IF([test "x$jpeg_avail" != "xno"],         [AC_SUBST([HAVE_JPEG], [yes])])
AS_IF([test "x$png_avail" != "xno"],          [AC_SUBST([HAVE_PNG], [yes])])
AS_IF([test "x$zlib_avail" != "xno"],         [AC_SUBST([HAVE_ZLIB], [yes])])
AS_IF([test "x$avcodec_avail" != "xno"],      [AC_SUBST([HAVE_AVCODEC], [yes])])
AS_IF([test "x$avformat_avail" != "xno"],     [AC_SUBST([HAVE_AVFORMAT], [yes])])
AS_IF([test "x$avresample_avail" != "xno"],   [AC_SUBST([HAVE_AVRESAMPLE], [yes])])
AS_IF([test "x$avutil_avail" != "xno"],       [AC_SUBST([HAVE_AVUTIL], [yes])])
AS_IF([test "x$swscale_avail" != "xno"],      [AC_SUBST([HAVE_SWSCALE], [yes])])
AS_IF([test "x$sndfile_avail" != "xno"],      [AC_SUBST([HAVE_SNDFILE], [yes])])
AS_IF([test "x$opencv_avail" != "xno"],       [AC_SUBST([HAVE_OPENCV], [yes])])
AS_IF([test "x$xvid_avail" != "xno"],         [AC_SUBST([HAVE_XVID], [yes])])
AS_IF([test "x$fftw3f_avail" != "xno"],       [AC_SUBST([HAVE_FFTW3F], [yes])])
AS_IF([test "x$fftw3f_newer_avail" != "xno"], [AC_SUBST([HAVE_FFTW3F_NEWER], [yes])])

AC_CONFIG_FILES([config.mak])
AC_OUTPUT

