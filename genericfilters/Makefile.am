include ../global.inc

lib_LTLIBRARIES = libgenericfilters.la

libgenericfilters_la_SOURCES = \
	convolution/convolution.c \
	convolution/proc_3x3.c \
	convolution/proc_5x5.c \
	convolution/proc_h.c \
	convolution/proc_v.c \
	convolution/convolution_hv.c \
	convolution/proc_hv.c \
	convolution/xxflate.c \
	convolution/inflate.c \
	convolution/deflate.c \
	convolution/edge.c \
	convolution/sobel.c \
	convolution/prewitt.c \
	convolution/tedge.c \
	convolution/canny.c \
	convolution/gaussian_blur.c \
	convolution/float_to_dst.c \
	convolution/edge_detect_canny.c \
	neighbors/maximum.c \
	neighbors/median.c \
	neighbors/minimum.c \
	neighbors/neighbors.c \
	neighbors/binarize2.c \
	alone/invert.c \
	alone/limiter.c \
	alone/levels.c \
	alone/binarize.c \
	alone/alone.c \
	specific/hysteresis.c \
	common/common.c

libgenericfilters_la_CFLAGS = $(AM_CFLAGS) -I./alone -I./common -I./convolution -I./neighbors -I./specific

libgenericfilters_la_LDFLAGS = $(XGLOBAL_LDFLAGS)

libgenericfilters_la_LIBADD = -lm

