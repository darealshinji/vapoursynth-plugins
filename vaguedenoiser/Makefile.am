include ../global.inc

lib_LTLIBRARIES = libvaguedenoiser.la

libvaguedenoiser_la_SOURCES = src/VagueDenoiser.cpp

libvaguedenoiser_la_CXXFLAGS = -std=c++11 -mfpmath=sse

libvaguedenoiser_la_CPPFLAGS = -DVS_TARGET_CPU_X86

libvaguedenoiser_la_LDFLAGS = $(XGLOBAL_LDFLAGS)

