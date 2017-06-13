import vapoursynth as vs

__version__ = '1.99'

def psharpen(clip, strength=25, threshold=75, ss_w=1.0, ss_h=1.0,
             d_width=None, d_height=None):
    """From http://forum.doom9.org/showpost.php?p=683344&postcount=28

    Sharpening function similar to LimitedSharpenFaster.

    Args:
        clip (clip): Input clip.
        strength (int): Strength of the sharpening.
        threshold (int): Controls "how much" to be sharpened.
        ss_w (float): Supersampling factor (reduce aliasing on edges).
        ss_h (float): Supersampling factor (reduce aliasing on edges).
        d_width (int): Output resolution after sharpening.
        d_height (int): Output resolution after sharpening.
    """
    core = vs.get_core()

    src = clip

    if d_width is None:
        d_width = src.width
    if d_height is None:
        d_height = src.height

    strength = _clamp(0, strength, 100) / 100.0
    threshold = _clamp(0, threshold, 100) / 100.0

    if ss_w < 1.0:
        ss_w = 1.0
    if ss_h < 1.0:
        ss_h = 1.0

    if ss_w != 1.0 or ss_h != 1.0:
        clip = core.resize.Lanczos(clip, width=_m4(src.width*ss_w), height=_m4(src.height*ss_h))

    if src.format.num_planes != 1:
        clip = core.std.ShufflePlanes(clips=clip, planes=[0],
                                      colorfamily=vs.GRAY)

    max_ = core.std.Maximum(clip)
    min_ = core.std.Minimum(clip)

    nmax = core.std.Expr([max_, min_], ['x y -'])
    nval = core.std.Expr([clip, min_], ['x y -'])

    expr0 = threshold * (1.0 - strength) / (1.0 - (1.0 - threshold) * (1.0 - strength))

    expr = ('{x} {y} / 2 * 1 - abs {expr0} < {strength} 1 = {x} {y} 2 / = 0 {y} 2 / ? '
            '{x} {y} / 2 * 1 - abs 1 {strength} - / ? {x} {y} / 2 * 1 - abs 1 {threshold} - '
            '* {threshold} + ? {x} {y} 2 / > 1 -1 ? * 1 + {y} * 2 / {scl} *').format(
                x=_interop(clip)[0],
                y=_interop(clip)[1],
                scl=_interop(clip)[2],
                expr0=expr0,
                threshold=threshold,
                strength=strength)

    nval = core.std.Expr([nval, nmax], [expr])

    clip = core.std.Expr([nval, min_], ['x y +'])

    if src.format.num_planes != 1:
        clip = core.std.ShufflePlanes(clips=[clip, src], planes=[0, 1, 2],
                                      colorfamily=src.format.color_family)

    if ss_w != 1.0 or ss_h != 1.0 or d_width != src.width or d_height != src.height:
        clip = core.resize.Lanczos(clip, width=d_width, height=d_height)

    return clip



def _interop(clip):
    bits = clip.format.bits_per_sample
    scl = (1 << bits) // 256

    x_val = 'x {} /'.format(scl) if bits != 8 else 'x'
    y_val = 'y {} /'.format(scl) if bits != 8 else 'y'

    return [x_val, y_val, scl]


def _clamp(minimum, value, maximum):
    return int(max(minimum, min(round(value), maximum)))


def _m4(value, mult=4.0):
    return 16 if value < 16 else int(round(value / mult) * mult)
