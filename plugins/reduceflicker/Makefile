
LIB = libreduceflicker.so

include ../config.mak

ifeq ($(HAVE_I686),yes)
# linking fails on i386 with -fstack-protector-all
local_CXXFLAGS = -fstack-protector --param=ssp-buffer-size=4
endif

include ../cxx.inc
