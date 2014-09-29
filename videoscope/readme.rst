Description
===========

This is a rewrite of the Avisynth plugin VScope: http://forum.doom9.org/showthread.php?t=76238

It is similar to Avisynth's built-in Histogram() filter.


Differences
===========

Some details are slightly different, compared to the original:

- The luma in the histograms is clamped to 16-235 (as opposed to seemingly arbitrary upper limits of 255, 240, 232, depending on the function)

- The chroma in the colormap is more saturated.

- The tick marks are not coloured.

- The full frame histograms in the corner are not coloured. I couldn't see the colours there anyway.


Usage
=====
::

   vscope.Scope(clip clip[, string mode="both", bint tickmarks=1, string side="Y", string bottom="Y", string corner="blank")

mode
   "side"
      Draw a histogram on the right side.

   "bottom"
      Draw a histogram below the image.

   "both"
      Draw both histograms.

tickmarks
   Draw dotted lines over the histograms.

side
   "Y"
      Draw histogram of luma.

   "U"
      Draw histogram of chroma channel U

   "V"
      Draw histogram of chroma channel V.

   "UV"
      Draw histograms of both chroma channels, side by side.

   "YUV"
      Draw histograms of chroma channels, on top of histogram of luma channel.

bottom
   See *side*.

corner
   "blank"
      Keep the corner blank.

   "colormap"
      Chroma map.

   "Y"
      Full frame histogram of luma.

   "U"
      Full frame histogram of chroma channel U.

   "V"
      Full frame histogram of chroma channel V.

   "UV"
      Vectorscope.


Compilation
===========

::

   ./autogen.sh
   ./configure
   make


License
=======

The license is WTFPL.

The original code processes YUY2 directly, so none of it could be copied.
