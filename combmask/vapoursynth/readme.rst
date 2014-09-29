====================================
CombMask - VapourSynth plugin
====================================

CombMask is a simple filter set for create comb mask and merge clips.

Both functions support 8/9/10/16bit integer planar formats.

CombMask:
--------
Create a binary(0 and maximum value) combmask clip. '_Combed' prop is set to all the frames.::

    comb.CombMask(clip clip[, int cthresh, int mthresh, int mi, int[] planes])

cthresh - spatial combing threshold. default is 6(8bit), 12(9bit), 24(10bit) or 1536(16bit).

mthresh - motion adaptive threshold. default is 9, 18, 36 or 2304.

mi - The # of combed pixels inside any of 8x16 size blocks on a plane for the frame to be detected as combed. If number of combed pixels is over this value, _Combed prop will be set to the mask as true. Value range is between 0 and 128. Default is 40.

planes - Choose which planes to process. default will process all planes. Allowed values are 0, 1, and 2.::

    0 = Y (luma) or R (red)
    1 = U (Cb)   or G (green)
    2 = V (Cr)   or B (blue)

    examples
    planes=[0]    = processes the Y or R plane only.
    planes=[1,2]  = processes the U V or G B planes only.

note: The metric of combing detection is similler to IsCombedTIVTC(metric=0) by Kevin Stone(aka. tritical).

CMaskedMerge:
-------------
An exclusive masking filter for CombMask. 

This filter can process only binary(0 and maximum value) mask, and skip merging process if the mask says '_Combed is false'.

Therefore, this filter is faster than std.MaskedMerge() if 'mask' is created by CombMask()::

    comb.CMaskedMerge(clip base, clip alt, clip mask[, int[] planes])

base - base clip.

alt - alternate clip which will be merged to base.

mask - mask clip.

planes - same as CombMask.

note: base, alt and mask must be the same format/resolution.

Examples:
---------
::

    import vapoursynth as vs
    core = vs.get_core()
    core.std.LoadPlugin('/path/to/combmask.dll')
    core.std.LoadPlugin('/path/to/vivtc.dll')
    core.std.LoadPlugin('/path/to/eedi3.dll')
    clip = something
    base = core.vivtc.VFM(clip, 1)
    alt = core.eedi3.eedi3(clip, 1)
    mask = core.comb.CombMask(base)
    
    # merge two clips
    merged = core.comb.CMaskedMerge(base, alt, mask)
    
    # replace only comed frames
    def func(n, f):
        if f.props._Combed == True:
            return 1
        return 0

    replaced = core.std.SelectClip(clips=[base, alt], src=mask, selector=func)


How to compile:
---------------
on unix like system(include mingw), type as follows::

    $ git clone git://github.com/chikuzen/CombMask.git
    $ cd ./CombMask/vapoursynth/src
    $ ./configure
    $ make install

if you want to use msvc++, then::

    - rename all *.c to *.cpp
    - create vcxproj yourself

This plugin requires SSE2 capable cpu. Thus ARM and PowerPC are unsupported.

Source code:
------------
https://github.com/chikuzen/CombMask


Author: Oka Motofumi (chikuzen.mo at gmail dot com)
