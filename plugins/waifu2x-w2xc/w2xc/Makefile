ifneq ($(V),1)
AR_silent     = @ echo '  AR       '$@;
RANLIB_silent = @ echo '  RANLIB   '$@;
V=0
endif


MAKE = make
AR = ar
RANLIB = ranlib

OBJS = src/modelHandler.o \
	src/modelHandler_avx.o src/modelHandler_fma.o src/modelHandler_sse.o \
	src/modelHandler_OpenCL.o src/convertRoutine.o src/threadPool.o \
	src/modelHandler_CUDA.o src/w2xconv.o src/common.o \
	src/cvwrap.o src/Env.o src/Buffer.o


all: libw2xc.a

clean:
	$(MAKE) -f Makefile.linux clean
	rm -f conv *.a *.so src/*.o src/*.d

libw2xc.a: $(OBJS)
	$(AR_silent)$(AR) cru $@ $^
	$(RANLIB_silent)$(RANLIB) $@

$(OBJS): src/modelHandler_CUDA.ptx20 src/modelHandler_CUDA.ptx30
	$(MAKE) -f Makefile.linux libw2xc.so V=$(V)

# I don't know if this actually breaks things
src/modelHandler_CUDA.ptx20 src/modelHandler_CUDA.ptx30:
	touch $@

