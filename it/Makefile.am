include ../global.inc

lib_LTLIBRARIES = libit.la

libit_la_SOURCES = \
	src/vs_it_c.cpp \
	src/vs_it.cpp \
	src/vs_it_interface.cpp \
	src/vs_it_mmx.cpp \
	src/vs_it_process.cpp \
	src/vs_it_sse.cpp

libit_la_CXXFLAGS = $(AM_CXXFLAGS) -D__SSE -mssse3

