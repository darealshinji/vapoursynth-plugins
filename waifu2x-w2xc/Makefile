
LIB   = libwaifu2x-w2xc.so
W2XC  = w2xc/libw2xc.a

$(LIB): $(W2XC)

local_CXXFLAGS   = -I./w2xc/src
override_LDFLAGS = -Wl,-Bsymbolic-functions -Wl,-z,relro -Wl,-z,noexecstack -Wl,--as-needed
LIBS = $(W2XC) -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_features2d -fopenmp -ldl

clean-local:
	$(MAKE) -C w2xc clean

$(W2XC):
	$(MAKE) -C w2xc

include ../cxx.inc

# meh
$(LIB): src/Waifu2x-w2xc.o $(W2XC)
	$(CXX) -shared -Wl,-soname,$@ $(override_LDFLAGS) -o $@ $^ $(LIBS)

