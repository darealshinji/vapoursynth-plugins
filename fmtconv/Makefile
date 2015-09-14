
LIB = libfmtconv.so

ifeq ($(shell uname -p),x86_64)
local_CXXFLAGS = -mcx16
endif

include ../global.inc

CXXSRCS = src/AvstpWrapper.cpp \
        src/main.cpp \
        src/fmtc/Bitdepth.cpp \
        src/fmtc/Convert.cpp \
        src/fmtc/ConvStep.cpp \
        src/fmtc/fnc.cpp \
        src/fmtcl/ArrayMultiType.cpp \
        src/fmtcl/BitBltConv_avx2.cpp \
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
        src/fmtcl/FilterResize.cpp \
        src/fmtcl/fnc.cpp \
        src/fmtcl/KernelData.cpp \
        src/fmtcl/Matrix2020CLProc.cpp \
        src/fmtcl/MatrixProc_avx2.cpp \
        src/fmtcl/MatrixProc_avx.cpp \
        src/fmtcl/MatrixProc.cpp \
        src/fmtcl/ResampleSpecPlane.cpp \
        src/fmtcl/ResizeData.cpp \
        src/fmtcl/ResizeDataFactory.cpp \
        src/fmtcl/Scaler_avx2.cpp \
        src/fmtcl/Scaler.cpp \
        src/fmtcl/TransLut_avx2.cpp \
        src/fmtcl/TransLut.cpp \
        src/fmtcl/TransOp2084.cpp \
        src/fmtcl/TransOpAffine.cpp \
        src/fmtcl/TransOpCanonLog.cpp \
        src/fmtcl/TransOpFilmStream.cpp \
        src/fmtcl/TransOpLinPow.cpp \
        src/fmtcl/TransOpLogC.cpp \
        src/fmtcl/TransOpLogTrunc.cpp \
        src/fmtcl/TransOpPow.cpp \
        src/fmtcl/TransOpSLog.cpp \
        src/fmtcl/VoidAndCluster.cpp \
        src/fmtc/Matrix2020CL.cpp \
        src/fmtc/Matrix.cpp \
        src/fmtc/NativeToStack16.cpp \
        src/fmtc/Resample.cpp \
        src/fmtc/Stack16ToNative.cpp \
        src/fmtc/Transfer.cpp \
        src/fstb/CpuId.cpp \
        src/fstb/fnc.cpp \
        src/fstb/ToolsAvx2.cpp \
        src/fstb/ToolsSse2.cpp \
        src/vsutl/CpuOpt.cpp \
        src/vsutl/FilterBase.cpp \
        src/vsutl/fnc.cpp \
        src/vsutl/PlaneProcCbInterface.cpp \
        src/vsutl/PlaneProcessor.cpp

%avx.o: %avx.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -mavx -o $@ $^

%vx2.o: %vx2.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -mavx2 -o $@ $^

%.o: %.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -o $@ $^

$(LIB): $(CXXOBJS)
	$(CXXLD_silent)$(CXX) $(VSLDFLAGS) -o $@ $(CXXOBJS) $(LIBS)

