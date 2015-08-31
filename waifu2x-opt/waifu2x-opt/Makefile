ifneq ($(V),1)
CXXLD_silent  = @ echo '  CXXLD    '$@;
CXX_silent    = @ echo '  CXX      '$@;
AR_silent     = @ echo '  AR       '$@;
RANLIB_silent = @ echo '  RANLIB   '$@;
endif

LIB = libwaifu2x.so
LIB_STATIC = libwaifu2x.a

local_CXXFLAGS = -c -Wall -Wextra -Werror=format-security -Dwaifu2x_EXPORTS -Iinclude -fPIC -fopenmp $(CXXFLAGS) $(CPPFLAGS)
local_LDFLAGS  = -fopenmp -Wl,-z,relro -Wl,-z,noexecstack -Wl,-z,defs -Wl,--as-needed $(LDFLAGS)

OBJS = waifu2x.o common/model.o avx/avx_impl.o dft_avx/dft_avx_impl.o


all: $(LIB) $(LIB_STATIC)

clean:
	rm -f $(OBJS) $(LIB) $(LIB_STATIC)

$(LIB): $(OBJS)
	$(CXXLD_silent)$(CXX) -shared -Wl,-soname,$@ $(local_LDFLAGS) -o $@ $^

$(LIB_STATIC): $(LIB)
	$(AR_silent)ar cru $@ $(OBJS)
	$(RANLIB_silent)ranlib $@

waifu2x.o: waifu2x.cpp
	$(CXX_silent)$(CXX) $(local_CXXFLAGS) -o $@ $<

common/model.o: common/model.cpp
	$(CXX_silent)$(CXX) $(local_CXXFLAGS) -o $@ $<

avx/avx_impl.o: avx/avx_impl.cpp
	$(CXX_silent)$(CXX) $(local_CXXFLAGS) -mfma -mavx2 -o $@ $<

dft_avx/dft_avx_impl.o: dft_avx/dft_avx_impl.cpp
	$(CXX_silent)$(CXX) $(local_CXXFLAGS) -mfma -mavx2 -o $@ $<

