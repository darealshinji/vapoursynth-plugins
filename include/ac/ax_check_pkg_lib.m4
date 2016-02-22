# The MIT License (MIT)
# 
# Copyright (c) 2015-2016, djcj <djcj@gmx.de>
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# usage: AX_CHECK_PKG_LIB(prefix, pkg-module, library, headers, [extra-libs])

m4_define([AX_CHECK_PKG_LIB], [{
    eval have_$( echo $1 )="no"
    eval have_$( echo $1 )_lib="no"
    eval have_$( echo $1 )_headers="no"
    AC_LANG_PUSH([C++])
    PKG_CHECK_MODULES([$1], [$2], [
        eval have_$( echo $1 )="yes"
        eval have_$( echo $1 )_lib="yes"
        eval have_$( echo $1 )_headers="yes"
    ], [
        # library check
        LIBS_backup="$LIBS"
        LIBS="-l$3 $5"
        AC_MSG_CHECKING([for -l$3])
        AC_LINK_IFELSE([
            AC_LANG_SOURCE(
                [[int main() { return 0; }]]
            )
        ], [AC_MSG_RESULT([yes])
            eval have_$( echo $1 )_lib="yes"
        ], [AC_MSG_RESULT([no])])
        LIBS="$LIBS_backup"
        # header checks
        AS_IF([test "x$4" != "x"], [
            AC_CHECK_HEADERS([$4], [
                eval have_$( echo $1 )_headers="yes"
            ])
        ])
    ])
    AS_IF([test "x$have_${1}_headers" = "xyes" -a "x$have_${1}_lib" = "xyes"],
          [eval have_$( echo $1 )="yes"])
    AC_LANG_POP([C++])
}])
