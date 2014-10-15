AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter

AM_CPPFLAGS = $(VapourSynth_CFLAGS)

lib_LTLIBRARIES = libhistogram.la

libhistogram_la_SOURCES = src/classic.c src/color.c src/color2.c src/histogram.c src/levels.c src/luma.c

libhistogram_la_LDFLAGS = -no-undefined -avoid-version

libhistogram_la_LIBS = -lm
