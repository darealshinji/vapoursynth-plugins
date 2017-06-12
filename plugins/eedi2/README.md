Description
===========

EEDI2 resizes an image by 2x in the vertical direction by copying the existing image to 2*y(n) and interpolating the missing field. It is intended for edge-directed interpolation for deinterlacing (i.e. not really made for resizing a normal image, but can do that as well).

Ported from AviSynth plugin http://bengal.missouri.edu/~kes25c/


Usage
=====

    eedi2.EEDI2(clip clip, int field[, int mthresh=10, int lthresh=20, int vthresh=20, int estr=2, int dstr=4, int maxd=24, int map=0, int nt=50, int pp=1])

* clip: Clip to process. Any planar format with integer sample type of 8-16 bit depth is supported.

* field: Controls which field in the resized image the original image will be copied to.
  * 0 = bottom field
  * 1 = top field
  * 2 = alternates each frame, starts with bottom
  * 3 = alternates each frame, starts with top

* mthresh, lthresh, vthresh: These all control edge detection used for building the initial edge map. mthresh is the edge magnitude threshold... its range is from 0 to 255, lower values will detect weaker edges. lthresh is the laplacian threshold... its range is 0 to 510, lower values will detect weaker lines. vthresh is the variance threshold... its range is 0 to a large number, lower values will detect weaker edges. Use the "map" option to tweak these settings as needed.

* estr, dstr: These are used for dilation and erosion of the edge map. estr sets the required number of edge pixels (<=) in a 3x3 area, in which the center pixel has been detected as an edge pixel, for the center pixel to be removed from the edge map. dstr sets the required number of edge pixels (>=) in a 3x3 area, in which the center pixel has not been detected as an edge pixel, for the center pixel to be added to the edge map. Use the "map" option to tweak these settings as needed.

* maxd: Sets the maximum pixel search distance for determining the interpolation direction. Larger values will be able to connect edges and lines of smaller slope but can lead to artifacts. Sometimes using a smaller maxd will give better results than a larger setting. The maximum possible value for maxd is 29.

* map: Allows one of three possible maps to be shown.
  * 0 = no map
  * 1 = edge map (Edge pixels will be set to 255 and non-edge pixels will be set to 0)
  * 2 = original scale direction map
  * 3 = 2x scale direction map

* nt: Defines a noise threshold between pixels in the sliding vectors, this is used to set initial starting values. Lower values should reduce artifacts but sacrifice edge reconstruction... while higher values should improve edge recontruction but lead to more artifacts. The possible range of values is 0 to 256.

* pp: Enables two optional post-processing modes aimed at reducing artifacts by identifying problem areas and then using plain vertical linear interpolation in those parts. Using the pp modes will slow down processing and can cause some loss of edge directedness.
  * 0 = no post-processing
  * 1 = check for spatial consistency of final interpolation directions
  * 2 = check for junctions and corners
  * 3 = do both 1 and 2
