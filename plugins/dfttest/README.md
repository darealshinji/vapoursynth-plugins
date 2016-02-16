Description
===========

2D/3D frequency domain denoiser.

Requires libfftw3f-3.dll to be in the search path. http://www.fftw.org/install/windows.html

Ported from AviSynth plugin http://bengal.missouri.edu/~kes25c/


Usage
=====

    dfttest.DFTTest(clip clip[, int ftype=0, float sigma=5.0, float sigma2=5.0, float pmin=0.0, float pmax=500.0, int sbsize=12, int smode=1, int sosize=9, int tbsize=3, int tmode=0, int tosize=0, int swin=0, int twin=7, float sbeta=2.5, float tbeta=2.5, bint zmean=True, float f0beta=1.0, string nstring='', string sstring='', string ssx='', string ssy='', string sst='', int[] planes])

```
ftype -

   Controls the filter type.  Possible settings are:

       0 -  generalized wiener filter

             mult = max((psd - sigma) / psd, 0) ^ f0beta

       1 -  hard threshold

             mult = psd < sigma ? 0.0 : 1.0

       2 -  multiplier

             mult = sigma

       3 -  multiplier switched based on psd value

             mult = (psd >= pmin && psd <= pmax) ? sigma : sigma2

       4 -  multiplier modified based on psd value and range

             mult = sigma * sqrt((psd * pmax) / ((psd + pmin) * (psd + pmax)))

   The real and imaginary parts of each complex dft coefficient are multiplied
   by the corresponding 'mult' value.

      ** psd = magnitude squared = real*real + imag*imag


sigma,sigma2 -

   Value of sigma and sigma2 (used as described in ftype parameter description).
   If using the sstring parameter then the sigma parameter is ignored.


pmin,pmax -

   Used as described in the ftype parameter description.


sbsize -

   Sets the length of the sides of the spatial window.  Must be 1 or greater.
   Must be odd if using smode=0.


smode -

   Sets the mode for spatial operation.  There are two possible settings:

      0 -  process every pixel independently... center the spatial window
           on the current pixel, filter, move to the next pixel, repeat.
           Spatial overlapping 'sosize' not used.

      1 -  process the spatial dimension in blocks of sbsize.  Spatial
           overlapping 'sosize' used.


sosize -

   Sets the spatial overlap amount.  Must be in the range 0 to sbsize-1 (inclusive).
   If sosize is greater than sbsize>>1, then sbsize%(sbsize-sosize) must equal 0.
   In other words, overlap greater than 50% requires that sbsize-sosize be a divisor
   of sbsize.


tbsize -

   Sets the length of the temporal dimension (i.e. number of frames).  Must be at
   least 1.  Must be odd if using tmode=0.


tmode -

   Sets the mode for temporal operation.  There are two possible settings:

      0 -  process every frame independently... center the temporal window
           on the current frame, filter, move to the next frame, repeat.
           Temporal overlapping 'tosize' not used.

      1 -  process the temporal dimension in blocks of tbsize.  Temporal
           overlapping 'tosize' used.  

   Currently only tmode=0 is implemented in VapourSynth.


tosize -

   Sets the temporal overlap amount.  Must be in the range 0 to tbsize-1 (inclusive).
   If tosize is greater than tbsize>>1, then tbsize%(tbsize-tosize) must equal 0.
   In other words, overlap greater than 50% requires that tbsize-tosize be a divisor
   of tbsize.


swin,twin -

   Sets the type of analysis/synthesis window to be used for spatial (swin) and
   temporal (twin) processing.  Possible settings:

     0:  hanning
     1:  hamming
     2:  blackman
     3:  4 term blackman-harris
     4:  kaiser-bessel
     5:  7 term blackman-harris
     6:  flat top
     7:  rectangular
     8:  Bartlett
     9:  Bartlett-Hann
    10:  Nuttall
    11:  Blackman-Nuttall


sbeta,tbeta -

   Sets the beta value for kaiser-bessel window type.  sbeta goes with swin,
   tbeta goes with twin.  Not used unless the corresponding window value
   is set to 4.


zmean -

   Controls whether the window mean is subtracted out (zero'd) prior to
   filtering in the frequency domain.


f0beta -

   Power term in ftype=0. The ftype=0 formula is:

          max((psd - sigma) / psd, 0) ^ f0beta

   For f0beta=1, this equation corresponds to the wiener filter with
   spectral subtraction as the estimate of the signal power. For f0beta=0.5,
   the equation corresponds to spectral subtraction. The 1.0 and 0.5 cases
   are separated from the general routine in the code to allow for fast
   operation. Other values will result in the general routine being used,
   which has to perform a pow() computation, and is therefore much slower.


nstring -

   When ftype<2, nstring can be used to specify block locations in the video
   from which dfttest will estimate the noise power spectrum (sigma) to
   be used for filtering.

   When the noise to be removed is not white (i.e. doesn't have a flat power
   spectrum), specifying only a single sigma value is not adequate.

   The nstring should list locations in the video that consist of noise on
   a flat background, separated by a space. The line syntax is:

             frame_number,plane,ypos,xpos   e.g.  0,0,20,20

       plane:  (0=Y,1=U,2=V)
       ypos/xpos:  the upper left position of the block
                   (0,0 is the upper left of the frame)

       dfttest positions a window (of the type defined by sbsize/tbsize/swin/twin)
       at the specified location, and estimates the power using fft magnitude^2.
       When tbsize>1, frame_number specifies the first frame of the temporal block.
       Make sure that the window size is large enough to capture the full noise
       pattern.

       If you list multiple blocks, then the estimates obtained at each block are
       averaged to form the final estimate. Having more block locations to use
       lowers the variance of the estimate. The more block locations you specify the
       closer the true noise spectrum will be estimated, resulting in better
       denoising. When listing multiple block locations, it is best/preferred if the
       locations do not overlap.

       Typically, subtracting out the noise power spectrum is not adequate becase
       it is only the average. In any one block the noise spectrum has the potential
       to exceed the average in a frequency bin. Therefore, one typically over
       subtracts based on some multiple of the noise spectrum (usually in the range
       of 3-8). The default used in dfttest is 5 if ftype=0 and 7 if ftype=1. If you
       want to use another value, then put the following as the first entry in the
       string:

              a=over_subtraction_factor   e.g.  a=3.5

       Example:

          nstring="a:4.0 35,0,45,68 28,0,23,87"


sstring/ssx/ssy/sst -

   Used to specify functions of sigma based on frequency. If you want sigma to vary
   based on frequency, then use 'sstring' instead of the 'sigma' parameter. sstring
   allows you to enter values of sigma for different normalized [0.0,1.0] frequency
   locations. Values for locations between the ones you explicitly specify are computed
   via linear interpolation. The frequency range, which is dependent on sbsize/tbsize,
   is normalized to [0.0,1.0] with 0.0 being the lowest frequency and 1.0 being the
   highest frequency. You MUST specify sigma values for those end point locations
   (0.0 and 1.0)! You can specify as many other locations as you wish, and they don't
   have to be in any particular order. Each frequency/sigma pair is given as "f.f:s.s".
   The list of frequency/sigma pairs is saved as a string, with each pair separated by
   a space.

   For example, if you want a linear ramp of sigma from 1.0 for the lowest frequency
   to 10.0 for the highest frequency use:

        sstring = "0.0:1.0 1.0:10.0"

         "0.0:1.0"  =>  this means sigma=1.0 at frequency 0.0

         "1.0:10.0"  => this means sigma=10.0 at frequency 1.0

         Sigma values for frequencies between 0.0 and 1.0 will be computed via
         linear interpolation.

   Or if you want a band-stop filter that passes low and high frequencies (filters
   middle frequencies) use something like:

        sstring = "0.0:0.0 0.15:10.0 0.85:10.0 1.0:0.0"

   There are two methods for computing sigma values for a given frequency bin based on
   sstring. The first computes the normalized frequency location of each dimension 
   (horizontal,vertical,temporal), interpolates sigma for each of those dimensions, 
   and then multiples the individual sigmas to obtain the final sigma value. So that
   everything scales correctly, all sigma values entered in sstring are first raised to
   the 1/#_dimensions power before perform performing linear interpolation and multiplying.
   The second method (based on fft3dfilter's system) works by computing a single location
   from the seperate dimension locations (x,y,z) as:

       new = sqrt((x * x + y * y + z * z) / 3.0)

   sigma is then interpolated to this location. By default the first system is used.
   To use the second system simply put a '$' sign at the beginning of sstring as shown
   below:

        sstring = "$ 0.0:1.0 1.0:10.0"


    ---------------- ssx/ssy/sst explanation -------------------------------

   sstring breaks the 1D (sbsize=1), 2D (for tbsize=1), or 3D (for sbsize>1 and tbsize>1) 
   frequency spectrum into chunks by normalizing each dimension to [0.0,1.0]... i.e. the
   frequency range [0.0,0.25] is a cube covering the first 1/4 of each dimension. This works
   fine if you want to treat all dimensions the same in terms of how sigma should vary.
   However, if you wanted to ramp sigma based only on temporal frequency or horizontal
   frequency, this is too limited. This is where ssx/ssy/sst come in!
   
   ssx/ssy/sst allow you to specify sigma as a function of horizontal (ssx), vertical (ssy),
   and temporal (sst) frequency only. The syntax is exactly the same as that of sstring. To
   get the final sigma value for a frequency location, the three separate values (one for
   each dimension) are computed and then multiplied together. As with sstring the sigma values
   are first raised to the 1/#_dimensions power before performing linear interpolation and
   multiplying. If you don't specify all three strings, then a flat function equal to the
   'sigma' parameter is used for the missing dimensions. For dimensions of size one (the
   spatial dimenions if sbsize=1 or the temporal dimension for tbsize=1) the corresponding
   string is ignored.

   For example:

        ssx="0.0:1.0 1.0:10.0",ssy="0.0:1.0 1.0:10.0",sst="0.0:1.0 1.0:10.0"

   will give the same result as

        sstring="0.0:1.0 1.0:10.0"

   Or if you want to ramp sigma based on temporal frequency:

        sigma=10.0,sst="0.0:1.0 1.0:10.0"

        This will use 10.0 for the horizontal/vertical dimensions, and ramp
        sigma from 1.0 to 10.0 in the temporal dimension.

   If 'sstring' is specified, it takes precedence over ssx/ssy/sst.


planes -

   A list of the planes to process. By default all planes are processed.
```
