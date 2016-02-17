
LIB = libimagereader.so
local_CFLAGS = -Wno-sign-compare -I./libjpeg-turbo
LIBADD = libjpeg-turbo/.libs/libturbojpeg.a -ljpeg -lpng -lz

include ../../cc.inc

