AM_CFLAGS = -Wall -Wextra -Wno-unused-parameter

AM_CXXFLAGS = -Wall -Wextra -Wno-unused-parameter

AM_CPPFLAGS = $(VapourSynth_CFLAGS) -I/usr/local/include -I/usr/include

XGLOBAL_LDFLAGS = -no-undefined -avoid-version
