## KNLMeansCL ##

**KNLMeansCL** is an optimized OpenCL implementation of the Non-local means denoising algorithm. 
Every pixel is restored by the weighted average of all pixels in its search window. The level of averaging is determined by the filtering parameter h. 

#### Requirements ####
- OpenCL driver. AMD: [link](http://support.amd.com), NVIDIA: [link](http://www.nvidia.com/download/find.aspx), Intel: [link](https://software.intel.com/en-us/articles/opencl-drivers).
- [Visual C++ Redistributable Package for Visual Studio 2013](http://www.microsoft.com/en-US/download/details.aspx?id=40784).

#### Syntax ####
```
AviSynth:               KNLMeansCL (clip, int d (0), int a (2), int s (4), 
                                bool cmode(false),  int wmode (1), float h (1.2), 
                                string device_type ("default"), int device_id (0),
                                bool lsb_inout (false), bool info (false))

VapourSynth:            knlm.KNLMeansCL (clip clip, int d (0), int a (2), int s (4), 
                                int cmode(0), int wmode (1), float h (1.2), 
                                string device_type ("default"), int device_id (0), 
                                int info (0)) 
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
                        denoising the current frame. D=0 uses 1 frame, while D=1 uses 3
                        frames and so on. Usually, larger it the better the result of the
                        denoising. Temporal size = (2 * D + 1).

                        Default: 0.


int a                   Set the radius of the search window. A=0 uses 1 pixel, while D=1 
                        use 9 pixels and son on. Usually, larger it the better the result
                        of the denoising. Spatial size = (2 * A + 1)^2.
                        Total search window size = temporal size * spatial size.
	
                        Default: 2.


int s                   Set the radius of the similarity neighborhood window. The impact 
                        on performance is low, therefore it depends on the nature of the 
                        noise. Similarity neighborhood size = (2 * S + 1)^2.
	
                        Default: 4.


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
	
	
float h                 Controls the strength of the filtering. Larger values will 
                        remove more noise.
	                
                        Default: 1.2.


clip rclip              Extra reference clip option to do weighting calculation.
	
                        Default: not set.


string device_type      CPU := An OpenCL device that is the host processor.
                        GPU := An OpenCL device that is a GPU. 
                        ACCELERATOR := Dedicated OpenCL accelerators.
                        DEFAULT := The default OpenCL device in the system.
                        ALL := All OpenCL devices available in the system.
	
                        Default: DEFAULT.
                        

int device_id           The 'device_id'+1º device of type 'device_type' in the system.
                        Example: [device_type = "GPU", device_id = 1] return the second 
                        GPU in the system.
			
                        Default: 0.
	
	
bool lsb_inout          AviSynth hack. Set 16-bit input and output clip.

                        Default: false.
	
	
bool info               Display info on screen. It requires YUV color space.

                        Default: false.
```


## Change Log ##
**- v0.6.4 (2015-09-10)**
  - VapourSynth: removed OpenMP.

**- v0.6.3 (2015-09-07)**
  - Enhanced OpenCL device selection (device_id).
  - VapourSynth linux: fixed wrong locate.
  - Minor changes and bug fixes.

**- v0.6.2 (2015-09-04)**
  - Enhanced error log.
  - Fixed warning use of logical '||' with constant operand.

**- v0.6.1 (2015-09-01)**
  - Fixed 'h'strength in some situations.
  - Minor changes.

**- v0.6.0 (2015-08-26)**
  - Added color distance (cmode).
  - VapourSynth: added support for missing color format.
  - Changed weighted RGB distance.
  - No more SSE2 / SSE3 required. 
  - Minor changes and bug fixes.
  
**- v0.5.9 (2015-08-02)**
  - Fixed rare issues with OpenCL compiler, again. 

**- v0.5.8 (2015-07-29)**
  - Fixed rare issues with OpenCL compiler.
  - Fixed rare issues with clGetPlatformIDs.
  - AviSynth: mem does not need to be aligned.

**- v0.5.7 (2015-07-13)**
  - Extra reference clip.
  - Weighted RGB distance.

**- v0.5.6 (2015-06-26)**
  - VapourSynth: fix temporal support.
  - VapourSynth: fix arg device_type ignored.
  - Fix possible opencl device not available. 

**- v0.5.5 (2015-06-25)**
  - AviSynth: RGB32 support.

**- v0.5.4 (2015-06-06)**
  - AviSynth: fix temporal support with 16-bits clip.  

**- v0.5.3 (2015-05-31)**
  - VapourSynth: better accuracy with 32-bits.
  - VapourSynth: fix arguments.

**- v0.5.2 (2015-05-24)**
  - Add AviSynth+ support.
  - VapourSynth: fix memory leak.

**- v0.5.1 (2015-05-24)**
  - Add VapourSynth support.

