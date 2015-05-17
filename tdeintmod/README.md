Description
===========

TDeintMod is a combination of TDeint and TMM, which are both ported from tritical's AviSynth plugin http://bengal.missouri.edu/~kes25c/.

IsCombed is a utility function to check whether or not a frame is combed and stores the result (0 or 1) as a frame property named _Combed. It's intended to be used within `std.FrameEval` to process only combed frames and leave non-combed frames untouched.

Only a few functionality of TDeint is kept in TDeintMod, either because some use inline asm and there is no equivalent C code in the source, or some are very rarely used by people nowadays. For example, the biggest change is that TDeint's internal building of motion mask is entirely dropped, and be replaced with TMM's motion mask. The second is that only cubic interpolation is kept as the only one internal interpolation method, all the others (ELA interpolation, kernel interpolation and blend interpolation) are dropped. Cubic interpolation is kept only for testing purpose, and people should really specify an externally interpolated clip via `edeint` argument for practical use.


Usage
=====

    tdm.TDeintMod(clip clip, int order[, int field=order, int mode=0, int length=10, int mtype=1, int ttype=1, int mtql=-1, int mthl=-1, int mtqc=-1, int mthc=-1, int nt=2, int minthresh=4, int maxthresh=75, int cstr=4, bint show=False, clip edeint])

* order: Sets the field order of the video. The filter will use the field order specified in the source frames and will only fall back to the specified order if not present.
  * 0 = bottom field first (bff)
  * 1 = top field first (tff)

* field: When in mode 0, this sets the field to be interpolated. When in mode 1, this setting does nothing. If omitted, it is set equal to `order`.
  * 0 = interpolate top field (keep bottom field)
  * 1 = interpolate bottom field (keep top field)

* mode: Sets the mode of operation.
  * 0 = same rate output
  * 1 = double rate output (bobbing)

* length: Sets the number of fields required for declaring pixels as stationary. length=6 means six fields (3 top/3 bottom), length=8 means 8 fields (4 top/4 bottom), etc... A larger value for length will prevent more motion-adaptive related artifacts, but will result in fewer pixels being weaved. Must be between 6 and 30 inclusive.

* mtype: Sets whether or not both vertical neighboring lines in the current field of the line in the opposite parity field attempting to be weaved have to agree on both stationarity and direction.
  * 0 = no
  * 1 = no for across, but yes for backwards/forwards
  * 2 = yes
  
  0 will result in the most pixels being weaved, while 2 will have the least artifacts.

* ttype: Sets how to determine the per-pixel adaptive (quarter pel/half pel) motion thresholds.
  * 0 = 4 neighbors, diff to center pixel, compensated
  * 1 = 8 neighbors, diff to center pixel, compensated
  * 2 = 4 neighbors, diff to center pixel, uncompensated
  * 3 = 8 neighbors, diff to center pixel, uncompensated
  * 4 = 4 neighbors, range (max-min) of neighborhood
  * 5 = 8 neighbors, range (max-min) of neighborhood
  
  compensated means adjusted for distance differences due to field vs frames and chroma downsampling. The compensated versions will always result in thresholds <= to the uncompensated versions.

* mtql/mthl/mtqc/mthc: These parameters allow the specification of hard thresholds instead of using per-pixel adaptive motion thresholds. mtqL sets the quarter pel threshold for luma, mthL sets the half pel threshold for luma, mtqC/mthC are the same but for chroma. Setting all four parameters to -2 will disable motion adaptation (i.e. every pixel will be declared moving) allowing for a dumb bob. If these parameters are set to -1 then an adaptive threshold is used. Otherwise, if they are between 0 and 255 (inclusive) then the value of the parameter is used as the threshold for every pixel.

* nt/minthresh/maxthresh: nt sets the noise threshold, which will be added to the value of each per-pixel threshold when determining if a pixel is stationary or not. After the addition of 'nt', any threshold less than minthresh will be increased to minthresh and any threshold greater than maxthresh will be decreased to maxthresh. Must be between 0 and 255 inclusive.

* cstr: Sets the number of required neighbor pixels (3x3 neighborhood) in the quarter pel mask, of a pixel marked as moving in the quarter pel mask, but stationary in the half pel mask, marked as stationary for the pixel to be marked as stationary in the combined mask.

* show: Displays the motion mask instead of the deinterlaced frame.

* edeint: Allows the specification of an external clip from which to take interpolated pixels instead of having TDeintMod use its internal interpolation method. If a clip is specified, then TDeintMod will process everything as usual except that instead of computing interpolated pixels itself it will take the needed pixels from the corresponding spatial positions in the same frame of the edeint clip. To disable the use of an edeint clip simply don't specify a value for edeint.

---

    tdm.IsCombed(clip clip[, int cthresh=6, int blockx=16, int blocky=16, bint chroma=False, int mi=64, int metric=0])

* cthresh: Area combing threshold used for combed frame detection. It is like dthresh or dthreshold in telecide() and fielddeinterlace(). This essentially controls how "strong" or "visible" combing must be to be detected. Good values are from 6 to 12. If you know your source has a lot of combed frames set this towards the low end (6-7). If you know your source has very few combed frames set this higher (10-12). Going much lower than 5 to 6 or much higher than 12 is not recommended.

* blockx: Sets the x-axis size of the window used during combed frame detection. This has to do with the size of the area in which `mi` number of pixels are required to be detected as combed for a frame to be declared combed. See the `mi` parameter description for more info. Possible values are any number that is a power of 2 starting at 4 and going to 2048 (e.g. 4, 8, 16, 32, ... 2048).

* blocky: Sets the y-axis size of the window used during combed frame detection. This has to do with the size of the area in which `mi` number of pixels are required to be detected as combed for a frame to be declared combed. See the `mi` parameter description for more info. Possible values are any number that is a power of 2 starting at 4 and going to 2048 (e.g. 4, 8, 16, 32, ... 2048).

* chroma: Includes chroma combing in the decision about whether a frame is combed. Only use this if you have one of those weird sources where the chroma can be temporally separated from the luma (i.e. the chroma moves but the luma doesn't in a field). Otherwise, it will just help to screw up the decision most of the time.
  * True = include chroma combing
  * False = don't

* mi: The number of required combed pixels inside any of the `blockx` by `blocky` sized blocks on the frame for the frame to be considered combed. While `cthresh` controls how "visible" or "strong" the combing must be, this setting controls how much combing there must be in any localized area (a `blockx` by `blocky` sized window) on the frame. Min setting = 0, max setting = `blockx` x `blocky` (at which point no frames will ever be detected as combed).

* metric: Sets which spatial combing metric is used to detect combed pixels.
```
Assume 5 neighboring pixels (a,b,c,d,e) positioned vertically.
    a
    b
    c
    d
    e

0:  d1 = c - b;
    d2 = c - d;
    if ((d1 > cthresh && d2 > cthresh) || (d1 < -cthresh && d2 < -cthresh))
    {
       if (abs(a+4*c+e-3*(b+d)) > cthresh*6) it's combed;
    }

1:  val = (b - c) * (d - c);
    if (val > cthresh*cthresh) it's combed;
```
Metric 0 is what TDeint always used previous to v1.0 RC7. Metric 1 is the combing metric used in Donald Graft's FieldDeinterlace()/IsCombed() funtions in decomb.dll.


Example usage of IsCombed
=========================

```
import vapoursynth as vs
import functools

core = vs.get_core()
clip = Whatever

def conditionalDeint(n, f, orig, deint):
    if f.props._Combed:
        return deint
    else:
        return orig

deint = core.tdm.TDeintMod(clip, order=1, edeint=core.nnedi3.nnedi3(clip, field=1))
combProps = core.tdm.IsCombed(clip)
clip = core.std.FrameEval(clip, functools.partial(conditionalDeint, orig=clip, deint=deint), combProps)
clip.set_output()
```

Note that it only makes sense to do so in same rate mode, because the output's number of frames from TDeintMod won't match those of the input in double rate mode.
