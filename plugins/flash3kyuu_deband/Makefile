include ../../config.mak

ifeq ($(HAVE_X86_64),yes)
LIBNAME = f3kdb
local_CXXFLAGS = -Isrc/include -DHAVE_ALIGNAS=1

include ../../global.inc

CXXSRCS = \
	src/auto_utils.cpp \
	src/core.cpp \
	src/flash3kyuu_deband_impl_c.cpp \
	src/flash3kyuu_deband_impl_sse2.cpp \
	src/flash3kyuu_deband_impl_sse4.cpp \
	src/flash3kyuu_deband_impl_ssse3.cpp \
	src/impl_dispatch.cpp \
	src/process_plane_context.cpp \
	src/public_interface.cpp \
	src/random.cpp \
	src/vapoursynth/plugin.cpp

%_ssse3.o: %_ssse3.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -mssse3 -o $@ $^

%_sse4.o: %_sse4.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -msse4.1 -o $@ $^

%.o: %.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -o $@ $^

$(LIB): $(CXXOBJS)
	$(CXXLD_silent)$(CXX) $(VSLDFLAGS) -o $@ $(CXXOBJS) $(LIBS)

endif #HAVE_X86_64

