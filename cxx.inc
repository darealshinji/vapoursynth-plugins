
include ../global.inc

$(LIB): $(CXXOBJS)
	$(CXXLD_silent)$(CXX) $(VSLDFLAGS) -o $@ $(CXXOBJS) $(LIBADD) $(LIBS)

%.o: %.cpp
	$(CXX_silent)$(CXX) $(VSCXXFLAGS) -o $@ $^

