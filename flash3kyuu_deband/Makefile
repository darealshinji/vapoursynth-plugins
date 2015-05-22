
ifeq ($(shell uname -p),x86_64)
LIB = libf3kdb.so
local_CXXFLAGS = -Isrc/include -DHAVE_ALIGNAS=1 -fvisibility=hidden -fvisibility-inlines-hidden
endif

include ../global.inc

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

%_sse2.o: %_sse2.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -msse2 -o $@ $^

%_ssse3.o: %_ssse3.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -mssse3 -o $@ $^

%_sse4.o: %_sse4.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -msse4.1 -o $@ $^

%.o: %.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -o $@ $^

$(LIB): $(CXXOBJS)
	$(CXXLD_silent)$(CXX) $(VSLDFLAGS) -o $@ $(CXXOBJS) $(LIBS)

