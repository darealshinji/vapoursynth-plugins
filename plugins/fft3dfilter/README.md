FFT3DFilter
===========
FFT3DFilter is a 3D Frequency Domain filter - strong denoiser and moderate sharpener.

FFT3DFilter uses Fast Fourier Transform method for image processing in the frequency domain. It works not locally, but makes some delocalized (block) processing

This is a port of Version 2.1.1 of the Avisynth plugin, which is Copyright (C)2004-2007 Alexander G. Balakhnin, aka Fizick (http://avisynth.org.ru/fft3dfilter/fft3dfilter.html)

This is how it works:
---------------------
* get current frame, and optionally some previous and subsequent frames;
* divide every frame to small overlapped blocks;
* get frequency spectrum by windowed forward 2D FFT transform for every block of these frames;
* apply some filtering to frequency spectrum;
* after denoising, optionally sharpen image (in 2D frequency domain) by amplifying high spectrum (spatial, 2D) frequencies;
* make inverse 2D FFT transform of cleaned spectrum for every block;
* do windowed summation of filtered overlapped blocks to output frame.
* Some analysis and synthesis weighting windows are used to get effective signal gain=1 after blocks summation.

Usage
=====

```python
    core.fft3dfilter.FFT3DFilter(clip clip[, float sigma=2.0, float beta=1.0, int[] planes, int bw=48, int bh=48, int bt=3, int ow= bw/3, int oh=bh/3, float kratio=2.0, float sharpen=0.0, float scutoff=0.3, float svr=1.0, float smin=4.0, float smax=20.0, bint measure=True, bint interlaced=False, int wintype=0, bint pframe=0, int px=0, int py=0, bint pshow=False, float pcutoff=0.1, float pfactor=0.0, float sigma2=sigma, float sigma3=sigma, float sigma4=sigma, float degrid=1.0, float dehalo=0.0, float hr=2.0, float ht=50.0, int ncpu=1])
```

Parameters:
-----------

* ``clip``: input clip to be processed
* ``sigma``: given noise value for all (or highest) frequencies (float>0, default=2.0)
* ``beta``: noise margin (float>=1.0, default=1.0, for no noise left): control noise amount left in the restoration for Wiener filter, so max filter noise attenuation = (beta-1)/beta.
* ``planes``: A list of the planes to process. By default, only luma plane (0) is processed
* ``bw``: block width (integer, default = 48)
* ``bh``: block height (integer, default = 48)
* ``ow``: overlap width (default=bw/3)
* ``oh``: overlap height (default=bh/3)
* ``bt``: mode of operation / block temporal size, number of frames. default = 3
  * 0 - all previous frames (Kalman filter mode);
  * 1 - only current frame (spatial 2D Wiener filter);
  * 2 - previous and current frame (3D Wiener filter);
  * 3 - previous, current and next frame (3D Wiener filter)
  * 4 - two previous, current and next frame (3D Wiener filter)
  * 5 - two previous, current and two next frames (3D Wiener filter)
  * -1 - sharpen only (2D);
* ``kratio``: ratio of threshold to sigma to reset Kalman filter (default = 2.0): variation threshold = sigma*kratio, good value is about from 1.5 to 3.0;
* ``sharpen``: sharpening strength (default=0.0 - not sharpen). good values about 0.3 to 1.0 (negative values result in reverse effect)
* ``scutoff``: sharpening cutoff frequency, relative to max (default=0.3)
* ``svr``: sharpening (and dehalo) vertical ratio (to horizontal) (default=1.0 - same as horizontal, 0 - no vertical sharpening)
* ``smin``: minimum limit (approximate noise margin, to prevent amplifying noise) for sharpening stage (float>0, default=4.0)
* ``smax``: maximum limit (approximate over-sharpening margin, to prevent over-sharpening) for sharpening stage (float>0, default=20.0)
* ``measure``: select the most optimal (fastest) FFT method by speed measure (with longer initial stage). Instead of simple estimation (default=True)
* ``interlaced``: separate fields processing (default=False)
* ``wintype``: weighting windows type (default=0):
  * 0 - same analysis and synthesis half-cosine window;
  * 1 - intermediate between 0 and 2;
  * 2 - flat analysis window, raised cosine (Hanning) synthesis window.
* ``pframe``: noise pattern frame number (default=False)
* ``px``: noise pattern block horizontal X position (default=0)
* ``py``: noise pattern block vertical Y position (default=0)
  * px and py are measured in blocks, not pixels, they are the horizontal and vertical block number respectively, based in block-size and overlap:
    * Horizontal position of block in pixels = (px – 1) * (bw – ow);
    * Vertical position of block in pixels: (py – 1) * (bh – oh)
  * if px=py=0, then the pattern block is defined automatically with minimal power spectral density.
* ``pshow``: show noise pattern block and its properties (default=False)
* ``pcutoff``: noise pattern cutoff frequency (relative to max) (default=0.1)
* ``pfactor``: noise pattern denoise strength (0.0 to 1.0, default=0.0: disable this method)
* ``sigma2``: given noise value at second scale level (mid-high) frequencies (float>0, default=sigma)
* ``sigma3``: given noise value at third scale level (mid-low) frequencies (float>0, default=sigma)
* ``sigma4``: given noise value at lowest frequencies (float>0, default=sigma)
* ``degrid``: weighting window compensation degree for grid decreasing (float>0, default=1.0)
* ``dehalo``: halo removal strength (float>0, default=0.0)
* ``hr``: halo approximate radius (float>0, default=2.0)
* ``ht``: halo approximate threshold (float>0, default=50.0)
* ``ncpu``: max number of CPU threads to use for FFT calculation (int>0, default=1)

Notes on sigma(s) values:
-------------------------
* The most important parameter is a given noise value sigma. You must use reasonable value based on a priori info for current clip.
* Typical value for digital sources is about 1.5 to 2.5, and about 3 and above for analog captured video.
* Filter can produce ghosting for large sigma (and kratio) for 3D modes.
* Large sigma values can be harmful: Destroy/ blur details, generate halos (ringing), produce ghosting and/or banding

Scaling parameters according to bit-depth:
------------------------------------------

The default and recommended values stated in this document apply to videos with integer sample type and 8 bits depth.

Specifically, the values for the parameters ``sigma`` (the main one); ``sigma2``, ``sigma3`` and ``sigma4`` (for ‘composite’ noise pattern); and ``smin`` and ``smax`` (for calibrating the sharpening process) depend on the sample type and bit-depth of the video. They are defined in relation to the maximum pixel value of the video being processed.

That´s why these parameters need to be scaled multiplying the default or recommended values by the factor:  {Maximum_pixel_value}/256. As an example:

* For the FLOAT sample type: 1.0/256
* For INTEGER, 10 bits: 1024/256, equivalent to 1<<(10-8)
* For INTEGER, 16 bits: (2\*\*16)/(2\*\*8), equivalent to 1<<8

Generalizing, it will suffice to use the following python statement, and the use the resulting factor

```python
    ret = AnyClip
    bd_factor = 1.0 / 256.0 if ret.format.sample_type == vs.FLOAT else 1 << (ret.format.bits_per_sample - 8)
    ret = core.fft3dfilter.FFT3DFilter(clip=ret, sigma=1.5*bd_factor)
```

Notes on block size and overlap values:
---------------------------------------
* Good values for overlapping are about quarter to half of block size. Half (ow=bw/2, oh=bh/2) is the best, but slower.
* Good values of block size are about 16 to 64.
* FFTW works most efficiently for arrays with  size (bw, bh) that can be factored into small primes 2, 3, 5, and 7 (and one 11, 13), and otherwise it uses a slower general-purpose routine.
* Larger blocks give some better noise reduction and faster processing, but also give worse ringing (halos) artifacts.

To avoid ringing artifacts:
---------------------------
* Use smaller values for sigma(s) (sigma <= 4.0)
* Use smaller block sizes (bw <= 32 and bh <= 32

To avoid grid artifacts:
------------------------
* Overlapped blocks and weighting windows are used to prevent blocking (and ringing).
* The less overlap, the faster processing, but with more visible grid artifacts.
* Filter can produce some grid artifacts for large sigma and small bw, bh, ow, oh or small relative overlap sizes, especially with sharpening enabled.
* Weighting window wintype=0 can produce the worst grid artifacts, window type wintype=2 does not produce grid artifacts, but can produce some ringing, wintype=1 is intermediate case.
* It’s recommended to use the weighting window compensation with degrid=1, it improves the noise reduction quality and decreases the grid artifacts, especially for 2D.

Modes of operation:
===================
The plugin has several processing modes at filter stage, activated by the bt parameter:

Temporal-only Kalman filter for spectrum data
---------------------------------------------
* ``bt=0``: Temporal Kalman filter for spectrum data. Use all previous frames data to get estimation of cleaned current data with optimal recursive data process algorithm.
* The filter starts working with small (=1) gain (degree of noise reduction), and then gradually (in frames sequence) increases the gain if inter-frame local spectrum (noise) variation is small.
* So, Kalman filter can provide stronger noise reduction than Wiener filter. The Kalman filter gain is limited by some given noise value.
* The local gain (and filter process) is reset to 1 when local variation exceeds the given threshold (due to motion, scene change, etc).
* So, the Kalman filter output is history-dependent (on frame taken as a start filtered frame).

Spatial only (2D) Wiener filter for spectrum data
-------------------------------------------------
* ``bt=1``: 2D (spatial) Wiener filter for spectrum data. Use current frame data only. Reduce weak frequencies (with small power spectral density) by optimal Wiener filter with some given noise value (sigma). Sharpening and noise reduction are simultaneous in this mode.

Spatial-temporal (3D) Wiener filter for spectrum data
-----------------------------------------------------
* ``bt=2``: 3D Wiener filter for spectrum data. Add third dimension to FFT by using previous and current frame data. Reduce weak frequencies (with small power spectral density) by optimal Wiener filter with some given noise value.
* ``bt=3 (default)`` 3D Wiener filter for spectrum data with previous, current and next frame data.
* ``bt=4``: 3D Wiener filter for spectrum data with two previous, current and next frame data.
* ``bt=5``: 3D Wiener filter for spectrum data with two previous, current and two next frames data.

Sharpening only mode:
---------------------
There is also a sharpen-only mode without denoising (``bt=-1``). The following principles apply for this sharpen-only mode and for the sharpening stage after denoising for the Wiener filter modes:

* the weakest frequencies (with small amplitudes) are not amplified to prevent noise increasing (``smin``);
* the strongest frequencies (with large amplitudes) are not amplified to prevent over-sharping and haloing (``smax``).
* The sharpening strength is maximal for frequencies with middle-range amplitudes. Of course, you can control both these margins and general sharpening strength (``sharpen``).
* Gaussian High Pass Filter with variable cutoff frequency (``scutoff``) is used for sharpening.

**dehalo: (halo removal)**
The plugin has a special option, ``dehalo``, for decreasing strong frequencies, it may be used for spatial adaptive softening of over-sharpened pictures (halo removal). This mode may be combined with denoising and sharpening.

Using a detected noise pattern
------------------------------
It is possible to get a noise pattern (shape) estimate by spectrum analysis of some empty block (without any objects), and then use that noise pattern to reduce the noise in the whole clip. It may be useful for removal of film (especially amateur 8 mm) grain and analog TV capture interference.

To use the noise pattern detection method instead of any of the other ordinary (blind) methods you must:

* First, switch on the show mode with ``pshow=True``, and set some non-zero value ``pfactor=1.0``.
* Preview the clip to select a frame number with the ``pframe`` parameter and some block position with the ``px`` and ``py`` parameters. The selected block shown must not contain any objects besides a typical noise pattern
* Then switch off the show mode with ``pshow=False``,
* and set noise reduction strength, ``pfactor`` to a recommended value = 0.5 to 1.0.

The best window type for this method is wintype=2.

The sigma and beta parameters are not used in this denoising method, but you can use this method in show mode pshow=True to estimate sigma value for ordinary denoising method (it is not strictly the same, but similar value).

Composite noise pattern
-----------------------
There is also composite method, that allows you to **directly set different noise values: sigma for highest frequencies; and sigma2(mid-high), sigma3(mid-low), sigma4(lowest) for lower frequencies**. The pattern coefficients will be created internally from these sigma values by interpolation. Set pfactor=0 for this method (internally it will be =1).

Features and limitations
========================
* The filter uses fast external FFTW library version 3 (http://www.fftw.org). The filter will NOT work without it!
* Algorithm is optimized by speed for forward sequential frames access only.
* The sharpening method is experimental
* The noise pattern method is experimental too.
* The degrid option is not implemented for Kalman filter.
* For noisy video it is useful to use some mild median prefilter. For example, RemoveGrain (for 2D pre-filtering, using modes 5 or 6) or DeGrainMedian (for 3D pre-filteing, using modes 5 or 4), before FFT3Dfilter.
* Multithread mode ncpu>1 is almost not tested
* To prevent "banding" you can use higher bit-depths for processing and after filtering you can decrease the bit-depth with dithering

License
=======
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 - GPL v2, like the Avisynth plugin.

Documentation is distributed under CreativeCommons BY-SA 3.0 license. Large parts of this document were copied from "fft3dfilter.html", written by Alexander G. Balakhnin, aka Fizick

------

Simple sample scripts
=====================

2D filtering all planes:
------------------------

```python
    ret = Anything
    # optional mild median filter to help with noisy video:
    # ret  =  core.rgvs.RemoveGrain(clip=ret, mode=5)
    ret = core.fft3dfilter.FFT3DFilter(clip=ret, sigma=2.0, planes=[0,1,2], bt=1, bw=16, bh=16, ow=8, oh=8)
    ret.set_output()
```

3D filtering all planes, with motion compensation via mvtools:
--------------------------------------------------------------

```python
    ret = Anything
    super_ret  = core.mv.Super(clip=ret, hpad=8, vpad=8)
    bvectors =  core.mv.Analyse(super=super_ret, isb=True,  delta=1,   blksize=8, overlap=4)
    fvectors =  core.mv.Analyse(super=super_ret, isb=False, delta=1,   blksize=8, overlap=4)
    fwd_comp = core.mv.Compensate(clip=ret, super=super_ret, vectors=fvectors)
    bwd_comp = core.mv.Compensate(clip=ret, super=super_ret, vectors=bvectors)
    ret = core.std.Interleave(clips=[fwd_comp, ret, bwd_comp])
    # optional mild median filter to help with noisy video:
    # ret = core.dgm.DegrainMedian(clip=ret, mode=5)
    ret = core.fft3dfilter.FFT3DFilter(clip=ret, sigma=2.0, planes=[0,1,2], bt=3, bw=16, bh=16, ow=8, oh=8)
    ret = core.std.SelectEvery(clip=ret, cycle=3, offsets=1)
    ret.set_output()
```

FFT3DFilter as a prefilter for motion compensated denoising:
------------------------------------------------------------

```python
    ret = Anything
    filtered  = core.rgvs.RemoveGrain(clip=ret, mode=5)
    filtered  = core.fft3dfilter.FFT3DFilter(clip=filtered, sigma=4.0, sigma2=0.5, sigma3=0.5, sigma4=0.75, planes=[0,1,2], bt=1, bw=16, bh=16, ow=8, oh=8)
    super_ret   = core.mv.Super(clip=ret)
    super_filt  = core.mv.Super(clip=filtered)
    bvectors =  core.mv.Analyse(super=super_filtered, isb=True,  delta=1, blksize=8, overlap=4)
    fvectors =  core.mv.Analyse(super=super_filtered, isb=False, delta=1, blksize=8, overlap=4)
    ret = core.mv.Degrain1(clip=ret, super=super_ret, mvbw=bvectors, mvfw=fvectors, plane=4)
    ret.set_output()
```

Using a detected noise pattern for 2D filtering:
------------------------------------------------

```python
    ret = Anything
    ret = core.fft3dfilter.FFT3DFilter(clip=ret, bt=1, bw=32, bh=32, ow=16, oh=16, pshow=True, pfactor=0.5)
    ret.set_output()
```

And after locating the appropriate frame and block:

```python
    ret = Anything
    ret = core.fft3dfilter.FFT3DFilter(clip=ret, bt=1, bw=32, bh=32, ow=16, oh=16, pshow=False, pfactor=0.5, pframe=24, px=32, py=8)
    ret.set_output()
```

**adjusting bw, ow, bh and oh for videos with chroma subsampling:**

Chroma subsampling makes the px and py point to different locations and make different block sizes for the pattern block in the chroma planes. The only way they will coincide is when there’s no chroma subsampling (4:4:4)

Chroma planes must be processed separately from the luma plane in another instance of the same filter, adjusting the block size and overlap, depending on the subsampling, so that the position and size of the block pattern coincide.

To use this **workaround**, choose power-of-two values for the bw, ow, bh and oh parameters. That way they can be divided into 2 or 4.

```python
    ret = AnyClip

    bw = bh = 32
    ow = oh = 16

    ret =  core.fft3dfilter.FFT3DFilter(clip=ret, planes=[0], bt=1, bw=bw, bh=bh, ow=ow, oh=oh, pframe=80, pshow=False, pfactor=1.0, px=px, py=py)

    wf = 1 << ret.format.subsampling_w
    hf = 1 << ret.format.subsampling_h

    ret =  core.fft3dfilter.FFT3DFilter(clip=ret, planes=[1,2], bt=1, bw=bw/wf, bh=bh/wf, ow=ow/wf, oh=oh/wf, pframe=80, pshow=False, pfactor=1.0, px=px, py=py)

    ret.set_output()
```

Using a 'composite' noise pattern for 2D filtering:
---------------------------------------------------

```python
    ret = Anything
    ret = core.fft3dfilter.FFT3DFilter(clip=ret, sigma=4.0, sigma2=0.5, sigma3=0.5, sigma4=0.75, planes=[0,1,2], bt=1, bw=16, bh=16, ow=8, oh=8)
    ret.set_output()
```

This pattern is 'safe' for reducing compression artifacts like  blocking and ringing, without blurring details or creating halos around high contrast borders

Some variations of the same noise pattern are also useful for noisier videos:

    +------+------+----+------+----+-------+---------------------------------------------------+
    |      |safest|safe|medium|high|extreme|Higher values will harm the video:                 |
    +======+======+====+======+====+=======+===================================================+
    |sigma |     2|   4|     6|   8|      8|Blurs details and produce “halos” around borders   |
    +------+------+----+------+----+-------+---------------------------------------------------+
    |sigma2|   0.2| 0.5|  0.75|   1|      2|Blurs details and produce “halos” around borders   |
    +------+------+----+------+----+-------+---------------------------------------------------+
    |sigma3|   0.2| 0.5|  0.75|   1|      1|Blurs details and produce “ghosting” and "banding" |
    +------+------+----+------+----+-------+---------------------------------------------------+
    |sigma4|   0.4|0.75|     1|   1|      1|Blurs details and produce “ghosting” and "banding" |
    +------+------+----+------+----+-------+---------------------------------------------------+