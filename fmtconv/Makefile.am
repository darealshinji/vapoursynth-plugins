include ../global.inc

lib_LTLIBRARIES = libfmtconv.la

libfmtconv_la_SOURCES = \
	src/fmtc/Bitdepth.cpp \
	src/fmtc/ConvStep.cpp \
	src/fmtcl/BitBltConv.cpp \
	src/fmtcl/ChromaPlacement.cpp \
	src/fmtcl/CoefArrInt.cpp \
	src/fmtcl/ContFirBlackman.cpp \
	src/fmtcl/ContFirBlackmanMinLobe.cpp \
	src/fmtcl/ContFirCubic.cpp \
	src/fmtcl/ContFirFromDiscrete.cpp \
	src/fmtcl/ContFirGauss.cpp \
	src/fmtcl/ContFirInterface.cpp \
	src/fmtcl/ContFirLanczos.cpp \
	src/fmtcl/ContFirLinear.cpp \
	src/fmtcl/ContFirRect.cpp \
	src/fmtcl/ContFirSinc.cpp \
	src/fmtcl/ContFirSnh.cpp \
	src/fmtcl/ContFirSpline16.cpp \
	src/fmtcl/ContFirSpline36.cpp \
	src/fmtcl/ContFirSpline64.cpp \
	src/fmtcl/ContFirSpline.cpp \
	src/fmtcl/DiscreteFirCustom.cpp \
	src/fmtcl/DiscreteFirInterface.cpp \
	src/fmtcl/ErrDifBuf.cpp \
	src/fmtcl/ErrDifBufFactory.cpp \
	src/fmtcl/KernelData.cpp \
	src/fmtcl/ResampleSpecPlane.cpp \
	src/fmtcl/ResizeData.cpp \
	src/fmtcl/ResizeDataFactory.cpp \
	src/fmtc/Matrix2020CL.cpp \
	src/fmtc/Matrix.cpp \
	src/fmtc/NativeToStack16.cpp \
	src/fmtc/Stack16ToNative.cpp \
	src/fstb/CpuId.cpp \
	src/fstb/fnc.cpp \
	src/fstb/ToolsSse2.cpp \
	src/vsutl/FilterBase.cpp \
	src/vsutl/fnc.cpp \
	src/vsutl/PlaneProcCbInterface.cpp \
	src/vsutl/PlaneProcessor.cpp

#if WINDOWS
#libfmtconv_la_SOURCES += src/AvstpFinder.cpp src/AvstpWrapper.cpp
#endif

libfmtconv_la_CXXFLAGS = $(AM_CXXFLAGS) -Isrc -mfpmath=sse -msse2 -march=native -Wno-return-type


noinst_LTLIBRARIES = libavx2.la

libavx2_la_SOURCES = \
	src/fmtc/Convert.cpp \
	src/fmtc/Matrix_avx.cpp \
	src/fmtc/Resample.cpp \
	src/fmtc/Transfer.cpp \
	src/fmtc/Transfer_avx2.cpp \
	src/fmtcl/BitBltConv_avx2.cpp \
	src/fmtcl/FilterResize.cpp \
	src/fmtcl/Scaler.cpp \
	src/fmtcl/Scaler_avx2.cpp \
	src/fstb/ToolsAvx2.cpp \
	src/main.cpp

libavx2_la_CXXFLAGS = $(AM_CXXFLAGS) -Isrc -mavx2 -march=native -fabi-version=6 \
	-Wno-reorder -Wno-return-type -Wno-unused-variable


libfmtconv_la_LIBADD = libavx2.la

