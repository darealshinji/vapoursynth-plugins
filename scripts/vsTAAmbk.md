# vsTAAmbk 0.6.2
A ported AA-script from Avisynth  
For more detials NMM-HD：https://www.nmm-hd.org/newbbs/viewtopic.php?f=23&t=1666

##Requirements:

VapourSynth R28 or newer

####Plugins:
* EEDI2						
* nnedi3						
* RemoveGrain/Repair			
* fmtconv														
* MVTools								
* sangnom
* eedi3_092 (opt)

####Script:
* HAvsFunc r22 or newer (and its requirements)
* Mvsfunc (and its requirements)

##Usage
import vsTAAmbk as taa

aa = taa.TAAmbkX(src, ...)

##Paraments
For more details please visit our threads in NMM-HD Forum.  
* *aatype* - Select main AA method (Default 1).  
    For more details about AA method please visit the thread in NMM-HD.
* *strength* - The downscale factor of predown. 0~0.5 (Default 0).  
    0 - No predown ; 0.5 - predown to half size
* *preaa* - Select the preaa mode. -1~2 (Default 0).  
    0 - No preaa ; 1 - Vertical ; 2 - Horizontal ; -1 - Both  
    Pretty useful against residual comb.  
* *cycle* - Cycle times of main AA (Default 0).  
    The main purpose of cycle is to against 3D aliasing or clip with horrible aliasing
* *mtype* - Select the method to build mask.  
* *mclip* - Use your own mask clip instead of creating one.  
* *mthr*,*mthr2* - Paraments for mask builder.  
    For more details please see the thread in NMM-HD
* *mlthresh* - Set luma thresh for n-pass mask. List  
* *mpand* - Times of expanding and inpanding of the mask. (Default [2,1])  
* *txtprt* - Create a mask to protect white captions on screen. 1~255 (Default None)  
    Value is the luma thresh to determin whether it is captions
* *thin* - Warp the line before main AA by aWarpSharp2. 0 - No thin (Default 0)  
* *dark* - Darken the line before main AA by Toon. 0 - No darken (Default 0.0)  
* *sharp* - Post-sharpen the clip after main AA. 0 - No sharpen (Default 0)  
* *repair* - Repair the clip after Main AA. 0 - No repair (Default 0)  
* *postaa* - Use soothe to counter the aliasing bring in by post-sharpen  
* *src* - Use your own src clip for sharp, repair, mask merge, etc  
* *stabilize* - Stabilize the temporal changes by MVTools. 0~3 (Default 0)  
    Value is the temporal radius. 0 - No Stabilize
* *down8* - Main AA will be done in 8bit if you set this True and input is 16bit  
    Will use LimitFilter to limit the loss
* *showmask* - show the mask  
    0 - Normal output ; 1 - Mask only ; 2 - Stack mask and clip ; 3 - Interleave mask and clip ; -1 - Text mask only
* *eedi3m* - Use nnedi3 to create a mask for eedi3's mclip. Effective when you have eedi3_092
* *other paraments* - Will be collected into a dict for particular aatype
