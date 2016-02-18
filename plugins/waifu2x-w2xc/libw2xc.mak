include ../../config.mak

ifneq ($(V),1)
GEN_silent    = @ echo '  GEN      '$@;
CXX_silent    = @ echo '  CXX      '$@;
CCLD_silent   = @ echo '  CCLD     '$@;
CC_silent     = @ echo '  CC       '$@;
AR_silent     = @ echo '  AR       '$@;
endif

d = libw2xc/

CPPFLAGS += -DBUILD_TS=\"\" -DHAVE_OPENCV -DX86OPT -Dw2xc_EXPORTS -DPIC
CXXFLAGS += -Wno-unused-result
CXXFLAGS += -std=gnu++11 -fPIC -fopenmp
CXXFLAGS += -Iwaifu2x -I$(d) -I$(d)src -I$(d)include -I/usr/include/opencv

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

clean:
	rm -f $(d)*.a $(d)*.o $(d)src/*.o $(d)conv
	rm -f modelHandler_sse.ii modelHandler_sse.s $(d)modelHandler_OpenCL.cl.h

$(d)libw2xc.a: $(OBJS)
	$(AR_silent)$(AR) cru $@ $^

$(OBJS): $(d)modelHandler_OpenCL.cl.h

%.o: %.cpp
	$(CXX_silent)$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

$(d)src/modelHandler_avx.o: CXXFLAGS+=-mavx
$(d)src/modelHandler_fma.o: CXXFLAGS+=-mfma
$(d)src/modelHandler_sse.o: CXXFLAGS+=-save-temps -msse3

$(d)modelHandler_OpenCL.cl.h: $(d)conv
	$(GEN_silent)$< $(d)src/modelHandler_OpenCL.cl $@ str

$(d)conv: $(d)conv.o
	$(CCLD_silent)$(CC) -o $@ $^

$(d)conv.o: $(d)conv.c
	$(CC_silent)$(CC) -o $@ -c $<

