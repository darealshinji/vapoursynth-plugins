include ../../config.mak

ifneq ($(V),1)
CXX_silent    = @ echo '  CXX      '$@;
AR_silent     = @ echo '  AR       '$@;
endif

local_CXXFLAGS  = -O3 -Wall -Wextra -Werror=format-security -fPIC -DPIC
local_CXXFLAGS += -Wno-missing-field-initializers -Wno-unused-parameter -Wno-unused-but-set-parameter
local_CXXFLAGS += -I$(d) -I$(d)API -I$(d)Colorspace -I$(d)Common -I$(d)Depth -I$(d)Resize -I$(d)Unresize
local_CXXFLAGS += -mfpmath=sse -msse2 -DZIMG_X86 $(CXXFLAGS) $(CPPFLAGS)

d = libzimg/

LIB = $(d)libzimg.a

OBJS = $(SRCS:.cpp=.o)
SRCS = $(d)API/zimg.cpp \
	$(d)Colorspace/colorspace.cpp \
	$(d)Colorspace/colorspace_param.cpp \
	$(d)Colorspace/graph.cpp \
	$(d)Colorspace/matrix3.cpp \
	$(d)Colorspace/operation.cpp \
	$(d)Colorspace/operation_impl.cpp \
	$(d)Depth/depth_convert.cpp \
	$(d)Depth/depth.cpp \
	$(d)Depth/dither.cpp \
	$(d)Depth/dither_impl.cpp \
	$(d)Depth/error_diffusion.cpp \
	$(d)Resize/filter.cpp \
	$(d)Resize/resize.cpp \
	$(d)Resize/resize_impl.cpp \
	$(d)Unresize/bilinear.cpp \
	$(d)Unresize/unresize.cpp \
	$(d)Unresize/unresize_impl.cpp \
	$(d)Colorspace/operation_impl_x86.cpp \
	$(d)Depth/depth_convert_x86.cpp \
	$(d)Depth/dither_impl_x86.cpp \
	$(d)Resize/resize_impl_x86.cpp \
	$(d)Unresize/unresize_impl_x86.cpp \
	$(d)Colorspace/operation_impl_sse2.cpp \
	$(d)Depth/depth_convert_sse2.cpp \
	$(d)Depth/dither_impl_sse2.cpp \
	$(d)Resize/resize_impl_sse2.cpp \
	$(d)Unresize/unresize_impl_sse2.cpp \
	$(d)Colorspace/operation_impl_avx2.cpp \
	$(d)Depth/depth_convert_avx2.cpp \
	$(d)Depth/dither_impl_avx2.cpp \
	$(d)Resize/resize_impl_avx2.cpp \
	$(d)Unresize/unresize_impl_avx2.cpp


all: $(LIB)

clean distclean:
	rm -f $(OBJS) $(LIB)

$(LIB): $(OBJS)
	$(AR_silent)$(AR) cru $@ $^

%_avx2.o: local_CXXFLAGS+=-mavx2 -mfma -mf16c

%.o: %.cpp
	$(CXX_silent)$(CXX) -c $(local_CXXFLAGS) -o $@ $<

