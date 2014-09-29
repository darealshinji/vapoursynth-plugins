CombMask - Combmask create filter for Avisynth2.6x


description:
    CombMask is a simple filter that creates a comb mask that can (could) be
    used by other filters like MaskTools2.
    The mask consists of binaries of 0(not combed) and 255(combed).

    MaskedMerge is an exclusive masking filter for CombMask. This is often faster
    than MaskTools2's mt_merge().

    IsCombed is a is a utility function that can be used within AviSynth's
    conditionalfilter to test whether or not a frame is combed and returns true
    if it is and false if it isn't.

    These filters are written from scratch, but most of logics are come from
    tritical's TIVTC plugin.


syntax:
    CombMask(clip, int cthresh, int mthresh, bool chroma, bool sse2)

        cthresh(0 to 255, default is 6):
            spatial combing threshold.

        mthresh(0 to 255, default is 9):
            motion adaptive threshold.

        chroma(default is true):
            Whether processing is performed to UV planes or not.

        sse2(default true):
            enable SSE2 intrinsic code(faster).


    MaskedMerge(clip base, clip alt, clip mask, int MI, bool chroma, bool sse2)

        base: base clip.

        alt: alternate clip which will be merged to base.

        mask: mask clip.

        MI(0 to 128, default is 40):
            The # of combed pixels inside any of 8x16 size blocks on the Y-plane
            for the frame to be detected as combed.
            if the frame is not combed, merge process will be skipped.

        chroma(default is true):
            Whether processing is performed to UV planes or not.

        sse2(default true):
            same as CombMask.


    IsCombed(clip, int cthresh, int mthresh,int MI, bool sse2)

        cthresh: Same as CombMask.

        mthresh: Same as CombMask.

        MI: Same as MaskedMerge.

        sse2: same as CombMask.


usage:

    LoadPlugin("CombMask.dll)
    src = SourceFilter("foo\bar\fizz\buzz")
    deint = src.some_deinterlace_filter()
    deint2 = src.another_filter()
    mask = deint.CombMask()
    last = deint.MaskedMerge(deint2, mask)
    return last


    LoadPlugin("CombMask.dll")
    src = a_YV12_clip
    combed = src.ConvertToYV16(interlaced=true)
    nocomb = src.ConvertToYV16(interlaced=false)
    ConditionalFilter(src, combed, nocomb, "IsCombed", "=", "true")


reqirement:

    - Avisynth2.6alpha4 or later
    - WindowsXPsp3 / Vista / 7 / 8
    - Microsoft Visual C++ 2010 Redistributable Package
    - SSE2 capable CPU


author:
    Oka Motofumi (chikuzen.mo at gmail dot com)
