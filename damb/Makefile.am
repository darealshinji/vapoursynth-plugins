
lib_LTLIBRARIES = libdamb.la

libdamb_la_SOURCES = src/entrypoint.cpp \
					 src/read.cpp \
					 src/write.cpp \
					 src/shared.h

libdamb_la_CXXFLAGS = $(AM_CXXFLAGS) -std=c++11 -Wshadow $(SNDFILE_CFLAGS)

libdamb_la_LIBADD = $(SNDFILE_LIBS)
