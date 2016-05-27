
include ../../global.inc

ifeq ($(HAVE_YASM),yes)
ifneq ($(NOASM),yes)
_ASMOBJS = $(ASMOBJS)
endif
endif

$(LIB): $(CXXOBJS) $(COBJS) $(_ASMOBJS)
	$(CXXLD_silent)$(CXX) $(VSLDFLAGS) -o $@ $^ $(LIBADD) $(LIBS)

%.o: %.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -o $@ $^

%.o: %.c
	$(CC_silent)$(CC) $(VSCFLAGS) -o $@ $^

