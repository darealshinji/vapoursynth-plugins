
LIB = libimagereader.so
local_CFLAGS = -Ilibjpeg-turbo -std=c99
LIBADD = libjpeg-turbo/.libs/libturbojpeg.a -ljpeg -lpng -lz

include ../cc.inc

