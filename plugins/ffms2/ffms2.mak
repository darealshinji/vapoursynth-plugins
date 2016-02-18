include ../../config.mak

LIBNAME = ffms2
local_CXXFLAGS = \
	-Isrc/include \
	-Isrc/src/config \
	-I../../ffmpeg_libs/include \
	-DWITH_AVRESAMPLE=1 \
	-D_FILE_OFFSET_BITS=64 \
	-DFFMS_EXPORTS \
	-D__STDC_CONSTANT_MACROS \
	-fvisibility=hidden \
	-Wno-missing-field-initializers

LIBADD = -L../../ffmpeg_libs/lib -lavcodec -lavutil -lavformat -lswscale -lavresample -lz

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

