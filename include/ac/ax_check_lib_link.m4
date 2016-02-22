# The MIT License (MIT)
# 
# Copyright (c) 2016, djcj <djcj@gmx.de>
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

# usage: AX_CHECK_LIB_LINK(prefix, library, [extra-libs])
# if [prefix] is empty, [library] will be used as prefix

m4_define([AX_CHECK_LIB_LINK], [{
    pfx="$1"
    AS_IF([test "x$pfx" = "x"], [pfx="$2"])
    eval have_$( echo $pfx )="no"
    LIBS_backup="$LIBS"
    LIBS="-l$2 $3"
    AC_MSG_CHECKING([for -l$2])
    AC_LINK_IFELSE([
        AC_LANG_SOURCE(
            [[int main() { return 0; }]]
        )
    ], [AC_MSG_RESULT([yes])
        eval have_$( echo $pfx )="yes"
    ], [AC_MSG_RESULT([no])])
    LIBS="$LIBS_backup"
}])
