AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter -std=gnu99

AM_CPPFLAGS = $(VapourSynth_CFLAGS)

lib_LTLIBRARIES = libvautodeint.la

libvautodeint_la_SOURCES = \
	src/IsCombedTIVTCport.c \
	src/PlaneDifferenceFromPrevious.c \
	src/PluginInit.c

libvautodeint_la_LDFLAGS = -no-undefined -avoid-version
