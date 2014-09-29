Description
===========

TemporalSoften filter for VapourSynth.

This is a port of the internal Avisynth filter TemporalSoften.

Only one mode is implemented because each mode requires decoding a function written in assembly and that's somewhat annoying. I picked mode 2 because QTGMC uses it.


Usage
=====

::

   focus.TemporalSoften(clip clip[, int radius=4, int luma_threshold=4, int chroma_threshold=4, int scenechange=0, int mode=2])

Allowed values (ranges are inclusive):

- radius: 1..7
- luma_threshold: 0..255
- chroma_threshold: 0..255
- scenechange: 0..254
- mode: 2

The two thresholds can't both be 0.


Compilation
===========

::

   ./autogen.sh
   ./configure
   make
