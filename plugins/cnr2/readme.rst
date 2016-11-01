Description
===========

Cnr2 is a temporal denoiser designed to denoise only the chroma. This
is a port of the Avisynth filter Cnr2 version 2.6.1, originally written
by Marc FD and later adopted by tritical.

According to the original author, this filter is suited for stationary
rainbows or noisy analog captures.

Due to the way it works, this filter is forced to run in a single
thread.


Usage
=====
::

    cnr2.Cnr2(clip clip[, string mode="oxx", float scdthr=10.0, int ln=35, int lm=192, int un=47, int um=255, int vn=47, int vm=255, bint scenechroma=False])


Parameters:
    *clip*
        Clip to process. Must be YUV420P8, YUV422P8, YUV440P8, or
        YUV444P8, with constant format and dimensions.

    *mode*
        Mode for each plane. The letter ``o`` means wide mode, which
        is less sensitive to changes in the pixels, and more effective.
        The letter ``x`` means narrow mode, which is less effective.

        Default: "oxx".

    *scdthr*
        Scene change detection threshold as percentage of maximum
        possible change. Lower values make it more sensitive.

        Default: 10.0.

    *ln*, *un*, *vn*
        Sensitivity to movement in the Y, U, and V planes,
        respectively. Higher values will denoise more, at the risk of
        introducing ghosting in the chroma.

        Default: 35, 47, 47.

    *lm*, *um*, *vm*
        Strength of the denoising. Higher values will denoise harder.

        Default: 192, 255, 255.

    *scenechroma*
        If True, the chroma is considered in the scene change detection.


Compilation
===========

::

    ./autogen.sh
    ./configure
    make


License
=======

GPL v2, like the Avisynth plugin.
