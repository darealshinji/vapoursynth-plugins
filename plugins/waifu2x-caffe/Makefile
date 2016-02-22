include ../../config.mak

# libcaffe-dev nvidia-cuda-dev libboost-filesystem-dev libboost-iostreams-dev libboost-system-dev
# libgflags-dev libgoogle-glog-dev libhdf5-dev libopenblas-dev libopencv3-dev libprotobuf-dev

ifeq ($(ENABLE_WAIFU2X_CAFFE),yes)
LIBNAME = waifu2x-caffe
endif

local_CXXFLAGS = -Iinclude -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare -Wno-switch -Wno-missing-field-initializers
ifeq ($(HAVE_CUDNN_H),yes)
local_CXXFLAGS += -DHAVE_CUDNN_H
endif

LIBS  = -lcaffe -lcudart
LIBS += -lboost_filesystem -lboost_iostreams -lboost_system
LIBS += -lgflags -lglog
LIBS += $(HDF5_LIBS)
LIBS += -lopenblas
LIBS += -lopencv_core -lopencv_imgcodecs -lopencv_imgproc
LIBS += -lprotobuf -lm

include ../../cxx.inc

