Description
===========

This is a port of the Avisynth plugin DegrainMedian, version 0.8.2.

DegrainMedian is a spatio-temporal limited median denoiser. It uses
various methods to replace every pixel with one selected from its 3x3
neighbourhood, from either the current, previous, or next frame.

The first column and the last column are simply copied from the source
frame. The first row and the last row are also copied from the source
frame. If interlaced=True, then the second row and the second-to-last
row are also copied from the source frame.


Usage
=====
::

    dgm.DegrainMedian(clip clip[, int[] limit=[4], int[] mode=[1], bint interlaced=False, bint norow=False, bint opt=True])


Parameters:
    *clip*
        Clip to process. Must be 8..16 bit with integer sample type.
        If there are any frames with unsupported format, they will be
        copied.

    *limit*
        Limits how much a pixel is changed. Each new pixel will be in
        the range [old pixel - limit, old pixel + limit].

        Unlike the Avisynth plugin, in this plugin the default limit
        for the second and third planes is the same as the limit for
        the first plane.

        A plane with no limit specified will use the previous plane's
        limit.

        If a plane's limit is 0, the plane will be copied from the
        input frame.

        This parameter is internally scaled according to the bit depth,
        therefore the valid range is [0, 255].

        Default: 4.

    *mode*
        Processing mode.

        Mode 0 is the strongest. Mode 5 is the weakest.

        Default: 1.

    *interlaced*
        If True, the top line and the bottom line of the 3x3
        neighbourhood will come from the same field as the middle
        line. In other words, one line will be skipped between the top
        line and the middle line, and between the middle line and the
        bottom line.

        This parameter should only be used when the input clip
        contains interlaced video.

        Default: False.

    *norow*
        If True, the two pixels to the left and right of the original
        pixel will not be used in the calculations. The corresponding
        pixels from the previous and next frames are still used.

        Default: False.

    *opt*
        If True, the best functions supported by your CPU will be used.
        If False, plain C++ functions will be used.

        Default: True.


Compilation
===========

::

    ./autogen.sh
    ./configure
    make


License
=======

GPL v2, like the Avisynth plugin.
