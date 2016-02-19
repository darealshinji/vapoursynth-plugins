#!/usr/bin/python3
# -*- coding: utf-8 -*-

import vapoursynth as vs

def get_scale_offsets(scaled_w, scaled_h, origin_w, origin_h,
                      offset_l, offset_t, offset_w, offset_h):
    """
Inverts scaling offsets (kind of like inverting the scaling itself, really).

Calculates the scale offsets necessary to scale back to its original form an
image that was scaled to the specified dimensions using the specified offsets.
    """
    # input parameters
    scaled = [scaled_w, scaled_h]
    origin = [origin_w, origin_h]
    scaled_off = [offset_l, offset_t, offset_w, offset_h]

    # source offsets, from lengths (w / h) to crop-style (negative values)
    for i, (org_l, off_l) in enumerate(zip(origin, scaled_off[:2]), 2):
        # If the input is positive, it represents a length, rather than a crop-
        # style offset from the bottom right. Even if the output from this code
        # is positive, however, it must always be a crop-style offset, so there
        # won't be any special treatment required.
        if scaled_off[i] > 0:
            scaled_off[i] -= org_l - off_l

    # the actual scaling of the offsets
    scales = 2 * [scale / original for original, scale in zip(origin, scaled)]
    off = [-offset * scale for offset, scale in zip(scaled_off, scales * 2)]

    # target offsets, from crop-style to lengths
    for i, (scaled_l, off_l) in enumerate(zip(scaled, off[:2]), 2):
        # The input must be a crop-style offset from the bottom right, even if
        # it happens to be positive. I could process only the positive values,
        # but because I like consistency, I decided to convert all values from
        # crop-style offsets to represent a length, regardless of whether it's
        # really necessary.
        off[i] += scaled_l - off_l

    return tuple(off)

def maskDetail(clip, final_width, final_height, RGmode=3, cutoff=None,
               gain=0.75, expandN=2, inflateN=1, blur_more=False,
               src_left=0, src_top=0, src_width=0, src_height=0,
               kernel='bilinear', invkstaps=4, taps=4, mode='normal',
               lowpasskernel='blackman', lowpassintaps=4, lowpassouttaps=3,
               lowpassthr=None, exportlowpass=False, pclevelthr=None):
    depth = clip.format.bits_per_sample
    scale = (2 ** 16 - 1) / (2 ** depth - 1)

    if cutoff is None:
        cutoff = 17990
    else:
        cutoff *= scale

    if lowpassthr is None:
        lowpassthr = 1542
    else:
        lowpassthr *= scale

    if pclevelthr is None:
        pclevelthr = 59881
    else:
        pclevelthr *= scale

    def lowpassLut16(x):
        p = x - 0x8000
        if p > 0 and p - lowpassthr > 0:
            return x - lowpassthr
        elif p <= 0 and p + lowpassthr < 0:
            return x + lowpassthr
        else:
            return 0x8000
    def luma16(x):
        x <<= 4
        value = x & 0xFFFF
        return 0xFFFF - value if x & 0x10000 else value
    def f16(x):
        if x < cutoff:
            return 0

        result = x * gain * (0x10000 + x) / 0x10000
        return min(0xFFFF, int(result))
    def pclevelLut16(x):
        return x if x > pclevelthr else 0

    core = vs.get_core()

    startclip = core.fmtc.bitdepth(clip, bits=16)

    original = (startclip.width, startclip.height)
    target = (final_width, final_height, src_left, src_top, src_width, src_height)

    if mode.startswith('lowpass'): # lowpass and lowpasspc
        twice = tuple(2 * o for o in original)
        lowpass = core.fmtc.resample(startclip, *twice, kernel=lowpasskernel,
                                     taps=lowpassintaps)
        lowpass = core.fmtc.resample(lowpass, *original, kernel=lowpasskernel,
                                     taps=lowpassouttaps)

        difflow = core.std.MakeDiff(startclip, lowpass, 0)
        if exportlowpass:
            return core.std.Lut(difflow, function=luma16)

        difflow = core.rgvs.RemoveGrain(difflow, mode=[1])
        difflow = core.std.Lut(difflow, function=lowpassLut16)

        startclip = core.std.MergeDiff(startclip, difflow, 0)

    if mode.startswith('pc') or mode.endswith('pc'): # pclevel and lowpasspc
        diff = core.std.Lut(startclip, function=pclevelLut16)
    else:
        temp = core.fmtc.resample(startclip, *target[:2], kernel=kernel,
                                  invks=True, invkstaps=invkstaps, taps=taps)
        temp = core.fmtc.resample(temp, *original, kernel=kernel, taps=taps)
        diff = core.std.MakeDiff(startclip, temp, 0)

    mask = core.std.Lut(diff, function=luma16).rgvs.RemoveGrain(mode=[RGmode])
    mask = core.std.Lut(mask, function=f16)

    for i in range(expandN):
        mask = core.generic.Maximum(mask, planes=[0])
    for i in range(inflateN):
        mask = core.generic.Inflate(mask, planes=[0])

    mask = core.fmtc.resample(mask, *target, taps=taps)
    if blur_more:
        mask = core.rgvs.RemoveGrain(mask, mode=[12,0,0])

    mask = core.std.ShufflePlanes(mask, planes=0, colorfamily=vs.GRAY)
    return core.fmtc.bitdepth(mask, bits=depth, dmode=1)
