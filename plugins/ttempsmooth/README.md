Description
===========

TTempSmooth is a motion adaptive (it only works on stationary parts of the picture), temporal smoothing filter.

Ported from AviSynth plugin http://bengal.missouri.edu/~kes25c/


Usage
=====

    ttmpsm.TTempSmooth(clip clip[, int maxr=3, int[] thresh=[4, 5, 5], int[] mdiff=[2, 3, 3], int strength=2, float scthresh=12.0, bint fp=True, clip pfclip=None, int[] planes])

* clip: Clip to process. Any planar format with either integer sample type of 8-16 bit depth or float sample type of 32 bit depth is supported.

* maxr: This sets the maximum temporal radius. By the way it works TTempSmooth automatically varies the radius used... this sets the maximum boundary. Possible values are 1 through 7. At 1 TTempSmooth will be (at max) including pixels from 1 frame away in the average (3 frames total will be considered counting the current frame). At 7 it would be including pixels from up to 7 frames away (15 frames total will be considered). With the way it checks motion there isn't much danger in setting this high, it's basically a quality vs. speed option. Lower settings are faster while larger values tend to create a more stable image.

* thresh: Your standard thresholds for differences of pixels between frames. TTempSmooth checks 2 frame distance as well as single frame, so these can usually be set slightly higher than with most other temporal smoothers and still avoid artifacts. Valid settings are from 1 to 256. Also important is the fact that as long as `mdiff` is less than the threshold value then pixels with larger differences from the original will have less weight in the average. Thus, even with rather large thresholds pixels just under the threshold wont have much weight, helping to reduce artifacts. If a single value is specified, it will be used for all planes. If two values are given then the second value will be used for the third plane as well.

* mdiff: Any pixels with differences less than or equal to `mdiff` will be blurred at maximum. Usually, the larger the difference to the center pixel the smaller the weight in the average. `mdiff` makes TTempSmooth treat pixels that have a difference of less than or equal to `mdiff` as though they have a difference of 0. In other words, it shifts the zero difference point outwards. Set `mdiff` to a value equal to or greater than `thresh-1` to completely disable inverse pixel difference weighting. Valid settings are from 0 to 255. If a single value is specified, it will be used for all planes. If two values are given then the second value will be used for the third plane as well.

* strength: TTempSmooth uses inverse distance weighting when deciding how much weight to give to each pixel value. The strength option lets you shift the drop off point away from the center to give a stronger smoothing effect and add weight to the outer pixels. It does for the spatial weights what `mdiff` does for the difference weights.
  * 1 =    0.13 0.14 0.16 0.20 0.25 0.33 0.50 1.00 0.50 0.33 0.25 0.20 0.16 0.14 0.13
  * 2 =    0.14 0.16 0.20 0.25 0.33 0.50 1.00 1.00 1.00 0.50 0.33 0.25 0.20 0.16 0.14
  * 3 =    0.16 0.20 0.25 0.33 0.50 1.00 1.00 1.00 1.00 1.00 0.50 0.33 0.25 0.20 0.16
  * 4 =    0.20 0.25 0.33 0.50 1.00 1.00 1.00 1.00 1.00 1.00 1.00 0.50 0.33 0.25 0.20
  * 5 =    0.25 0.33 0.50 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 0.50 0.33 0.25
  * 6 =    0.33 0.50 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 0.50 0.33
  * 7 =    0.50 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 0.50
  * 8 =    1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00

  The values shown are for `maxr=7`, when using smaller radius values the weights outside of the range are simply dropped. Thus, setting `strength` to a value of `maxr+1` or higher will give you equal spatial weighting of all pixels in the kernel.

* scthresh: The standard scenechange threshold as a percentage of maximum possible change of the luma plane. A good range of values is between 8 and 15. Set `scthresh` to 0 to disable scenechange detection.

* fp: Setting `fp=True` will add any weight not given to the outer pixels back onto the center pixel when computing the final value. Setting `fp=False` will just do a normal weighted average. `fp=True` is much better for reducing artifacts in motion areas and usually produces overall better results.

* pfclip: This allows you to specify a separate clip for TTempSmooth to use when calculating pixel differences. This applies to checking the motion thresholds, calculating inverse difference weights, and detecting scenechanges. Basically, the `pfclip` will be used to determine the weights in the average but the weights will be applied to the original input clip's pixel values.

* planes: A list of the planes to process. By default all planes are processed.


Compilation
===========

```
./autogen.sh
./configure
make
```
