Description
===========

Variant of the spp filter in MPlayer, similar to spp=6 with 7 point DCT where only the center sample is used after IDCT.


Usage
=====

    pp7.DeblockPP7(clip clip[, float qp=2.0, int mode=0, int opt=0, int[] planes])

* clip: Clip to process. Any planar format with either integer sample type of 8-16 bit depth or float sample type of 32 bit depth is supported.

* qp: Constant quantization parameter. It accepts a value in range 1.0 to 63.0.

* mode:
  * 0 = hard thresholding
  * 1 = soft thresholding (better deringing, but blurrier)
  * 2 = medium thresholding (compromise between hard and soft)

* opt: Sets which cpu optimizations to use.
  * 0 = auto detect
  * 1 = use c
  * 2 = use sse2
  * 3 = use sse4.1

* planes: A list of the planes to process. By default all planes are processed.


Compilation
===========

```
./autogen.sh
./configure
make
```
