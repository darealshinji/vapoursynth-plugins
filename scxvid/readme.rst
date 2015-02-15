Description
===========

Scene change detection plugin for VapourSynth, using xvidcore.

Converted from the Avisynth plugin written by Fredrik Mellbin.


Usage
=====
::

    scxvid.Scxvid(clip clip[, string log="", bint use_slices=True])

Parameters:
    *clip*
        Input clip. Must be YUV420P8 with constant format and dimensions.

    *log*
        Name of the xvid first pass log file.

    *use_slices*
        This should make Scxvid faster, at the cost of slight differences in
        the scene change detection.

The *log* parameter is optional, because the ``_SceneChangePrev`` property
will be attached to every frame. Thus some users may not need xvid's log file.

For correct scene change detection, one must request all the frames, starting
at 0, strictly in ascending order. It's probably best if Scxvid is the last
filter in the chain.


Compilation
===========

::

    ./autogen.sh
    ./configure
    make

Note: when compiling for Windows, you must prefix the xvidcore library's file
name(s) (xvidcore.dll and xvidcore.dll.a, or xvidcore.a) with "lib", or
Scxvid's build system won't pick it up. This is thanks to xvidcore's special
snowflake build system, which reinvents libtool.
