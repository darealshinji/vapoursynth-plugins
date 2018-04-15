include ../../config.mak

LIBNAME = d2vsource
local_CXXFLAGS = -Wno-deprecated-declarations -Wno-sign-compare -I../../ffmpeg_libs/include
LIBADD = -L../../ffmpeg_libs/lib \
	-lavcodec -ldl -lpthread -lavformat -lavcodec -lavutil -lswscale -lavresample

include ../../cxx.inc

