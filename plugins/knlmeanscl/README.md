# KNLMeansCL #
[![PayPal](https://www.paypalobjects.com/webstatic/en_US/btn/btn_donate_74x21.png)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=52QYMVWFRCQQY&lc=GB&item_name=KNLMeansCL&currency_code=EUR&bn=PP%2dDonationsBF%3abtn_donate_74x21%2epng%3aNonHosted) [![Build Status](https://travis-ci.org/Khanattila/KNLMeansCL.svg?branch=master)](https://travis-ci.org/Khanattila/KNLMeansCL) [![GitHub release](https://img.shields.io/github/release/Khanattila/KNLMeansCL.svg)](https://github.com/Khanattila/KNLMeansCL/releases) [![GitHub download](https://img.shields.io/github/downloads/Khanattila/KNLMeansCL/latest/total.svg)](https://github.com/Khanattila/KNLMeansCL/releases) 

**KNLMeansCL** is an optimized OpenCL implementation of the Non-local means de-noising algorithm. Every pixel is restored by the weighted average of all pixels in its search window. The level of averaging is determined by the filtering parameter h. 

For maximum performance the plugin should run on the GPU. The library is written in C/C++ and the source is available under the [GNU General Public License v3.0](https://github.com/Khanattila/KNLMeansCL/blob/master/LICENSE).

### AviSynth(+) and VapourSynth ###
For end user KNLMeansCL is a plugin for [AviSynth](http://avisynth.nl) / [AviSynth+](http://avs-plus.net/) and for [VapourSynth](http://www.vapoursynth.com). Windows, OS X and Linux are supported. For documentation and small benchmark see the [doc](https://github.com/Khanattila/KNLMeansCL/blob/master/DOC.md).

### Requirements ###
- AMD HD 5800 Series GPU or greater. At least [AMD Catalyst™ software 11.12](http://support.amd.com).
- Intel Graphics 2500/4000 or greater. At least [OpenCL™ Drivers 2013](http://software.intel.com/en-us/articles/opencl-drivers).
- NVIDIA GeForce GT 640 or greater. At least [NVIDIA driver 350.12 WHQL](http://www.nvidia.com/download/find.aspx).
- CPU fallback is still available. Install [AMD APP SDK](http://developer.amd.com/tools-and-sdks/opencl-zone/amd-accelerated-parallel-processing-app-sdk/) or [Intel OpenCL™ Runtime](http://software.intel.com/en-us/articles/opencl-drivers).
- [Visual C++ Redistributable Package for Visual Studio 2015](http://www.microsoft.com/en-us/download/details.aspx?id=48145).

### Legacy - [v0.6.11](https://github.com/Khanattila/KNLMeansCL/releases/tag/v0.6.11)###
- AMD HD 5400 Series GPU or greater.
- Intel Graphics 2500/4000 or greater.
- NVIDIA GeForce 8400 GS or greater.
- CPU fallback is still available. Install [AMD APP SDK](http://developer.amd.com/tools-and-sdks/opencl-zone/amd-accelerated-parallel-processing-app-sdk/) or [Intel OpenCL™ Runtime](http://software.intel.com/en-us/articles/opencl-drivers).
- [Visual C++ Redistributable Package for Visual Studio 2013](http://www.microsoft.com/en-US/download/details.aspx?id=40784).

#### Syntax ####
```
AviSynth:               KNLMeansCL (clip, int d (0), int a (2), int s (4), 
                                float h (1.2), bool cmode(false),  int wmode (1), 
                                float wref (1.0), string device_type ("auto"),
                                int device_id (0), bool lsb_inout (false), 
                                bool info (false))

VapourSynth:            knlm.KNLMeansCL (clip clip, int d (0), int a (2), int s (4), 
                                float h (1.2), int cmode(False), int wmode (1), 
                                float wref (1.0), string device_type ("auto"),  
                                int device_id (0), int info (False)) 
```

#### Supported image format ####
```
AviSynth:               RGB32, Y8, YV411, YV12, YV16, YV24.

VapourSynth: 	        All.
```


#### Parameters ####
``` 
clip clip               Video source.
	

int d                   Set the number of past and future frame that the filter uses for 
                        denoising the current frame. d=0 uses 1 frame, while d=1 uses 3
                        frames and so on. Usually, larger it the better the result of the
                        denoising. Temporal size = (2 * d + 1).

                        Default: 0.


int a                   Set the radius of the search window. a=0 uses 1 pixel, while a=1 
                        uses 9 pixels and son on. Usually, larger it the better the result
                        of the denoising. Spatial size = (2 * a + 1)^2.
                        Total search window size = temporal size * spatial size.
	
                        Default: 2.


int s                   Set the radius of the similarity neighborhood window. The impact 
                        on performance is low, therefore it depends on the nature of the 
                        noise. Similarity neighborhood size = (2 * s + 1)^2.
	
                        Default: 4.

                        
float h                 Controls the strength of the filtering. Larger values will 
                        remove more noise.
	                
                        Default: 1.2.


bool cmode              Use color distance instead of gray intensities. Normally 
                        KNLMeansCL processes only Luma and copy Chroma if present. If 
                        cmode is true KNLMeansCL processes Luma and Chroma together. 
                        If color space is RGB, cmode is always true.
	
                        Default: false.
	

int wmode               0 := Cauchy weighting function has a very slow decay. It assigns 
                        larger weights to dissimilar blocks than the Leclerc robust 
                        function, which will eventually lead to oversmoothing.
                        
                        1 := Leclerc weighting function has a faster decay, but still
                        assigns positive weights to dissimilar blocks. Original NLMeans 
                        weighting function.
                        
                        2 := Bisquare weighting function use a soft threshold to compare 
                        neighbourhoods (the weight is 0 as soon as a giventhreshold is 
                        exceeded).
	
                        Default: 1.
                        
                       
float wref              Amount of original pixel to contribute to the filter output, 
                        relative to the weight of the most similar pixel found.
	                
                        Default: 1.0.
                        
	
clip rclip              Extra reference clip option to do weighting calculation.
	
                        Default: not set.


string device_type      CPU := An OpenCL device that is the host processor.
                        GPU := An OpenCL device that is a GPU. 
                        ACCELERATOR := Dedicated OpenCL accelerators.
                        AUTO := GPU -> ACCELERATOR -> CPU.
	
                        Default: AUTO.
                        
                        
int device_id           The 'device_id'+1º device of type 'device_type' in the system.
                        Example: [device_type = "GPU", device_id = 1] return the second 
                        GPU in the system.
			
                        Default: 0.
	
	
bool lsb_inout          AviSynth hack. Set 16-bit input and output clip.

                        Default: false.
	
	
bool info               Display info on screen. It requires YUV color space.

                        Default: false.
```

#### Troubleshooting ####
Some NVIDIA users have reported a problem with the library 'OpenCL.dll'. Replace it (C:\Windows\System32\OpenCL.dll and/or C:\Windows\SysWOW64\OpenCL.dll) with the one provided in the .zip or replace it with the one provided by NVIDIA (C:\Program Files\NVIDIA Corporation\OpenCL\OpenCL.dll).

#### Benchmark ####

| | cmode | AviSynth | VapourSynth |
| :--------------- | :---------------: | :---------------: | :---------------: |
| YUV420P8 | False | 37 fps  | 35 fps  |
| YUV420P9  | False | - | 33 fps |
| YUV420P10  | False | -  | 33 fps  |
| YUV420P16  | False | 35 fps  | 34 fps  |
| YUV444P8  | True | 28 fps  | 27 fps  |
| YUV444P9  | True | -  | 22 fps  |
| YUV444P10  | True | -  | 23 fps  |
| YUV444P16  | True | 23 fps  | 23 fps  |
| RGB24  | - | - | 27 fps  |
| RGB27  | - | - | 23 fps  |
| RGB30  | - | - | 23 fps  |
| RGB48  | - | - | 23 fps  |
| RGBA32 | - | 28 fps  | - |

720p, KNLMeansCL(d=1, a=2, s=4, device_type="GPU") - v0.7.0.

Tested with the following configuration: Intel Core i5 2500K (4.2GHz), 8GB DDR3-1600 MHz, NVIDIA GeForce GTX 760, Windows 10 64bit.

## Change Log ##
**-v 0.7.6**
  - Fixed wrong version displaying.
  - VapourSynth: fixed wrong API usage.

**-v 0.7.5**
  - New 'wref' (default 1.0) parameter.
  - Improved automatic selection of OpenCL device.
  - Changed parameters order. 
  - VapourSynth: fixed missing 's' value check. 

**- v0.7.4**
  - Workaround for GeForce 900 Series.
   
**- v0.7.3**
  - Removed 'default' and 'all' from 'device_type'.
  - Improved OpenCL platform control.
  - VapourSynth: fixed frame properties.

**- v0.7.2**
  - VapourSynth: fixed memory leak.
  
**- v0.7.1**
  - Fixed 'clSetKernelArg' error in some configuration.
  
**- v0.7.0**
  - Performance increase: temporal up to 35% faster.
  - New 'auto' device_type.
  - Changed default 'd' value from '0' to '1'.
  - Changed min 'a' value from '0' to '1'.
  - OpenCL 1.2 support now required.
  - AviSynth 2.6.0a1-a5 backward compatibility.
  - Fixed Mac OS X version.
  
**- v0.6.11**
  - Performance increase: up to 50% faster.
  - Fixed bad processing borders.
  - AviSynth: OpenMP no more needed.
  - VapourSynth: fixed bad support of P9/P10 rclip.

**- v0.6.4**
  - VapourSynth: OpenMP no more needed.

**- v0.6.3**
  - Enhanced OpenCL device selection (device_id).
  - VapourSynth linux: fixed wrong locate.
  - Minor changes and bug fixes.

**- v0.6.2**
  - Enhanced error log.
  - Fixed warning use of logical '||' with constant operand.

**- v0.6.1**
  - Fixed 'h' strength in some situations.
  - Minor changes.

**- v0.6.0**
  - Added color distance (cmode).
  - VapourSynth: added support for missing color format.
  - Changed weighted RGB distance.
  - No more SSE2 / SSE3 required. 
  - Minor changes and bug fixes.
  
**- v0.5.9**
  - Fixed rare issues with OpenCL compiler, again. 

**- v0.5.8**
  - Fixed rare issues with OpenCL compiler.
  - Fixed rare issues with clGetPlatformIDs.
  - AviSynth: clip does not need to be aligned.

**- v0.5.7**
  - Added extra reference clip (rclip).
  - Weighted RGB distance.

**- v0.5.6**
  - VapourSynth: fixed temporal support.
  - VapourSynth: fixed arg device_type ignored.
  - Fixed possible opencl device not available. 

**- v0.5.5**
  - AviSynth: added RGB32 support.

**- v0.5.4**
  - AviSynth: fixed temporal support with 16-bits clip.  

**- v0.5.3**
  - VapourSynth: better accuracy with 32-bits.
  - VapourSynth: fixed arguments.

**- v0.5.2**
  - Added AviSynth+ support.
  - VapourSynth: fixed memory leak.

**- v0.5.1**
  - Added VapourSynth support.
