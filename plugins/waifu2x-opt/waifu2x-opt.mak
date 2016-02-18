
LIBNAME = waifu2x-opt
ZIMG    = libzimg/libzimg.a
W2X     = libwaifu2x/libwaifu2x.a

$(LIB): $(W2X) $(ZIMG)

local_CXXFLAGS = -Wno-unused-local-typedefs -Wno-return-type -Ilibwaifu2x/include -Ilibzimg/API
local_LDFLAGS  = -fopenmp
LIBS = $(W2X) $(ZIMG)

include ../../cxx.inc

