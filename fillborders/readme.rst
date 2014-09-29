Description
===========

This is a simple filter that fills the borders of a clip, without changing the clip's dimensions.


Usage
=====
::

   fb.FillBorders(clip clip[, int left=0, int right=0, int top=0, int bottom=0, string mode="repeat"])

left, right, top, bottom
   Number of pixels to fill on each side. These can be any non-negative numbers, within reason. If they are all 0, the input clip is simply passed through.

mode
   "repeat"
      Fills the borders using the outermost line or column.

   "mirror"
      Fills the borders by mirroring.

   "fillmargins"
      Fills the borders exactly like the Avisynth filter `FillMargins <http://forum.doom9.org/showthread.php?t=50132>`_, version 1.0.2.0. This mode is similar to "repeat", except that each pixel at the top and bottom borders is filled with a weighted average of its three neighbours from the previous line.


Compilation
===========

::

   ./autogen.sh
   ./configure
   make


License
=======

The license is WTFPL.
