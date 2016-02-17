include ../../config.mak

LIB = liblsmashsource.so

local_CFLAGS = -std=c99 -I./libs/include -Wno-sign-compare -Wno-missing-field-initializers
LIBADD = -L./libs/lib -llsmash -lavformat -lavcodec -lswscale -lavutil -lavresample -lpthread -lm

include ../../cc.inc

