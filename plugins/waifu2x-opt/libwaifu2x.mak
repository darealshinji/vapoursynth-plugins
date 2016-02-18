AR ?= ar
$(CXX) ?= g++

ifneq ($(V),1)
CXX_silent    = @ echo '  CXX      '$@;
AR_silent     = @ echo '  AR       '$@;
endif

LIB = $(d)libwaifu2x.a

d = libwaifu2x/

local_CXXFLAGS  = -std=c++11 -c -Wall -Wextra -Werror=format-security -Dwaifu2x_EXPORTS
local_CXXFLAGS += -I$(d)include -fPIC -DPIC -fopenmp $(CXXFLAGS) $(CPPFLAGS)
local_LDFLAGS   = -fopenmp -Wl,-z,relro -Wl,-z,noexecstack -Wl,-z,defs -Wl,--as-needed $(LDFLAGS)

OBJS = $(d)waifu2x.o $(d)common/model.o $(d)avx/avx_impl.o $(d)dft_avx/dft_avx_impl.o


all: $(LIB)

clean:
	rm -f $(OBJS) $(LIB)

$(LIB): $(OBJS)
	$(AR_silent)$(AR) cru $@ $(OBJS)

%.o: %.cpp
	$(CXX_silent)$(CXX) $(local_CXXFLAGS) -o $@ $<

$(d)avx/avx_impl.o $(d)dft_avx/dft_avx_impl.o: local_CXXFLAGS+=-mfma -mavx2

