m4_include([include/ac/ax_check_cl.m4])
m4_include([include/ac/ax_cxx_compile_stdcxx_11.m4])
m4_include([include/ac/ax_check_gnu_make.m4])
m4_include([include/ac/ax_prog_nasm.m4])
m4_include([include/ac/ax_prog_yasm.m4])
m4_include([include/ac/ax_check_pkg_lib.m4])
m4_include([include/ac/ax_check_compile_flag.m4])

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
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AX_CHECK_GNU_MAKE
AS_IF([test "x$ax_cv_gnu_make_command" = "x"], [exit 1],
      [AC_SUBST([MAKE], [$ax_cv_gnu_make_command])])
AX_PROG_NASM([nasm_avail="no"])
AX_PROG_YASM([yasm_avail="no"])

AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])

AX_CHECK_COMPILE_FLAG([-fstack-protector-strong], [
    CXXFLAGS="$CXXFLAGS -fstack-protector-strong"
    CFLAGS="$CFLAGS -fstack-protector-strong"
], [
    AX_CHECK_COMPILE_FLAG([-fstack-protector --param=ssp-buffer-size=4], [
        CXXFLAGS="$CXXFLAGS -fstack-protector --param=ssp-buffer-size=4"
        CFLAGS="$CFLAGS -fstack-protector --param=ssp-buffer-size=4"])
])

# architecture
AS_CASE([$host_cpu],
        [i?86], [have_i686="yes" bits="32"],
        [x86_64], [have_x86_64="yes" bits="64"],
        AC_MSG_ERROR([your CPU is currently not supported by this buildsystem: $host_cpu]))
AS_IF([test "x$have_i686" = "xyes"],   [AC_SUBST([HAVE_I686], [yes])])
AS_IF([test "x$have_x86_64" = "xyes"], [AC_SUBST([HAVE_X86_64], [yes])])
AS_IF([test $build != $host], [
  CFLAGS="-m$bits $CFLAGS"
  CXXFLAGS="-m$bits $CXXFLAGS"
  LDFLAGS="-m$bits $LDFLAGS"
])

# libraries
AC_CHECK_LIB([m], [sqrt], [], [exit 1])
AC_CHECK_LIB([pthread], [pthread_create], [], [exit 1])

AX_CHECK_PKG_LIB([zlib],     [zlib],     [z],        [zlib.h])
AX_CHECK_PKG_LIB([png],      [png],      [png],      [png.h])
AX_CHECK_PKG_LIB([sndfile],  [sndfile],  [sndfile],  [sndfile.h])
AX_CHECK_PKG_LIB([xvidcore], [xvidcore], [xvidcore], [xvid.h])
AX_CHECK_PKG_LIB([fftw3],    [fftw3],    [fftw3f],   [fftw3.h])

AS_IF([test "x$fftw3_lib_avail" != "xno"], [
  fftw3_minimal="3.3.4"
  printf "checking for fftw >= $fftw3_minimal... "
  fftw3_version=$($srcdir/include/ac/check-fftw3-version.sh $fftw3_minimal "$CC $CFLAGS $LDFLAGS")
  AS_IF([test "x$fftw3_version" = "xyes"],
        [fftw3_newer_avail="yes"], [fftw3_newer_avail="no"])
  printf "$fftw3_newer_avail\n"
])

AX_CHECK_PKG_LIB([opencv], [opencv], [opencv_core], [opencv2/opencv.hpp])
AX_CHECK_CL

# config
AS_IF([test "x$nasm_avail" != "xno"], [AC_SUBST([HAVE_NASM], [yes])])
AS_IF([test "x$yasm_avail" != "xno"], [AC_SUBST([HAVE_YASM], [yes])])
AS_IF([test "x$no_cl" != "xyes"],     [AC_SUBST([HAVE_OPENCL], [yes])])
AS_IF([test "x$png_avail" != "xno"],  [AC_SUBST([HAVE_PNG], [yes])])
AS_IF([test "x$zlib_avail" != "xno"], [AC_SUBST([HAVE_ZLIB], [yes])])
AS_IF([test "x$sndfile_lib_avail" != "xno" -a "x$sndfile_headers_avail" != "xno"],
                                      [AC_SUBST([HAVE_SNDFILE], [yes])])
AS_IF([test "x$opencv_lib_avail" != "xno" -a "x$opencv_headers_avail" != "xno"],
                                      [AC_SUBST([HAVE_OPENCV], [yes])])
AS_IF([test "x$xvid_lib_avail" != "xno" -a "x$xvid_headers_avail" != "xno"],
                                      [AC_SUBST([HAVE_XVID], [yes])])
AS_IF([test "x$fftw3_lib_avail" != "xno" -a "x$fftw3_headers_avail" != "xno"],
                                      [AC_SUBST([HAVE_FFTW3F], [yes])])
AS_IF([test "x$fftw3_newer_avail" != "xno"],
                                      [AC_SUBST([HAVE_FFTW3F_NEWER], [yes])])

AC_CONFIG_FILES([config.mak])
AC_OUTPUT

