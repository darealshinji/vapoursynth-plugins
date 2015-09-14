
LIB = libreduceflicker.so

ifneq ($(shell uname -p),x86_64)
# linking fails on i386 with -fstack-protector-all
local_CXXFLAGS = -fstack-protector
endif

include ../cxx.inc
