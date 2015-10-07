Description
===========

This is a very basic port of the built-in Avisynth filter Tweak. Internally, it uses std.Lut and std.Expr to carry out the adjustments.


Differences
===========

* No dithering whatsoever.

* Parameters "sse", "startHue", "endHue", "maxSat", "minSat", "interp", and "dither" don't exist.

* 8..16 bit integer and 32 bit float sample types are supported.


Usage
=====

Drop ``adjust.py`` someplace where Python can find it, then use ``import adjust`` in your scripts.

::

    adjust.Tweak(clip clip[, float hue=0.0, float sat=1.0, float bright=0.0, float cont=1.0, bint coring=True])

Parameters:

    * hue

      Adjust the hue. Positive values shift it towards red, while negative values shift it towards green.

      Range: -180.0 .. 180.0

    * sat

      Adjust the saturation. Values above 1.0 increase it, while values below 1.0 decrease it. 0.0 removes all colour.

      Range: 0.0 .. 10.0

    * bright

      Adjust the brightness. This value is directly added to each luma pixel.

    * cont:

      Adjust the contrast. Values above 1.0 increase it, while values below 1.0 decrease it.

      Range: 0.0 .. 10.0


License
=======

WTFPL.
