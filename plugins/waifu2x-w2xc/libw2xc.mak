include ../../config.mak

ifneq ($(V),1)
GEN_silent    = @ echo '  GEN      '$@;
CXX_silent    = @ echo '  CXX      '$@;
CCLD_silent   = @ echo '  CCLD     '$@;
CC_silent     = @ echo '  CC       '$@;
AR_silent     = @ echo '  AR       '$@;
endif

d = libw2xc/

local_CXXFLAGS  = -O3 -Wall -Wextra -fPIC -DPIC -fopenmp
local_CXXFLAGS += -Wno-switch -Wno-sign-compare -Wno-maybe-uninitialized -Wno-unused-parameter
local_CXXFLAGS += -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-result
local_CXXFLAGS += -DBUILD_TS=\"\" -DHAVE_OPENCV -DX86OPT -Dw2xc_EXPORTS
local_CXXFLAGS += -Iwaifu2x -I$(d) -I$(d)src -I$(d)include -I/usr/include/opencv
local_CXXFLAGS += $(CXXFLAGS) $(CPPFLAGS)

SRCS = $(d)src/modelHandler.cpp \
	$(d)src/modelHandler_avx.cpp \
	$(d)src/modelHandler_fma.cpp \
	$(d)src/modelHandler_sse.cpp \
	$(d)src/modelHandler_OpenCL.cpp \
	$(d)src/convertRoutine.cpp \
	$(d)src/threadPool.cpp \
	$(d)src/modelHandler_CUDA.cpp \
	$(d)src/w2xconv.cpp \
	$(d)src/common.cpp \
	$(d)src/cvwrap.cpp \
	$(d)src/Env.cpp \
	$(d)src/Buffer.cpp

OBJS = $(SRCS:.cpp=.o)


all: $(d)libw2xc.a

clean distclean:
	rm -f $(d)*.a $(d)*.o $(d)src/*.o $(d)conv
	rm -f modelHandler_sse.ii modelHandler_sse.s $(d)modelHandler_OpenCL.cl.h

$(d)libw2xc.a: $(OBJS)
	$(AR_silent)$(AR) cru $@ $^

$(OBJS): $(d)modelHandler_OpenCL.cl.h

%.o: %.cpp
	$(CXX_silent)$(CXX) -c $(local_CXXFLAGS) -o $@ $<

$(d)src/modelHandler_avx.o: local_CXXFLAGS+=-mavx
$(d)src/modelHandler_fma.o: local_CXXFLAGS+=-mfma
$(d)src/modelHandler_sse.o: local_CXXFLAGS+=-save-temps -msse3

$(d)modelHandler_OpenCL.cl.h: $(d)conv
	$(GEN_silent)$< $(d)src/modelHandler_OpenCL.cl $@ str

$(d)conv: $(d)conv.o
	$(CCLD_silent)$(CC) -o $@ $^

$(d)conv.o: $(d)conv.c
	$(CC_silent)$(CC) -o $@ -c $<

