include ../global.inc

lib_LTLIBRARIES = libcombmask.la

libcombmask_la_SOURCES = \
	src/adapt_motion.c \
	src/combmask.c \
	src/horizontal_dilation.c \
	src/is_combed.c \
	src/merge_frames.c \
	src/write_combmask.c

libcombmask_la_CFLAGS = $(AM_CFLAGS) -std=gnu99 -msse2

