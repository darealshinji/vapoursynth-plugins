AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter

AM_CPPFLAGS = $(VapourSynth_CFLAGS)

lib_LTLIBRARIES = libwwxd.la

BUILT_SOURCES = src/wwxd.o src/detection.o
CLEANFILES = $(BUILT_SOURCES)

libwwxd_la_SOURCES = src/wwxd.c src/detection.c

libwwxd_la_LDFLAGS = -no-undefined -avoid-version
