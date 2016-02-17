
LIBNAME = imagereader
local_CFLAGS = -Wno-sign-compare -I./libjpeg-turbo
LIBADD = libjpeg-turbo/.libs/libturbojpeg.a \
	libjpeg-turbo/.libs/libjpeg.a \
	libjpeg-turbo/simd/.libs/libsimd.a \
	-lpng -lz

include ../../cc.inc

