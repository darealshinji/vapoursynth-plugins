include ../../config.mak

LIBNAME = d2vsource
local_CXXFLAGS = -Wno-sign-compare -I../../ffmpeg_libs/include
LIBADD = -L../../ffmpeg_libs/lib \
	-lavcodec -lpthread -lavformat -lavutil -lswscale -lavresample

include ../../cxx.inc

