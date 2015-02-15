include ../global.inc

lib_LTLIBRARIES = libdamb.la

libdamb_la_SOURCES = src/entrypoint.cpp \
					 src/read.cpp \
					 src/write.cpp \
					 src/shared.h

libdamb_la_CXXFLAGS = -std=c++11 -Wshadow

libdamb_la_CPPFLAGS = $(VapourSynth_CFLAGS) $(SNDFILE_CFLAGS)

libdamb_la_LDFLAGS = $(XGLOBAL_LDFLAGS) $(PLUGINLDFLAGS)

libdamb_la_LIBADD = $(SNDFILE_LIBS)
