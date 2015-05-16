
LIB = libdamb.so
local_CXXFLAGS = -std=c++11 -Wshadow $(shell pkg-config --cflags sndfile)
LIBADD = $(shell pkg-config --libs sndfile)

include ../cxx.inc

