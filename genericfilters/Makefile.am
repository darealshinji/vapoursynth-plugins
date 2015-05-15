
lib_LTLIBRARIES = libgenericfilters.la

libgenericfilters_la_SOURCES = \
	alone/alone.c \
	alone/binarize.c \
	alone/invert.c \
	alone/levels.c \
	alone/limiter.c \
	common/common.c \
	convolution/canny.c \
	convolution/convolution.c \
	convolution/convolution_hv.c \
	convolution/edge.c \
	convolution/edge_detect_canny.c \
	convolution/xxflate.c \
	neighbors/binarize2.c \
	neighbors/neighbors.c \
	specific/hysteresis.c

if X86
libgenericfilters_la_SOURCES += \
	convolution/simd/deflate_sse2.c \
	convolution/simd/edge_detect_canny_sse2.c \
	convolution/simd/float_to_dst_sse2.c \
	convolution/simd/gaussian_blur_sse2.c \
	convolution/simd/inflate_sse2.c \
	convolution/simd/prewitt_sse2.c \
	convolution/simd/proc_3x3_sse2.c \
	convolution/simd/proc_5x5_sse2.c \
	convolution/simd/proc_h_sse2.c \
	convolution/simd/proc_hv_sse2.c \
	convolution/simd/proc_v_sse2.c \
	convolution/simd/sobel_sse2.c \
	convolution/simd/tedge_sse2.c \
	neighbors/simd/maximum_sse2.c \
	neighbors/simd/median_sse2.c \
	neighbors/simd/minimum_sse2.c
else
libgenericfilters_la_SOURCES += \
	convolution/deflate.c \
	convolution/float_to_dst.c \
	convolution/gaussian_blur.c \
	convolution/inflate.c \
	convolution/prewitt.c \
	convolution/proc_3x3.c \
	convolution/proc_5x5.c \
	convolution/proc_h.c \
	convolution/proc_hv.c \
	convolution/proc_v.c \
	convolution/sobel.c \
	convolution/tedge.c \
	neighbors/maximum.c \
	neighbors/median.c \
	neighbors/minimum.c
endif

libgenericfilters_la_CFLAGS = $(AM_CFLAGS) -std=gnu99 \
	-I./alone -I./common -I./convolution -I./neighbors -I./specific \
	-ffast-math -fomit-frame-pointer -fno-zero-initialized-in-bss -fexcess-precision=fast \
	-fvisibility=hidden

if X86
libgenericfilters_la_CFLAGS += -DUSE_X86_INTRINSICS -msse2 -mfpmath=sse -Wno-unused-function
endif

libgenericfilters_la_LIBADD = -lm

