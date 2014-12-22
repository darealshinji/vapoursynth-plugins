include ../global.inc

ACLOCAL_AMFLAGS = -I m4

if x86_64
lib_LTLIBRARIES = libf3kdb.la

libf3kdb_la_SOURCES = \
	src/auto_utils.cpp \
	src/core.cpp \
	src/flash3kyuu_deband_impl_c.cpp \
	src/flash3kyuu_deband_impl_sse2.cpp \
	src/flash3kyuu_deband_impl_ssse3.cpp \
	src/flash3kyuu_deband_impl_sse4.cpp \
	src/impl_dispatch.cpp \
	src/process_plane_context.cpp \
	src/public_interface.cpp \
	src/random.cpp \
	src/vapoursynth/plugin.cpp

libf3kdb_la_CXXFLAGS = -I./include  -DHAVE_ALIGNAS=1  \
			-std=c++11 -fvisibility=hidden -fvisibility-inlines-hidden -ggdb -ftrapv  \
			-msse2 -mssse3 -msse4.1

libf3kdb_la_LDFLAGS = $(XGLOBAL_LDFLAGS)
endif

