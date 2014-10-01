AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter -std=gnu99 -msse2

AM_CPPFLAGS = $(VapourSynth_CFLAGS)

lib_LTLIBRARIES = libcombmask.la

libcombmask_la_SOURCES = src/adapt_motion.c src/combmask.c src/horizontal_dilation.c \
	src/is_combed.c src/merge_frames.c src/write_combmask.c

libcombmask_la_LDFLAGS = -no-undefined -avoid-version
