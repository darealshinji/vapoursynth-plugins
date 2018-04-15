# vsTAAmbk 0.7.5
An Anti-aliasing script ported from Avisynth  
For more details NMM-HD：https://www.nmm-hd.org/newbbs/viewtopic.php?f=23&t=1666

## Requirements:

VapourSynth R28 or newer

#### Plugins:
* [EEDI2](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-EEDI2)						
* [nnedi3](https://github.com/dubhater/vapoursynth-nnedi3)								
* [fmtconv](https://github.com/EleonoreMizo/fmtconv)													
* [MVTools](https://github.com/dubhater/vapoursynth-mvtools)							
* [SangNom](https://bitbucket.org/James1201/vapoursynth-sangnom/overview)
* [TCanny](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-TCanny)
* [eedi3m (optional)](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-EEDI3)
* [nnedi3cl (optional)](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-NNEDI3CL)
* [znedi3 (optional)](https://github.com/sekrit-twc/znedi3)

#### Script:
* [havsfunc](https://github.com/HomeOfVapourSynthEvolution/havsfunc) r22 or newer (and its requirements)
* [mvsfunc](https://github.com/HomeOfVapourSynthEvolution/mvsfunc) (and its requirements)

## Usage
```python
import vsTAAmbk as taa

aa = taa.TAAmbk(clip, aatype=1, aatypeu=None, aatypev=None, preaa=0, strength=0.0, cycle=0, mtype=None, mclip=None,
           mthr=None, mthr2=None, mlthresh=None, mpand=(1, 0), txtmask=0, txtfade=0, thin=0, dark=0.0, sharp=0,
           aarepair=0, postaa=None, src=None, stabilize=0, down8=True, showmask=0, opencl=False, opencl_device=0, **args)
```

## Parameters
For more details please visit our threads in NMM-HD Forum.  
* *clip*:<br />
    A clip to be processed.<br />
	Supported format: Gray and YUV with any subsampling whose bit-depth is 8bit, 10bit or 16bit.<br />

* *aatype*: (Default: 1)<br />
    Select main AA kernel for Y plane.<br />
	Value can be integer whose meaning is same with original TAA script.<br />
	Value can also be name of particular AA kernel.<br />
    For more details about AA kernel please visit the thread in NMM-HD.<br />

* *aatypeu*: (Default: same as aatype)<br />
    Select main AA kernel for U plane when clip's format is YUV.<br />

* *aatypev*: (Default: same as aatype)<br />
    Select main AA kernel for V plane when clip's format is YUV.<br />

* *strength*: (Default: 0)<br />
    The strength of predown. Valid range is 0~0.5<br />
	Before applying main AA kernel, the clip will be downscaled to (1-strength)*clip_resolution first
	and then be upscaled to original resolution by main AA kernel. This may be benefit for clip
	which has terrible aliasing commonly caused by poor upscaling.<br />
	Automatically disabled when using an AA kernel which is not suitable for upscaling.<br />

* *preaa*: (Default: 0)<br />
    Select the preaa mode. <br />
    * 0: No preaa
    * 1: Vertical
    * 2: Horizontal
    * -1: Both

    Perform a preaa before applying main AA kernel.<br />
	Preaa is basically a simplified version of daa.<br />
	Pretty useful for dealing with residual comb caused by poor deinterlace.<br />

* *cycle*: (Default: 0)<br />
    Set times of loop of main AA kernel.<br />
    Use for very very terrible aliasing and 3D aliasing.<br />

* *mtype*: (Default: 1)<br />
    Select type of edge mask to be used.<br />
    Currently there are three mask types:
    * 0: No mask
    * 1: Canny mask
    * 2: Sobel mask
    * 3: Prewitt mask

	Mask always be built under 8bit scale.<br />

* *mclip*: (Default: None)<br />
    Use your own mask clip instead of building one.<br />
	If mclip is set, script won't build another one. And you should take care of
	mask's resolution, bit-depth, format, etc by yourself.<br />

* *mthr*, *mthr2*:<br />
    Parameters of mask.<br />
    Meaning of them depends on particular mtype.<br />

* *mlthresh*: (Default None)<br />
    Set luma thresh for n-pass mask.<br />
    Use a list or tuple to specify the sections of luma.<br />

* *mpand*: (Default: (1, 0))<br />
    Use a list or tuple to specify the loop of mask expanding and mask inpanding.<br />

* *txtmask*: (Default: 0)<br />
    Create a mask to protect white captions on screen.<br />
    Value is the threshold of luma. Valid range is 0~255.<br />
	When a area whose luma is greater than threshold and chroma is 128±2, it will be
	considered as a caption.<br />

* *txtfade*: (Default: 0)<br />
    Set the length of fading. Useful for fading text.<br />

* *thin*: (Default: 0)<br />
    Warp the line by aWarpSharp2 before applying main AA kernel.<br />

* *dark*: (Default: 0.0)<br />
    Darken the line by Toon before applying main AA kernel.<br />

* *sharp*: (Default: 0)<br />
    Sharpen the clip after applying main AA kernel.<br />
	* 0: No sharpen.<br />
    * 1~inf: LSFmod(defaults='old')
    * 0~1: Simlar to Avisynth's sharpen()
    * -1~0: LSFmod(defaults='fast')
    * -1: Contra-Sharpen

    Whatever type of sharpen, larger absolute value of sharp means larger strength of sharpen.<br />

* *aarepair*: (Default: 0)<br />
    Use repair to remove artifacts introduced by main AA kernel.<br />
	According to different repair mode, the pixel in src clip will be replaced by
	the median or average in 3x3 neighbour of processed clip.<br />
	It's highly recommend to use repair when main AA kernel contain SangNom.<br />

* *postaa*: (Default: False)<br />
    Whether use soothe to counter the aliasing introduced by sharpening.<br />

* *src*: (Default: clip)<br />
    Use your own src clip for sharp, repair, mask merge, etc.<br />

* *stabilize*: (Default: 0)<br />
    Stabilize the temporal changes by MVTools.<br />
    Value is the temporal radius. Valid range is 0~3.<br />

* *down8*: (Default: True)<br />
    If you set this to True, the clip will be down to 8bit before applying main AA kernel
	and up it back to original bit-depth after applying main AA kernel.<br />
	LimitFilter will be used to reduce the loss in depth conversion.<br />

* *showmask*: (Default: 0)<br/>
    Output the mask instead of processed clip if you set it to not 0.<br />
    0: Normal output; 1: Mask only; 2: tack mask and clip; 3: Interleave mask and clip; -1: Text mask only<br />

* *opencl*: (Default: False)<br />
    Whether use opencl to accelarate processing.<br />
    Currently there are two processings can use opencl:<br />
    * TCannyCL
    * EEDI3CL
    <br />

* *opencl_device*: (Default: 0)<br />
    Select an opencl device.<br />

* *other parameters*:<br />
    Will be collected into a dict for particular aatype.<br />
