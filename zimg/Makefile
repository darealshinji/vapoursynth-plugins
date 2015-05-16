
LIB = libzimg.so
$(LIB): src/vszimg/vszimg.o
local_CXXFLAGS = -std=c++11 -Isrc/API -Wno-missing-field-initializers

include ../global.inc

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

%_sse2.o: %_sse2.cpp
	$(CXX) $(VSCXXFLAGS) -msse2 -o $@ $^

%_avx2.o: %_avx2.cpp
	$(CXX) $(VSCXXFLAGS) -mavx2 -mfma -mf16c -o $@ $^

%_f16c.o: %_f16c.cpp
	$(CXX) $(VSCXXFLAGS) -mf16c -o $@ $^

src/vszimg/vszimg.o: src/vszimg/vszimg.c
	$(CC) $(VSCFLAGS) -Isrc/API -o $@ $^

%.o: %.cpp
	$(CXX) $(VSCXXFLAGS) -o $@ $^

$(LIB): $(CXXOBJS)
	$(CXX) $(VSLDFLAGS) -o $@ $(CXXOBJS) src/vszimg/vszimg.o $(LIBS)

