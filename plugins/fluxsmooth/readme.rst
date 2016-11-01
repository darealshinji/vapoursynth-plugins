Description
===========

FluxSmooth is a filter for smoothing of fluctuations.


Usage
=====
::

   flux.SmoothT(clip clip[, int temporal_threshold=7, int[] planes=[0, 1, 2]])

SmoothT (**T**\ emporal) examines each pixel and compares it to the corresponding pixel in the previous and next frames. Smoothing occurs if both the previous frame's value and the next frame's value are greater, or if both are less, than the value in the current frame. Smoothing is done by averaging the pixel from the current frame with the pixels from the previous and/or next frames, if they are within *temporal_threshold*.

Parameters:
   clip
      Clip to process. Only clips with constant format and dimensions and 8..16 bit integer pixels are supported.

   temporal_threshold
      Temporal neighbour pixels within this threshold from the current pixel are included in the average.

   planes
      Planes to process.

::

   flux.SmoothST(clip clip[, int temporal_threshold=7, int spatial_threshold=7, int[] planes=[0, 1, 2]])


SmoothST (**S**\ patio\ **T**\ emporal) does the same as SmoothT, except the pixel's eight neighbours from the current frame are also included in the average, if they are within *spatial_threshold*.

The first and last rows and the first and last columns are not processed by SmoothST.

Parameters:
   clip
      Clip to process. Only clips with constant format and dimensions and 8..16 bit integer pixels are supported.

   temporal_threshold
      Temporal neighbour pixels within this threshold from the current pixel are included in the average. If set to -1, no temporal smoothing occurs.

   spatial_threshold
      Spatial neighbour pixels within this threshold from the current pixel are included in the average. If set to -1, no spatial smoothing occurs.

   planes
      Planes to process.


Compilation
===========

::

   ./autogen.sh
   ./configure
   make


License
=======

The original FluxSmooth author said:
"There is no copyright on this code, and there are no conditions on its distribution or use. Do with it what you will."
Thank you.
