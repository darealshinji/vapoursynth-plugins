===============================================
vsrawsource - Raw format reader for VapourSynth
===============================================
raw(uncompressed) video source filter for VapourSynth.
Also, YUV4MPEG2 and WindowsBitmap(24bit/32bit RGB) are supported.

Usage:
------
    >>> import vapoursynth as vs
    >>> core = vs.Core()
    >>> core.std.LoadPlugin('vsrawsource.dll')
    >>> clip = core.raws.Source('/path/to/file.raw')

    If input file has alpha channel,
    >>> clips = core.raws.Source('/path/to/file_with_alpha.raw', 1920, 1080, src_fmt='BGRA')
    >>> base = clip[0] # RGB24 clip
    >>> alpha = clip[1] # GRAY8 clip

options:
--------
    - **width**          video width (1~ default 720)
    - **height**         video height (1~ default 480)
    - **fpsnum**         framerate numerator (1~ default 30000)
    - **fpsden**         framerate denominator (1~ default 1001)
    - **sarnum**         sample aspect ratio numerator (0~ default 1)
    - **sarden**         sample aspect ratio denominator (0~ default 1)
    - **src_fmt**        color format of source video (default 'I420')
    - **off_header**     offset to the first frame data (0~ default 0)
    - **off_frame**      offset to the real data for every frame (0~ default 0)
    - **rowbytes_align** byte alignment of all rows of frame (1~16 default 1)

    these options will be ignored if source is YUV4MPEG2/WindowsBitmap.

supported color formats:
------------------------
    see format_list.txt.

Note:
-----
    When input video has alpha channel, this filter returns a list which has two clips.
    clip[0] is base clip. clip[1] is alpha clip.

How to compile:
---------------
    on unix system(include mingw/cygwin), type as follows::

    $ git clone git://github.com/chikuzen/vsrawsource.git
    $ cd ./vsrawsource
    $ ./configure
    $ make

    if you want to use msvc++, then

    - rename rawsource.c to rawsource.cpp
    - create vcxproj yourself

source code:
------------
    https://github.com/chikuzen/vsrawsource
