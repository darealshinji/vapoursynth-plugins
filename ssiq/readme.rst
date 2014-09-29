Description
===========

This is a copy-paste port of the Avisynth plugin SSIQ: http://avisynth.org/warpenterprises/files/ssiq_20070304.zip, which, in turn, is a port of the VirtualDub plugin.

According to the original description, "this filter performs structure-preserving smoothing (blurring) on the I/Q (chrominance or colour) information of the image, leaving Y (luminance) intact. It was created in an attempt to reduce the appearance of "rainbows" (cross-colour artifacts) near lines in some cel animation. It is based on Donald Graft's Smart Smoother filter, which operates in RGB space."

It certainly reduces rainbows, but beware the discoloration around lines...

SSIQ is purely spatial.

Support for RGB input is not yet enabled.


Usage
=====

::

   ssiq.SSIQ(clip clip[, int diameter=11, int strength=200, bint interlaced=0])

diameter
   Odd number between 3 and 11. Higher values kill more rainbows.

strength
   Number between 1 and 300. Higher values kill more rainbows.

interlaced
   Set to True if the input clip is interlaced.


Compilation
===========

::

   ./autogen.sh
   ./configure
   make
