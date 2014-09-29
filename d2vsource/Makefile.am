AM_CXXFLAGS = -Wall -Wextra -Wno-unused-parameter

AM_CPPFLAGS = $(VapourSynth_CFLAGS)

lib_LTLIBRARIES = libd2vsource.la

libd2vsource_la_SOURCES = \
	src/applyrff.cpp \
	src/compat.cpp \
	src/d2v.cpp \
	src/d2vsource.cpp \
	src/decode.cpp \
	src/directrender.cpp \
	src/vapoursynth.cpp

libd2vsource_la_LDFLAGS = -no-undefined -avoid-version  -lavutil -lavformat -lavcodec -lvapoursynth
