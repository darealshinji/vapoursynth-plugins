
include ../global.inc

ifeq ($(HAVE_YASM),yes)
_ASMOBJS = $(ASMOBJS)
endif

$(LIB): $(CXXOBJS) $(_ASMOBJS)
	$(CXXLD_silent)$(CXX) $(VSLDFLAGS) -o $@ $^ $(LIBADD) $(LIBS)

%.o: %.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -o $@ $^

