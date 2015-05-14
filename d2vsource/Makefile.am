
lib_LTLIBRARIES = libd2vsource.la

libd2vsource_la_SOURCES = \
	src/applyrff.cpp \
	src/compat.cpp \
	src/d2v.cpp \
	src/d2vsource.cpp \
	src/decode.cpp \
	src/directrender.cpp \
	src/vapoursynth.cpp

libd2vsource_la_CXXFLAGS = $(AM_CXXFLAGS) -fpermissive
libd2vsource_la_LIBADD = -lavcodec -lavformat -lavutil
