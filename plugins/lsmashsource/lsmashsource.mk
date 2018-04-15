include ../../config.mak

LIBNAME = lsmashsource

local_CFLAGS = \
	-I./libs/include \
	-I../../ffmpeg_libs/include \
	-Wno-sign-compare \
	-Wno-missing-field-initializers \
	-Wno-deprecated-declarations

LIBADD = libs/lib/liblsmash.a \
	-L../../ffmpeg_libs/lib \
	-lavformat -lavcodec -lswscale -lavutil -lavresample \
	-lpthread -lm

include ../../cc.inc

