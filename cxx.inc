
include ../global.inc

$(LIB): $(CXXOBJS)
	$(CXX) $(VSLDFLAGS) -o $@ $(CXXOBJS) $(LIBADD) $(LIBS)

%.o: %.cpp
	$(CXX) $(VSCXXFLAGS) -o $@ $^

