include ../../config.mak

ifeq ($(HAVE_OPENCL),yes)
ifeq ($(HAVE_BOOST_FILESYSTEM),yes)
ifeq ($(HAVE_BOOST_SYSTEM),yes)
LIBNAME = nnedi3cl
LIBADD = $(CL_LIBS) -lboost_filesystem -lboost_system
local_CXXFLAGS = \
	-Wno-deprecated-declarations \
	-Wno-ignored-qualifiers \
	$(BOOST_CPPFLAGS) \
	$(CL_CFLAGS) \
	-DNNEDI3_DATADIR='"$(datarootdir)/nnedi3"'
endif
endif
endif

include ../../cxx.inc

