Description
===========

TComb is a temporal comb filter (it reduces cross-luminance (rainbowing)
and cross-chrominance (dot crawl) artifacts in static areas of the picture).
It will ONLY work with NTSC material, and WILL NOT work with telecined material
where the rainbowing/dotcrawl was introduced prior to the telecine process!
It must be used before ivtc or deinterlace.


Usage
=====
::

   tcomb.TComb(clip clip[, int mode=2, int fthreshl=4, fthreshc=5, othreshl=5, othreshc=6, bint map=False, float scthresh=12.0])

Parameters:
   clip
      Clip to process. Must be 8 bit Gray or YUV with constant format and dimensions.

   mode
      * 0 - process luma only (remove dotcrawl)
      * 1 - process chroma only (remove rainbows)
      * 2 - process both

   fthreshl

   fthreshc
      Filtered pixel correlation thresholds.

      One of the things TComb checks for is correlation between filtered values over the length
      of the filtering window. If all values differ by less than fthreshl (for luma) or fthreshc
      (for chroma) then the filtered values are considered to be correlated. Larger values will
      allow more filtering (will be more effective at removing rainbowing/dot crawl), but will also
      create more artifacts. A good range of values is between 4 and 7.

   othreshl

   othreshc
      Original pixel correlation thresholds.

      One of the things TComb checks for is correlation between original pixel values from every
      other field of the same parity. Due to the oscillation period, these values should be equal
      or very similar in static areas containing dot crawl or rainbowing. If the pixel values
      differ by less than othreshl (for luma) or othreshc (for chroma) then the pixels are considered
      to be correlated. Larger values will allow more filtering (will be more effective at removing
      rainbowing/dotcrawl), but will also create more artifacts. A good range of values
      is between 4 and 8.

   map
      Instead of filtering the image, shows which pixels would get filtered
      and how.

      Each pixel in the output frame will have one of the following values
      indicating how it is being filtered:

      * 0 - not being filtered
      * 85 - [1 2 1] average of (n,n+1,n+2)
      * 170 - [1 2 1] average of (n-2,n-1,n)
      * 255 - [1 2 1] average of (n-1,n,n+1)

      n = current frame

   scthresh
      Scene change threshold.

      Sets the scenechange detection threshold as a percentage of maximum
      change on the luma plane.


Compilation
===========

::

   ./autogen.sh
   ./configure
   make


License
=======

GPL 2, like the Avisynth filter.
