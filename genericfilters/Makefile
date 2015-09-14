
LIB = libgenericfilters.so

local_CFLAGS = -Wno-unused-function -DUSE_X86_INTRINSICS \
	-Isrc/alone -Isrc/common -Isrc/convolution -Isrc/neighbors -Isrc/specific

LIBADD = -lm

include ../global.inc

CSRCS = \
	src/alone/alone.c \
	src/alone/binarize.c \
	src/alone/invert.c \
	src/alone/levels.c \
	src/alone/limiter.c \
	src/common/common.c \
	src/convolution/canny.c \
	src/convolution/convolution.c \
	src/convolution/convolution_hv.c \
	src/convolution/edge.c \
	src/convolution/edge_detect_canny.c \
	src/convolution/xxflate.c \
	src/neighbors/binarize2.c \
	src/neighbors/neighbors.c \
	src/specific/hysteresis.c \
	src/convolution/simd/deflate_sse2.c \
	src/convolution/simd/edge_detect_canny_sse2.c \
	src/convolution/simd/float_to_dst_sse2.c \
	src/convolution/simd/gaussian_blur_sse2.c \
	src/convolution/simd/inflate_sse2.c \
	src/convolution/simd/prewitt_sse2.c \
	src/convolution/simd/proc_3x3_sse2.c \
	src/convolution/simd/proc_5x5_sse2.c \
	src/convolution/simd/proc_h_sse2.c \
	src/convolution/simd/proc_hv_sse2.c \
	src/convolution/simd/proc_v_sse2.c \
	src/convolution/simd/sobel_sse2.c \
	src/convolution/simd/tedge_sse2.c \
	src/neighbors/simd/maximum_sse2.c \
	src/neighbors/simd/median_sse2.c \
	src/neighbors/simd/minimum_sse2.c

%.o: %.c
	$(CC_silent)$(CC) $(VSCFLAGS) -o $@ $^

$(LIB): $(COBJS)
	$(CCLD_silent)$(CC) $(VSLDFLAGS) -o $@ $(COBJS) $(LIBADD) $(LIBS)

