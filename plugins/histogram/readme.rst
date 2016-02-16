Description
===========

Port of the Histogram() function from Avisynth.

All modes dealing with video are implemented
(classic, levels, color, color2, luma).


Usage
=====
::

    hist.Classic(clip clip)

    hist.Levels(clip clip[, float factor=100.0])

    hist.Color(clip clip)

    hist.Color2(clip clip)

    hist.Luma(clip clip)


Compilation
===========

::

    ./autogen.sh
    ./configure
    make
