
LIBNAME = zimg
local_CXXFLAGS = -Wno-missing-field-initializers -Isrc/API

include ../../global.inc

$(LIB): src/vszimg/vszimg.o

CXXSRCS = src/API/zimg.cpp \
		src/Colorspace/colorspace.cpp \
		src/Colorspace/colorspace_param.cpp \
		src/Colorspace/graph.cpp \
		src/Colorspace/matrix3.cpp \
		src/Colorspace/operation.cpp \
		src/Colorspace/operation_impl.cpp \
		src/Colorspace/operation_impl_x86.cpp \
		src/Depth/depth_convert.cpp \
		src/Depth/depth.cpp \
		src/Depth/dither.cpp \
		src/Depth/dither_impl.cpp \
		src/Depth/error_diffusion.cpp \
		src/Resize/filter.cpp \
		src/Resize/resize.cpp \
		src/Resize/resize_impl.cpp \
		src/Resize/resize_impl_x86.cpp \
		src/Colorspace/operation_impl_sse2.cpp \
		src/Resize/resize_impl_sse2.cpp \
		src/Colorspace/operation_impl_avx2.cpp \
		src/Resize/resize_impl_avx2.cpp \
		src/Colorspace/operation_impl_f16c.cpp

%_avx2.o: VSCXXFLAGS+=-mavx2 -mfma -mf16c
%_f16c.o: VSCXXFLAGS+=-mf16c

src/vszimg/vszimg.o: src/vszimg/vszimg.c
	$(CC_silent)$(CC) $(VSCFLAGS) -Isrc/API -o $@ $^

%.o: %.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -o $@ $^

$(LIB): $(CXXOBJS)
	$(CXXLD_silent)$(CXX) $(VSLDFLAGS) -o $@ $(CXXOBJS) src/vszimg/vszimg.o $(LIBS)

