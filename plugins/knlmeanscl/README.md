# KNLMeansCL #
[![PayPal](https://www.paypalobjects.com/webstatic/en_US/btn/btn_donate_74x21.png)](https://www.paypal.com/cgi-bin/webscr?cmd=_donations&business=52QYMVWFRCQQY&lc=GB&item_name=KNLMeansCL&currency_code=EUR&bn=PP%2dDonationsBF%3abtn_donate_74x21%2epng%3aNonHosted) [![Build Status](https://travis-ci.org/Khanattila/KNLMeansCL.svg?branch=master)](https://travis-ci.org/Khanattila/KNLMeansCL) [![GitHub release](https://img.shields.io/github/release/Khanattila/KNLMeansCL.svg)](https://github.com/Khanattila/KNLMeansCL/releases) [![GitHub download](https://img.shields.io/github/downloads/Khanattila/KNLMeansCL/latest/total.svg)](https://github.com/Khanattila/KNLMeansCL/releases) 

**KNLMeansCL** is an optimized OpenCL implementation of the Non-local means de-noising algorithm. Every pixel is restored by the weighted average of all pixels in its search window. The level of averaging is determined by the filtering parameter h. 

For maximum performance the plugin should run on the GPU. The library is written in C/C++ and the source is available under the [GNU General Public License v3.0](https://github.com/Khanattila/KNLMeansCL/blob/master/LICENSE).

### AviSynth(+) and VapourSynth ###
For end user KNLMeansCL is a plugin for [AviSynth](http://avisynth.nl) / [AviSynth+](http://avs-plus.net/) and for [VapourSynth](http://www.vapoursynth.com). Windows, OS X and Linux are supported. For documentation and small benchmark see the [doc](https://github.com/Khanattila/KNLMeansCL/blob/master/DOCUMENTATION.md).

### Requirements ###
- AMD HD 5800 Series GPU or greater. At least [AMD Catalyst™ software 11.12](http://support.amd.com).
- Intel Graphics 2500/4000 or greater. At least [OpenCL™ Drivers 2013](http://software.intel.com/en-us/articles/opencl-drivers).
- NVIDIA GeForce GT 640 or greater. At least [NVIDIA driver 350.12 WHQL](http://www.nvidia.com/download/find.aspx).
- If you have an older device, you can use this [version](http://github.com/Khanattila/KNLMeansCL/releases/tag/v0.6.11).
- [Visual C++ Redistributable Package for Visual Studio 2013](http://www.microsoft.com/en-US/download/details.aspx?id=40784).


## Change Log ##
**- v0.7.4**
  - Workaround for GeForce 900 Series.

**- v0.7.3**
  - Removed 'default' and 'all' device_type.
  - More controls of the OpenCL platform.
  - Better error handling.
  - VapourSynth: fixed frame properties.

**- v0.7.2**
  - VapourSynth: fixed memory leak.
  
**- v0.7.1**
  - Fixed clSetKernelArg error in some configuration.
  
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
  - AviSynth: OpenMP no more needed.
  - Fixed bad processing borders.
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
