Description
===========

MSmooth is a spatial smoother that doesn't touch edges.

MSharpen is a sharpener that tries to sharpen only edges.


Usage
=====
::

    msmoosh.MSmooth(clip clip[, float threshold=6.0, int strength=3, bint mask=False, planes=<all>])

Parameters:
    clip
        Clip to process. Can have variable format and dimensions. Frames with
        float sample type or with bit depth greater than 16 will be returned
        untouched.

    threshold
        Sensitivity of the edge detection. Decrease if important edges are
        getting blurred. This parameter became a percentage in order to make
        it independent of the bit depth.

    strength
        Number of times the image should be blurred. Increase to make the
        image smoother and the filter slower.

    mask
        If True, the edge mask will be returned instead of the filtered frames.

    planes
        Planes to process.

::

    msmoosh.MSharpen(clip clip[, float threshold=6.0, float strength=39, bint mask=False, planes=<all>])

Parameters:
    clip
        Clip to process. Can have variable format and dimensions. Frames with
        float sample type or with bit depth greater than 16 will be returned
        untouched.

    threshold
        Sensitivity of the edge detection. Decrease if important edges are
        getting blurred. This parameter became a percentage in order to make
        it independent of the bit depth.

    strength
        Strength of the sharpening. This parameter became a percentage in
        order to make it independent of the bit depth.

    mask
        If True, the edge mask will be returned instead of the filtered frames.

    planes
        Planes to process.

        When mask=True, the untouched planes will contain garbage.


Compilation
===========

::

   ./autogen.sh
   ./configure
   make


License
=======

GPL 2, like the Avisynth filters, although one could argue that no code was
copied from them.
