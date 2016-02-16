Description
===========

AWarpSharp2 sharpens edges by warping them. This is a rewrite of the
Avisynth filter (re)written by SEt.


Usage
=====
::

    warp.AWarpSharp2(clip clip[, int thresh=128, int blur=2, int type=0, int depth=16, int chroma=0, int[] planes=<all>, bint opt=True])

    warp.ASobel(clip clip[, int thresh=128, int[] planes=<all>, bint opt=True])

    warp.ABlur(clip clip[, int blur=3, int type=1, int[] planes=<all>, bint opt=True])

    warp.AWarp(clip clip, clip mask[, int depth=3, int chroma=0, int[] planes=<all>, bint opt=True])

AWarpSharp2 performs edge detection, blurring, and warping, all in one.

ASobel performs edge detection, with an algorithm that might resemble the
one used by std.Sobel.

ABlur blurs.

AWarp warps.

Parameters:
    *clip*
        Clip to process. Must be 8 bit, not RGB, with constant format and
        dimensions.

        For the AWarp filter, *clip* must be either the same size as
        *mask*, or four times the size of *mask* in each dimension. The
        latter can be useful if better subpixel interpolation is desired,
        e.g. using nnedi3_rpow2.

        If *clip* is upscaled to four times the original size, it must be
        top-left aligned. See the examples below.

    *mask*
        Edge mask. Must have the same format as *clip*.

    *thresh*
        No pixel in the edge mask will have a value greater than *thresh*.
        Decrease for weaker sharpening.

        Range: 0..255.

        Default: 128.

    *blur*
        Controls the number of times to blur the edge mask. Increase for
        weaker sharpening.

        Range: 0..+infinity.

        Default: 2 for *type* 0, 3 for *type* 1.

    *type*
        Controls the type of blur to use. 0 means some kind of 13x13
        average. 1 means some kind of 5x5 average. 1 is better, but
        requires more passes for similar sharpening strength as 0.

        Default: 0.

    *depth*
        Controls how far to warp. Negative values warp in the other
        direction, i.e. will blur the image instead of sharpening.

        Range: -128..127

        Default: 16 for AWarpSharp2, 3 for AWarp.

    *chroma*
        Controls the chroma handling method. 0 will use the edge mask from
        the luma to warp the chroma. 1 will create an edge mask from each
        chroma channel and use those to warp each chroma channel
        individually.

    *planes*
        Controls which planes are processed. Planes that are not processed
        are simply copied from the source frame, except in the AWarp filter,
        when *clip* is four times the size of *mask*, in which case the
        unprocessed planes will contain uninitialised memory.

    *opt*
        If True, the best functions supported by your CPU will be used.
        If False, plain C++ functions will be used.


Examples
========

::

    import vapoursynth as vs

    c = vs.get_core()

    src = c.d2v.Source("d2v.d2v")

    mask = c.warp.ASobel(src)
    mask = c.warp.ABlur(mask)

    # Use sx=0.375 sy=0.375 for top-left alignment.
    src4 = c.fmtc.resample(src, w=src.width * 4, h=src.height * 4, sx=0.375, sy=0.375)
    src4 = c.fmtc.bitdepth(src4, bits=8)

    warp = c.warp.AWarp(src4, mask)


Compilation
===========

::

    ./autogen.sh
    ./configure
    make


License
=======

ISC.
