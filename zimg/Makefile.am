include ../global.inc

AM_CXXFLAGS += -std=c++11
AM_CPPFLAGS += -I./API

lib_LTLIBRARIES    = libzimg.la
noinst_LTLIBRARIES = libsse2.la libavx2.la libf16c.la libvszimg.la

libzimg_la_SOURCES = \
		API/zimg.cpp \
		Colorspace/colorspace.cpp \
		Colorspace/colorspace_param.cpp \
		Colorspace/graph.cpp \
		Colorspace/matrix3.cpp \
		Colorspace/operation.cpp \
		Colorspace/operation_impl.cpp \
		Colorspace/operation_impl_x86.cpp \
		Depth/depth_convert.cpp \
		Depth/depth.cpp \
		Depth/dither.cpp \
		Depth/dither_impl.cpp \
		Depth/error_diffusion.cpp \
		Resize/filter.cpp \
		Resize/resize.cpp \
		Resize/resize_impl.cpp \
		Resize/resize_impl_x86.cpp

libsse2_la_SOURCES  = Colorspace/operation_impl_sse2.cpp \
			Resize/resize_impl_sse2.cpp
libsse2_la_CXXFLAGS = $(AM_CXXFLAGS) -msse2

libavx2_la_SOURCES  = Colorspace/operation_impl_avx2.cpp \
			Resize/resize_impl_avx2.cpp
libavx2_la_CXXFLAGS = $(AM_CXXFLAGS) -mavx2 -mfma -mf16c

libf16c_la_SOURCES  = Colorspace/operation_impl_f16c.cpp
libf16c_la_CXXFLAGS = $(AM_CXXFLAGS) -mf16c

libvszimg_la_SOURCES = vszimg/vszimg.c
libvszimg_la_CFLAGS  = $(AM_CFLAGS)

libzimg_la_LIBADD  = libsse2.la libavx2.la libf16c.la libvszimg.la
libzimg_la_LDFLAGS = $(XGLOBAL_LDFLAGS)

