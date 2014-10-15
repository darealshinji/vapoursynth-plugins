================================================
vsimagereader - Image reader for VapourSynth
================================================

Image file reader plugin for VapourSynth r19 or later.

Supported formats are
    - BMP (Windows Bitmap)
    - JPEG
    - PNG (Portable Network Graphics)
    - TARGA (Truevision Advanced Raster Graphics Adapter)

Function:
---------
Currently, this plugin has one function.::

    imgr.Read(data[] files[, int fpsnum, int fpsden, bint alpha])

files - list of the file path of the images.

fpsnum - Framerate numerator. Default is 24.

fpsden - Framerate denominator. Default is 1.

alpha - When input image has alpha channel, this filter returns a list which has two clips. clip[0] is base clip. clip[1] is alpha clip. If image does not have alpha, clip[1] will be black(all 0) frame.

Usage:
------
    >>> import vapoursynth as vs
    >>> core = vs.Core()
    >>> core.std.LoadPlugin('/path/to/vsimagereader.dll')

    - read single file:
    >>> clip = core.imgr.Read('/path/to/file')

    - read two or more files:
    >>> srcs = ['/path/to/file1', '/path/to/file2', ... ,'/path/to/fileX']
    >>> clip = core.imgr.Read(srcs)

    - read image sequence:
    >>> import os
    >>> ext = '.png'
    >>> dir = '/path/to/the/directory/'
    >>> srcs = [dir + src for src in os.listdir(dir) if src.endswith(ext)]
    >>> clip = core.imgr.Read(srcs)

    - enable alpha:
    >>> clip = core.imgr.Read(srcs, alpha=True)
    >>> base = clip[0]
    >>> alpha = clip[1]

    - read variable width/height/format images
    >>> srcs = ['320x240_bgr.bmp', '640x360_420.jpeg']
    >>> clip = core.imgr.Read(srcs)
    >>> clip = core.std.AddBorders(clip, right=320, bottom=120)
    >>> clip = core.std.CropAbs(clip, width=640, height=360) # all frames are 640x360
    >>> clip = core.resize.Bicubic(clip, format=vs.COMPATBGR32) # all frames are COMPATBGR32

About supported format:
-----------------------

    - BMP:
        1/2/4/8/24/32bit color RGB are supported except RLE compressed.

        output format is always RGB24.

    - JPEG:
        Generally, JPEG images are compressed after converted source RGB to 8bit planar YUV, and they are reconverted to RGB at the time of decoding.

        vsimagereader omits the reconversion to RGB, and keep them with YUV.

        If chroma-subsample-type of the image is YUV420 or YUV422, the width of that will be make into mod 2 with padding.

        Also, if subsample-type of the image is YUV420 or YUV440, the height of that will be make into mod 2 with padding.

    - PNG:
        1/2/4bits samples will be expanded to 8bits.

    - TARGA:
        Only 24bit/32bit-RGB(uncompressed or RLE compressed) are supported. Color maps are not.

Note:
-----
    - vsimagereader is using TurboJPEG/OSS library for parsing/decoding JPEG image.
      TurboJPEG/OSS is part of libjpeg-turbo project. libjpeg-turbo is a derivative of libjpeg that uses SIMD instructions (MMX, SSE2, NEON) to accelerate baseline JPEG compression and decompression on x86, x86-64, and ARM systems.
    - vsimagereader is using libpng for parsing/decoding PNG image.
    - vsimagereader is using part of libtga's source code for decoding compressed TARGA image.

How to compile:
---------------
    vsimagereader requires libpng-1.2(1.2.50 or later is recommended) and libturbojpeg-1.2(1.2.1 or later is recommended).

    You can also use new libpng-1.4 or later(1.6.2 or later is recommended) instead of libpng-1.2.

    And, libpng requires zlib-1.0.4 or later(1.2.7 or later is recommended).

    Therefore, you have to install these libraries at first.

    If you have already installed them, type as follows.::

    $ git clone git://github.com/chikuzen/vsimagereader.git
    $ cd ./vsimagereader/src
    $ ./configure --enable-new-png
    $ make

Link:
-----
    vsimagereader source code repository:
        https://github.com/chikuzen/vsimagereader

    libjpeg-turbo:
        http://www.libjpeg-turbo.org/

    Independent JPEG Group:
        http://www.ijg.org/

    libpng.org:
        http://www.libpng.org/

    zlib:
        http://www.zlib.net/

    libtga:
        http://tgalib.sourceforge.net/

Author: Oka Motofumi (chikuzen.mo at gmail dot com)
