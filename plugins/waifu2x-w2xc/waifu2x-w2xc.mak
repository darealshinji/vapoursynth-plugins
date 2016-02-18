
OPENCV_LIBS = -lopencv_core -lopencv_highgui -lopencv_imgproc
#OPENCV_LIBS = $(shell pkg-config --libs opencv)

LIBNAME = waifu2x-w2xc
local_CXXFLAGS = -Ilibw2xc/src -Wno-missing-field-initializers
LIBS = libw2xc/libw2xc.a $(OPENCV_LIBS) -ldl -fopenmp -pthread -lpthread

include ../../cxx.inc

