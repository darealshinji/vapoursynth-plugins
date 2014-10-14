AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter

AM_CPPFLAGS = $(VapourSynth_CFLAGS) -D__SSE -mssse3

lib_LTLIBRARIES = libit.la

libit_la_SOURCES = \
	src/vs_it_c.cpp \
	src/vs_it.cpp \
	src/vs_it_interface.cpp \
	src/vs_it_mmx.cpp \
	src/vs_it_process.cpp \
	src/vs_it_sse.cpp

libit_la_LDFLAGS = -no-undefined -avoid-version
