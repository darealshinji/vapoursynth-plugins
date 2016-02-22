
OPENCV_LIBS = -lopencv_core -lopencv_highgui -lopencv_imgproc

LIBNAME = waifu2x-w2xc
local_CXXFLAGS = -Ilibw2xc/src -Wno-missing-field-initializers
local_LDFLAGS = -fopenmp -pthread
LIBS = libw2xc/libw2xc.a $(OPENCV_LIBS) -ldl -lpthread

include ../../cxx.inc

