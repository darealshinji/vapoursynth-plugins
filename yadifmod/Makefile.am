
lib_LTLIBRARIES = libyadifmod.la

libyadifmod_la_SOURCES = src/Yadifmod.cpp

# "-I.." to find the vectorclass headers
libyadifmod_la_CXXFLAGS = $(AM_CXXFLAGS) -I.. -std=c++11 -msse2 -DVS_TARGET_CPU_X86

