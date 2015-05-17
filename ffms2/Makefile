
LIB = libffms2.so
local_CXXFLAGS = \
	-Isrc/include \
	-Isrc/src/config \
	-D_FILE_OFFSET_BITS=64 \
	-DFFMS_EXPORTS \
	-D__STDC_CONSTANT_MACROS \
	-DWITH_AVRESAMPLE=1 \
	-std=c++11 \
	-fvisibility=hidden \
	-Wno-missing-field-initializers
LIBADD = -lavcodec -lavresample -lz

include ../global.inc

CXXSRCS = \
	src/src/core/audiosource.cpp \
	src/src/core/ffms.cpp \
	src/src/core/filehandle.cpp \
	src/src/core/indexing.cpp \
	src/src/core/lavfaudio.cpp \
	src/src/core/lavfindexer.cpp \
	src/src/core/lavfvideo.cpp \
	src/src/core/track.cpp \
	src/src/core/utils.cpp \
	src/src/core/videosource.cpp \
	src/src/core/videoutils.cpp \
	src/src/core/wave64writer.cpp \
	src/src/core/zipfile.cpp \
	src/src/vapoursynth/vapoursource.cpp \
	src/src/vapoursynth/vapoursynth.cpp

%.o: %.cpp
	$(CXX) $(VSCXXFLAGS) -o $@ $^

$(LIB): $(CXXOBJS)
	$(CXX) $(VSLDFLAGS) -o $@ $^ $(LIBADD) $(LIBS)

../config.mak:
	cd .. && ./configure

