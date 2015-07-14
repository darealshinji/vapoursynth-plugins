ifneq ($(V),1)
CXXLD_silent  = @ echo '  CXXLD    '$@;
CXX_silent    = @ echo '  CXX      '$@;
AR_silent     = @ echo '  AR       '$@;
RANLIB_silent = @ echo '  RANLIB   '$@;
endif

LIB = libwaifu2x.so
LIB_STATIC = libwaifu2x.a
CXXFLAGS = -Dwaifu2x_EXPORTS -O3 -fPIC -Iinclude -fopenmp -std=c++11

OBJS = waifu2x.o common/model.o avx/avx_impl.o dft_avx/dft_avx_impl.o


all: $(LIB) $(LIB_STATIC)

clean:
	rm -f $(OBJS) $(LIB) $(LIB_STATIC)

$(LIB): $(OBJS)
	$(CXXLD_silent)$(CXX) -fopenmp -shared -Wl,-soname,$@ -o $@ $^

$(LIB_STATIC): $(LIB)
	$(AR_silent)ar cru $@ $(OBJS)
	$(RANLIB_silent)ranlib $@

waifu2x.o: waifu2x.cpp
	$(CXX_silent)$(CXX) $(CXXFLAGS) -o $@ -c $<

common/model.o: common/model.cpp
	$(CXX_silent)$(CXX) $(CXXFLAGS) -o $@ -c $<

avx/avx_impl.o: avx/avx_impl.cpp
	$(CXX_silent)$(CXX) $(CXXFLAGS) -mfma -mavx2 -o $@ -c $<

dft_avx/dft_avx_impl.o: dft_avx/dft_avx_impl.cpp
	$(CXX_silent)$(CXX) $(CXXFLAGS) -mfma -mavx2 -o $@ -c $<

