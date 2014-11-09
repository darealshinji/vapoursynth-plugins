AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter

AM_CXXFLAGS = $(AM_CFLAGS)

AM_CPPFLAGS = $(VapourSynth_CFLAGS) -I/usr/local/include -I/usr/include

XGLOBAL_LDFLAGS = -no-undefined -avoid-version -Wl,-z,noexecstack
