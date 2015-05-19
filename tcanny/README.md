Description
===========

Builds an edge map using canny edge detection.

Ported from AviSynth plugin http://bengal.missouri.edu/~kes25c/


Usage
=====

    tcanny.TCanny(clip clip[, float sigma=1.5, float t_h=8.0, float t_l=1.0, int nms=3, int mode=0, float gmmax=50.0, int[] planes])

* sigma: Standard deviation of gaussian blur.

* t_h: High gradient magnitude threshold for hysteresis.

* t_l: Low gradient magnitude threshold for hysteresis.

* nms: Byte mask choosing non-maxima suppression methods.
  * (nms&1) = if pixel is maxima along nearest integer direction then keep
  * (nms&3) = if pixel is maxima along linearly interpolated direction then keep

* mode: Sets output format.
  * 0 = thresholded edge map (2^bitdepth-1 for edge, 0 for non-edge)
  * 1 = gradient magnitude map
  * 2 = edge pixel only gradient direction map (non-edge pixels set to 0)
  * 3 = gradient direction map

* gmmax: Used for scaling gradient magnitude into [0, 2^bitdepth-1] for mode=1.

* planes: A list of the planes to process. By default all planes are processed.
