include ../../config.mak

LIBNAME = ffms2

local_CXXFLAGS = \
	-Wno-missing-field-initializers \
	-Wno-deprecated-declarations \
	-I./include \
	-I../../ffmpeg_libs/include \
	-D_FILE_OFFSET_BITS=64 \
	-D__STDC_CONSTANT_MACROS

LIBADD = -L../../ffmpeg_libs/lib \
	-lavformat -lavutil -lavresample -lavcodec -ldl -lswscale \
	-lavutil -lpthread -lz

include ../../cxx.inc

