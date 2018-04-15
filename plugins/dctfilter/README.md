Description
===========

For each 8x8 block, DCTFilter will do a Discrete Cosine Transform (DCT), scale down the selected frequency values, and then reverse the process with an Inverse Discrete Cosine Transform (IDCT).

Requires libfftw3f-3.dll to be in the search path. http://www.fftw.org/install/windows.html


Usage
=====

    dctf.DCTFilter(clip clip, float[] factors[, int[] planes])

* clip: Clip to process. Any planar format with either integer sample type of 8-16 bit depth or float sample type of 32 bit depth is supported.

* factors: A list of 8 floating point numbers, all of which must be specified as in the range (0.0 <= x <= 1.0). These correspond to scaling factors for the 8 rows and columns of the 8x8 DCT blocks. The leftmost number corresponds to the top row, left column. This would be the DC component of the transform and should always be left as 1.0. The row & column parameters are multiplied together to get the scale factor for each of the 64 values in a block.

* planes: A list of the planes to process. By default all planes are processed.


Compilation
===========

Requires `fftw3f`.

```
./autogen.sh
./configure
make
```
