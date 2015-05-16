
ifeq ($(shell uname -p),x86_64)

LIB = libf3kdb.so
local_CXXFLAGS = -std=c++11 -I./include -DHAVE_ALIGNAS=1 -fvisibility=hidden -fvisibility-inlines-hidden -ggdb -ftrapv -msse2 -mssse3 -msse4.1

include ../cxx.inc

endif

