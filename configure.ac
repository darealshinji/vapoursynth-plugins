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
_sps="-fstack-protector-strong"
_sp="-fstack-protector --param=ssp-buffer-size=4"
AX_CHECK_COMPILE_FLAG([$_sps], [CXXFLAGS="$CXXFLAGS $_sps" CFLAGS="$CFLAGS $_sps"], [
    AX_CHECK_COMPILE_FLAG([$_sp], [CXXFLAGS="$CXXFLAGS $_sp" CFLAGS="$CFLAGS $_sp"])
])
AX_CXX_COMPILE_STDCXX([14], [ext], [mandatory])


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
AX_CHECK_LIB_LINK([bluray])
AX_CHECK_LIB_LINK([fftw3f])
AX_CHECK_LIB_LINK([opencv_core])
AX_CHECK_CL


# boost::compute headers
m4_define([AX_CHECK_BOOST_HEADERS], [{
    CXXFLAGS_backup="$CXXFLAGS"
    CXXFLAGS="$CXXFLAGS -I$1"
    AC_MSG_CHECKING([for boost headers in $1])
    AC_LANG_PUSH([C++])
    AC_COMPILE_IFELSE([
        AC_LANG_SOURCE(
            [[#include <boost/compute/core.hpp>
#include <boost/compute/utility/dim.hpp>
#include <boost/compute/utility/source.hpp>
namespace compute = boost::compute;]]
        )
    ], [AC_MSG_RESULT([yes])
       boost_cppflags="-I$1"
    ], [AC_MSG_RESULT([no])
       boost_cppflags=""
    ])
    CXXFLAGS="$CXXFLAGS_backup"
    AC_LANG_POP([C++])
}])

AX_CHECK_BOOST_HEADERS([/usr/include])
AS_IF([test "x$boost_cppflags" = "x"], [
    AX_CHECK_BOOST_HEADERS([/usr/include/compute])
    AS_IF([test "x$boost_cppflags" != "x"], [
        AC_SUBST([BOOST_CPPFLAGS], [$boost_cppflags])
    ])
])

# boost libraries
AS_IF([test "x$boost_cppflags" != "x"], [
    AX_CHECK_LIB_LINK([boost_filesystem])
    AX_CHECK_LIB_LINK([boost_system])
])


# config
AS_IF([test "x$have_nasm" = "xyes"],        [AC_SUBST([HAVE_NASM], [yes])])
AS_IF([test "x$have_yasm" = "xyes"],        [AC_SUBST([HAVE_YASM], [yes])])
AS_IF([test "x$no_cl" != "xyes"],           [AC_SUBST([HAVE_OPENCL], [yes])])
AS_IF([test "x$have_boost_filesystem" = "xyes"], [AC_SUBST([HAVE_BOOST_FILESYSTEM], [yes])])
AS_IF([test "x$have_boost_system" = "xyes"], [AC_SUBST([HAVE_BOOST_SYSTEM], [yes])])
AS_IF([test "x$have_png" = "xyes"],         [AC_SUBST([HAVE_PNG], [yes])])
AS_IF([test "x$have_z" = "xyes"],           [AC_SUBST([HAVE_ZLIB], [yes])])
AS_IF([test "x$have_sndfile" = "xyes"],     [AC_SUBST([HAVE_SNDFILE], [yes])])
AS_IF([test "x$have_opencv_core" = "xyes"], [AC_SUBST([HAVE_OPENCV], [yes])])
AS_IF([test "x$have_xvidcore" = "xyes"],    [AC_SUBST([HAVE_XVIDCORE], [yes])])
AS_IF([test "x$have_bluray" = "xyes"],      [AC_SUBST([HAVE_BLURAY], [yes])])
AS_IF([test "x$have_fftw3f" = "xyes"],      [AC_SUBST([HAVE_FFTW3F], [yes])])


AC_CONFIG_FILES([config.mak])
AC_OUTPUT
