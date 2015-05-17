
LIB = libimagereader.so
local_CFLAGS = -Wno-sign-compare -Ilibjpeg-turbo -std=c99
LIBADD = libjpeg-turbo/.libs/libturbojpeg.a -ljpeg -lpng -lz

include ../cc.inc

