Description
===========

This is a wavelet based denoiser.

Basically, it transforms each frame from the video input into the wavelet domain, using Cohen-Daubechies-Feauveau 9/7. Then it applies some filtering to the obtained coefficients. It does an inverse wavelet transform after. Due to wavelet properties, it should give a nice smoothed result, and reduced noise, without blurring picture features.

Ported from AviSynth plugin http://avisynth.org.ru/vague/vaguedenoiser.html


Usage
=====

    vd.VagueDenoiser(clip clip[, float threshold=2.0, int method=2, int nsteps=6, float percent=85.0, int[] planes])

* clip: Clip to process. Any planar format with either integer sample type of 8-16 bit depth or float sample type of 32 bit depth is supported.

* threshold: Filtering strength. The higher, the more filtered the clip will be. Hard thresholding can use a higher threshold than Soft thresholding before the clip looks overfiltered.

* method: The filtering method the filter will use.
  * 0: Hard thresholding. All values under the threshold will be zeroed.
  * 1: Soft thresholding. All values under the threshold will be zeroed. All values above will be reduced by the threshold.
  * 2: Qian's (garrote) thresholding. Scales or nullifies coefficients - intermediary between (more) soft and (less) hard thresholding.

* nsteps: Number of times, the wavelet will decompose the picture. Picture can't be decomposed beyond a particular point (typically, 8 for a 640x480 frame - as 2^9 = 512 > 480)

* percent: Partial of full denoising (limited coefficients shrinking), from 0 to 100.

* planes: A list of the planes to process. By default all planes are processed.
