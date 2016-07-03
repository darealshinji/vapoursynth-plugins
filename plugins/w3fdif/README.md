Description
===========

Weston 3 Field Deinterlacing Filter. Ported from FFmpeg's libavfilter https://ffmpeg.org/ffmpeg-filters.html#w3fdif


Usage
=====

    w3fdif.W3FDIF(clip clip, int order[, int mode=1])

* order: Sets the field order. The filter will use the field order specified in the source frames and will only fall back to the specified order if not present.
  * 0 = bff
  * 1 = tff

* mode: Sets the interlacing filter coefficients.
  * 0 = simple filter coefficient set
  * 1 = more-complex filter coefficient set
