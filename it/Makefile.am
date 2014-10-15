AM_CXXFLAGS = -Wall -Wextra -Wno-unused-parameter -D__SSE -mssse3

AM_CPPFLAGS = $(VapourSynth_CFLAGS)

lib_LTLIBRARIES = libit.la

libit_la_SOURCES = \
	src/vs_it_c.cpp \
	src/vs_it.cpp \
	src/vs_it_interface.cpp \
	src/vs_it_mmx.cpp \
	src/vs_it_process.cpp \
	src/vs_it_sse.cpp

libit_la_LDFLAGS = -no-undefined -avoid-version
