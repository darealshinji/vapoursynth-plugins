# VapourSynth-waifu2x-opt

Copyright© 2015 mawen1250

waifu2x image super-resolution/denoise filter for VapourSynth, based on the waifu2x-opt library

## Description

It's a high quality image restoration filter for anime, mainly for super-resolution, also contains 2 denoising modes.

This plugin employs [waifu2x-opt library](https://github.com/logicmachine/waifu2x-opt) and [zimg library](https://github.com/sekrit-twc/zimg) for scaling related stuffs. Thanks for their works!

Note that the waifu2x-opt is dynamic linked while the zimg is static linked.

Also the waifu2x-opt requires 3 model files in the same folder of vs_waifu2x.dll.

Thus there're totally 5 files you need to put into a same folder (such as the VapourSynth plugin auto-load folder):
  - vs_waifu2x.dll
  - waifu2x.dll
  - noise1_model.json
  - noise2_model.json
  - scale2.0x_model.json

namespace: waifu2x

functions: Denoise, Resize

## Important Note

- This filter is *very very slow*, much slower than my implementation of the [BM3D denoising filter](https://github.com/HomeOfVapourSynthEvolution/VapourSynth-BM3D).

- Few denoising parameters can be adjusted and the denoising is only applied to Y channel. Thus I would recommend BM3D for general denoising purpose rather than waifu2x.Denoise, the denoising quality of both filters achieve the state of art.

- waifu2x.Denoise is specially trained to deal with JPEG compression artifacts, thus it can do a very good job to eliminate ringing artifacts.

- The memory consumption can be very high due to large amount of buffers allocated during processing, and it will multiply if multiple VS threads is used. You can decrease "block_width" and "block_height" to reduce memory consumption. Also it's possible to employ the internal MT of waifu2x by setting "threads".

## Usage

### Denoising filter

This filter applies denoising to the Y channel, and keeps UV channels unchanged.<br />
For RGB input, it is automatically converted to YUV for processing, then converted back to RGB for output.

```python
waifu2x.Denoise(clip input[, int mode=1, int matrix=6, bint full, int block_width=1280, int block_height=1280, int threads=1])
```

- input:<br />
    The input clip, can be of Gray, RGB, YUV or YCoCg color family. 8-16 bit integer or 32 bit float is supported.<br />
    The internal process is always done in 32 bit float.<br />
    The output clip is of the same format as the input clip, thus it's recommended to use high bit depth input to avoid rounding error.

- mode:<br />
    Denoising model file to be used.<br />
  - 1 - noise1_model.json, for weak noise (default)
  - 2 - noise2_model.json, for strong noise

- matrix:<br />
    Matrix coefficients for RGB input, default 6.<br />
    The internal processing is always done in YUV color space, and this specifies the matrix used for RGB<->YUV conversion.<br />
    The model should be trained on BT.601 YUV data, so it might be best to always use 6.<br />
    The number is as specified in ISO/IEC 14496-10, with an additional one for OPP.<br />
      - 0 - GBR
      - 1 - bt709
      - 2 - Unspecified
      - 4 - fcc
      - 5 - bt470bg
      - 6 - smpte170m
      - 7 - smpte240m
      - 8 - YCgCo
      - 9 - bt2020nc
      - 10 - bt2020c
      - 100 - OPP, opponent color space

- full:<br />
    Specify if input/output clip is of full range (PC range) or limited range (TV range).<br />
    Default is true for RGB and YCoCg input, false for Gray and YUV input.

- block_width, block_height:<br />
    The block size used in waifu2x, it splits the processed image into several sub-images, which can significantly reduce memory consumption, while slightly increases computational cost.<br />
    Set a smaller value to lower the memory consumption.<br />
    0 means the block size is the image size with proper padding.<br />
    Negative value forces a specific block size.<br />
    Positive value will be automatically adjusted to the closest optimal size no larger than the set value, with proper padding.

- threads:<br />
    The number of threads used for internal multi-thread processing, as an alternative to the VapourSynth frame-level MT.<br />
    It won't increase memory consumption, but the performance is not that efficient as that of the VS MT, and the CPU usage is not very stable.<br />
    It's recommended to use it for single image processing or previewing with vsedit, etc.<br />
    For video processing, it's better to use the VS MT. If needed, decrease block_width and block_height for less memory consumption.<br />
    Set 0 to automatically detect the number of threads to be used.<br />
    Default is 1, thus only single-thread is used.

### Super-resolution filter

This filter applies scaling in YUV color space.<br />
For RGB input, it is automatically converted to YUV for processing, then converted back to RGB for output.<br />
The Y channels is first scaled to 2x using nearest-neighborhood interpolation, then waifu2x is applied to convert the low-resolution image to the high-resolution one (though the image size is unchanged). If any custom scaling parameter (width, height, shift_w, shift_h, subwidth, subheight) is set, a post-scaling is applied to the high-resolution image to get the final output image.<br />
The UV channels are scaled using Catmull-Rom (Bicubic, b=0, c=0.5) by default.

```python
waifu2x.Resize(clip input[, int width=input.width*2, int height=input.height*2, float shift_w=0, float shift_h=0, float subwidth=input.width, float subheight=input.height, string filter="bicubic", float filter_param_a=0, float filter_param_b=0.5, string filter_uv="bicubic", float filter_param_a_uv=0, float filter_param_b_uv=0.5, int subsample_w=input.format.subsampling_w, int subsample_h=input.format.subsampling_h, string chroma_loc_in="mpeg2", string chroma_loc_out=chroma_loc_in, int matrix=6, bint full, int block_width=1280, int block_height=1280, int threads=1])
```

- input:<br />
    The input clip, can be of Gray, RGB, YUV or YCoCg color family. 8-16 bit integer or 32 bit float is supported.<br />
    The internal process is always done in 32 bit float.<br />
    The output clip is of the same color family and depth as the input clip, thus it's recommended to use high bit depth input to avoid rounding error.

- width, height, shift_w, shift_h, subwidth, subheight:<br />
    Scaling parameters, same as the ones in z.Resize.<br />
    By default, width and height is 2x that of the input clip.

- filter, filter_param_a, filter_param_b:<br />
    The kernel parameters used for Y channel post-scaling, same as the ones in z.Resize.<br />
    By default, filter="bicubic", filter_param_a=0, filter_param_b=0.5, also known as Catmull-Rom.

- filter_uv, filter_param_a_uv, filter_param_b_uv:<br />
    The kernel parameters used for UV channel scaling, same as the ones in z.Resize.<br />
    By default, filter_uv="bicubic", filter_param_a_uv=0, filter_param_b_uv=0.5, also known as Catmull-Rom.

- subsample_w, subsample_h:<br />
    The output chroma sub-sampling for YUV or YCoCg color family input, same as the ones in z.Resize.<br />
    For example, subsample_w=0, subsample_h=0 means 4:4:4, subsample_w=1, subsample_h=1 means 4:2:0.<br />
    Default is same as the input.

- chroma_loc_in, chroma_loc_out:<br />
    The input/output chroma location (chroma placement), can be "mpeg2" or "mpeg1".<br />
    Default chroma_loc_in is "mpeg2", default chroma_loc_out is chroma_loc_in.

- matrix, full:<br />
    Same as the ones in waifu2x.Denoise.

- block_width, block_height, threads:<br />
    Same as the ones in waifu2x.Denoise.<br />
    Note that block_width and block_height is applied on the 2x image. Thus for 1280x1280 input, it's 2560x2560 after 2x, and it will be split into 4 sub-images if block_width=1280 and block_height=1280.
