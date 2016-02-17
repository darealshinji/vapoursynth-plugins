include ../../config.mak

ifeq ($(HAVE_AVCODEC),yes)
ifeq ($(HAVE_AVUTIL),yes)
ifeq ($(HAVE_AVFORMAT),yes)
ifeq ($(HAVE_SWSCALE),yes)
ifeq ($(HAVE_ZLIB),yes)
FFMS2 = yes
endif #ZLIB
endif #SWSCALE
endif #AVFORMAT
endif #AVUTIL
endif #AVCODEC


ifeq ($(FFMS2),yes)

LIB = libffms2.so
local_CXXFLAGS = \
	-Isrc/include \
	-Isrc/src/config \
	-D_FILE_OFFSET_BITS=64 \
	-DFFMS_EXPORTS \
	-D__STDC_CONSTANT_MACROS \
	-fvisibility=hidden \
	-Wno-missing-field-initializers
LIBADD = -lavcodec -lavutil -lavformat -lswscale -lz

ifeq ($(HAVE_AVRESAMPLE),yes)
local_CXXFLAGS += -DWITH_AVRESAMPLE=1
LIBADD += -lavresample
endif

endif #FFMS2


include ../../global.inc

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
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -o $@ $^

$(LIB): $(CXXOBJS)
	$(CXXLD_silent)$(CXX) $(VSLDFLAGS) -o $@ $^ $(LIBADD) $(LIBS)

