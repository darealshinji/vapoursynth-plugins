# VapourSynth-Bilateral

Bilateral.dll | 2014 | mawen1250

VapourSynth plugin

namespace: bilateral

functions: Bilateral, Gaussian

## Bilateral

### Description

Bilateral filter, can be used to perform spatial de-noise, spatial smoothing while preserving edges.

Larger spatial sigma results in larger smoothing radius, while smaller range sigma preserves edges better.

Now there're 2 different algorithms implemented in this function, algorithm=1 is suitable for large sigmaS and large sigmaR, and algorithm=2 is suitable for small sigmaS and small sigmaR. By default, algorithm=0 will choose the appropriate algorithm to perform the filtering.

If clip ref is specified, it will perform joint/cross Bilateral filtering, which means clip ref is used to determine the range weight of Bilateral filter.

This function accept 8-16bit integer Gray/YUV/RGB/YCoCg input, and clip ref should be of the same format with clip input.

By default, this function will only process Y plane for YUV/YCoCg format, and process all the planes for other formats. It is recommended to use Y as ref clip for chroma plane processing.

### Usage

```python
bilateral.Bilateral(clip input, clip ref=input, float[] sigmaS=3.0, float[] sigmaR=0.02, int[] planes=[], int[] algorithm=0, int[] PBFICnum=[])
```

- input:<br />
    clip to process

- ref: (Default: input)<br />
    Reference clip to calculate range weight.<br />
    Specify it if you want to perform joint/cross Bilateral filter.

- sigmaS: (Default: 3.0)<br />
    sigma of Gaussian function to calculate spatial weight.<br />
    The scale of this parameter is equivalent to pixel distance.<br />
    Larger sigmaS results in larger filtering radius as well as stronger smoothing.<br />
    Use an array to assigne sigmaS for each plane. If sigmaS for the second plane is not specified, it will be set according to the sigmaS of first plane and sub-sampling.<br />
    algorithm=1:<br />
        It is of constant processing time regardless of sigmaS, while in small sigmaS the smoothing effect is stronger compared to Bilateral filter prototype.<br />
    algorithm=2:<br />
        It will be slower as sigmaS increases, and for large sigmaS the approximation will be bad compared to Bilateral filter prototype.

- sigmaR: (Default: 0.02)<br />
    sigma of Gaussian function to calculate range weight.<br />
    The scale of this parameter is the same as pixel value ranging in [0,1].<br />
    Smaller sigmaR preserves edges better, may also leads to weaker smoothing.<br />
    Use an array to specify sigmaR for each plane, otherwise the same sigmaR is used for all the planes.<br />
    algorithm=1:<br />
        As sigmaR decreases, the approximation of this algorithm gets worse, so more PBFICs should be used to produce satisfying result. If PBFICnum is not assigned, the number of PBFICs used will be set according to sigmaR.<br />
    algorithm=2:<br />
        It is of constant processing time regardless of sigmaR, while for large sigmaR the approximation will be bad compared to Bilateral filter prototype.

- planes:<br />
    An array to specify which planes to process.<br />
    By default, chroma planes are not processed.

- algorithm: (Default: 1)<br />
    0 = Automatically determine the algorithm according to sigmaS, sigmaR and PBFICnum.<br />
    1 = O(1) Bilateral filter uses quantized PBFICs. (IMO it should be O(PBFICnum))<br />
    2 = Bilateral filter with truncated spatial window and sub-sampling. O(sigmaS^2)

- PBFICnum:<br />
    Number of PBFICs used in algorithm=1.<br />
    Default: 4 when sigmaR>=0.08. It will increase as sigmaR decreases, up to 32. For chroma plane default value will be odd to better preserve neutral value of chromiance.<br />
    Available range is [2,256].<br />
    Use an array to specify PBFICnum for each plane.

## Gaussian

### Description

Recursive implementation of Gaussian filter, which is of constant processing time regardless of sigma.

The kernel of this function is also used to implement the algorithm of bilateral.Bilateral(algorithm=1).

### Usage

```python
bilateral.Gaussian(clip input, float[] sigma=3.0, float[] sigmaV=sigma)
```

- input:<br />
    clip to process

- sigma: (Default: 3.0)<br />
    sigma of Gaussian function to calculate spatial weight for horizontal filtering.<br />
    Use an array to specify sigma for each plane. If sigma for the second plane is not specified, it will be set according to the sigma of first plane and horizontal sub-sampling.<br />
    Larger sigma results in larger filtering radius as well as stronger blurring in horizontal direction.

- sigmaV: (Default: sigma)<br />
    sigma of Gaussian function to calculate spatial weight for vertical filtering.<br />
    Use an array to specify sigmaV for each plane. If sigmaV for the second plane is not specified, it will be set according to the sigmaV of first plane and vertical sub-sampling.<br />
    Larger sigmaV results in larger filtering radius as well as stronger blurring in vertical direction.
