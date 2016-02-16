# VapourSynth-Retinex

Retinex.dll | 2014 | mawen1250

VapourSynth plugin

namespace: retinex

functions: MSRCP, MSRCR

## About Retinex

The Retinex theory and algorithm mainly aims at simulating the color constancy feature of HVS(Human Visual System).

The light perceived by visual receptors can be separated into illuminance and reflectance. Retinex estimates the illuminance and derive the reflectance from the light, the filtered result of which is an image represents the reflectance characteristics of the scene, regardless of the illuminance.

Retinex is a very powerful filter in dynamic range compression, local contrast enhancement, color constancy, de-fog, etc.

## MSRCP

### Description

MSR(Multi Scale Retinex) is the most successful implementation of Retinex, based on center/surround theory.

MSRCP(Multi Scale Retinex with Chromaticity Preservation) is based on MSR. It applies MSR on intensity channel, and adjust UV/RGB according to the filtering result of intensity channel to preserve chromaticity.

As MSRCP preserves chromaticity, it is excellent for dynamic range compression and local contrast enhancement, while it doesn't eliminate color cast. To implement the full color constancy feature of Retinex, it is recommended to use MSRCR(Multi Scale Retinex with Color Restoration) instead.

This function accept 8-16bit integer Gray/YUV/RGB/YCoCg input. Sub-sampled format is not supported. If you want to process YUV420/YUV422 clip, convert it to YUV444 or RGB first.

For processing in YUV444 and RGB, the filtering results are different. The intensity channel on which MSR is applied, is Y for YUV444 input and (R+G+B)/3 for RGB input. Since Y is a weighted average of R, G, B, processing in YUV444 may produce imbalanced chromaticity preservation. Also when chroma_protect is larger than 1 (default 1.2), the saturation of YUV444 processing result will be different from that of RGB processing result.

### Usage

```python
retinex.MSRCP(clip input, float[] sigmaS=[25,80,250], float lower_thr=0.001, float upper_thr=0.001, bool fulls, bool fulld=fulls, float chroma_protect=1.2)
```

- input:<br />
    clip to process

- sigma: (Default: [25,80,250])<br />
    sigma of Gaussian function to apply Gaussian filtering.<br />
    Assign an array of multiple sigma to apply MSR.<br />
    Basically, in SSR(Single Scale Retinex), small sigma result in stronger dynamic range compression and local contrast enhancement, while large sigma result in better color rendition. To afford an acceptable trade-off between these features, MSR combines different scales to compute the final Retinex output.<br />
    It is recommended by most researches to use 3 different scales (balance between speed and quality).

- lower_thr: (Default: 0.001)<br />
    After applying MSR, a normalization is applied to the value range [Floor,Ceil] of the Retinex output.<br />
    This parameter define the ratio of pixel number to determine the Floor of value range. 0 means using the minimum value in the Retinex output.<br />
    Valid range is [0,1), and the sum of lower_thr and upper_thr should be less than 1.<br />
    Increase it to enhance the global contrast of the output image.

- upper_thr: (Default: 0.001)<br />
    After applying MSR, a normalization is applied to the value range [Floor,Ceil] of the Retinex output.<br />
    This parameter define the ratio of pixel number to determine the Ceil of value range. 0 means using the maximum value in the Retinex output.<br />
    Valid range is [0,1), and the sum of lower_thr and upper_thr should be less than 1.<br />
    Increase it if there are some extreme bright parts in the Retinex output which makes the whole image too dark.

- fulls: (Default: True for RGB/YCoCg input, False for YUV/Gray input)<br />
    Determine the value range of input clip. True means full range/PC range, and False means limited range/TV range.

- fulld: (Default: fulls)<br />
    Determine the value range of output clip. True means full range/PC range, and False means limited range/TV range.<br />
    Set different value for fulls and fulld will result in range conversion.

- chroma_protect: (Default: 1.2)<br />
    The base of log function to attenuate chroma adjustment. It could avoid extreme chroma amplifying, while the saturation of the result is changed.<br />
    Available range is [1, +inf), 1 means no attenuation.<br />
    It is only available for YUV/YCoCg input.

### Example

TV range YUV420P8 input, filtered in TV range YUV444P16 with chroma protect, output TV range YUV444P16

```python
v = core.fmtc.resample(v, csp=vs.YUV444P16)
v = core.retinex.MSRCP(v, chroma_protect=1.2)
```

JPEG image(PC range YUV420P8 with MPEG-1 chroma placement) input, filtered in PC range YUV444P16 without chroma protect, output PC range RGB48

```python
i = core.lsmas.LWLibavSource(r'Image.jpg')
i = core.fmtc.resample(i, csp=vs.YUV444P16, fulls=True, cplace="MPEG1")
i = core.retinex.MSRCP(i, fulls=True, chroma_protect=1)
i = core.fmtc.matrix(i, mat="601", fulls=True, csp=vs.RGB48)
```

PNG image(PC range RGB24) input, filtered in PC range RGB48, output PC range RGB48

```python
i = core.lsmas.LWLibavSource(r'Image.png')
i = core.fmtc.bitdepth(i, bits=16)
i = core.retinex.MSRCP(i)
```

## MSRCR

### Description

MSRCR(Multi Scale Retinex with Color Restoration) is based on MSR. It applies MSR to each spectral channel (e.g. R, G and B), and modify the MSR output by multiplying it by a color restoration function of the chromaticity.

When MSR is applied to each spectral channel, it assumes the image obey gray world assumption. Otherwise, if the image violates gray world assumption, the MSR will produce grayish image by decreasing the color saturation, thus the color restoration step is proposed to solve this problem. However, for images with nice color balance, MSRCR still produces a desaturated look. Hence it is recommended to use MSRCP in most cases, and only apply MSRCR to the images with color cast. Also since MSRCR applies MSR to each spectral channel instead of intensity channel, it is slower than MSRCP.

This function only accept 8-16bit integer RGB input.

### Usage

```python
retinex.MSRCR(clip input, float[] sigmaS=[25,80,250], float lower_thr=0.001, float upper_thr=0.001, bool fulls=True, bool fulld=fulls, float restore=125)
```

- input:<br />
    clip to process

- sigma: (Default: [25,80,250])<br />
    The same as MSRCP.

- lower_thr: (Default: 0.001)<br />
    The same as MSRCP.

- upper_thr: (Default: 0.001)<br />
    The same as MSRCP.

- fulls: (Default: True)<br />
    The same as MSRCP.

- fulld: (Default: fulls)<br />
    The same as MSRCP.

- restore: (Default: 125)<br />
    The strength of the nonlinearity for color restoration function, larger value result in stronger restoration, available range is [0, +inf).<br />
    It is a multiplier in a log function, so try to adjust it in a large scale (e.g. multiply it by a power of 10) if you want to see any difference.

### Example

TV range YUV420P8 input, filtered in PC range RGB48, output PC range RGB48

```python
v = core.fmtc.resample(v, csp=vs.YUV444P16)
v = core.fmtc.matrix(v, mat="709", csp=vs.RGB48)
v = core.retinex.MSRCR(v)
```

JPEG image(PC range YUV420P8 with MPEG-1 chroma placement) input, filtered in PC range RGB48 without color restoration (pure MSR), output PC range RGB48

```python
i = core.lsmas.LWLibavSource(r'Image.jpg')
i = core.fmtc.resample(i, csp=vs.YUV444P16, fulls=True, cplace="MPEG1")
i = core.fmtc.matrix(i, mat="601", fulls=True, csp=vs.RGB48)
i = core.retinex.MSRCR(i, restore=0)
```

PNG image(PC range RGB24) input, filtered in PC range RGB48, output PC range RGB48

```python
i = core.lsmas.LWLibavSource(r'Image.png')
i = core.fmtc.bitdepth(i, bits=16)
i = core.retinex.MSRCR(i)
```
