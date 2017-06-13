import vapoursynth as vs

__version__ = '1.99'

def resamplehq(src, width=None, height=None, kernel='spline36',
               matrix='709', tmatrix=None, src_left=0, src_top=0,
               src_width=0, src_height=0, fulls=False, dither=True):
    """Gamma correct resizing in linear light (RGB).

    Args:
        width (int): The target width.
        height (int): The target height.
        kernel (string): The kernel to use while resizing.
            Default is "spline36".
        matrix (string): The source matrix. Default is "709".
            Ignored if source colorspace is RGB.
        tmatrix (string): The target matrix. Default is "matrix" value.
        src_left (int): A sub‐pixel offset to crop the source from the left.
            Default 0.
        src_top (int): A sub‐pixel offset to crop the source from the top.
            Default 0.
        src_width (int): A sub‐pixel width to crop the source to. If negative,
            specifies offset from the right. Default is source width−src_left.
        src_height (int): A sub‐pixel height to crop the source to.
            If negative, specifies offset from the bottom.
            Default is source height − src_top.
        fulls (bool): True if input clip is full range, False otherwise (default).
        dither (bool): If True, the output is dithered to the source bitdepth
             if this was lower thant 16. If false, the output will be kept at
             the internal precision of 16 bits.
    """
    core = vs.get_core()

    # Var stuff

    clip = src

    if tmatrix is None:
        tmatrix = matrix

    if src.format.bits_per_sample != 16:
        clip = core.fmtc.bitdepth(clip=clip, bits=16)

    tid = clip.format.id

    # Convert to RGB

    if src.format.color_family != vs.RGB:
        clip = core.fmtc.resample(clip=clip, css='444')
        clip = core.fmtc.matrix(clip=clip, mat=matrix,
                                fulls=fulls, col_fam=vs.RGB)

    # Do stuff

    clip = core.fmtc.transfer(clip, transs=tmatrix, transd='linear')

    clip = core.fmtc.resample(clip, w=width, h=height, kernel=kernel,
                              sx=src_left, sy=src_top, sw=src_width, sh=src_height)

    clip = core.fmtc.transfer(clip, transs='linear', transd=tmatrix)

    # Back to original format

    if src.format.color_family != vs.RGB:
        clip = core.fmtc.matrix(clip=clip, mat=matrix, fulld=fulls,
                                col_fam=src.format.color_family)
        clip = core.fmtc.resample(clip=clip, csp=tid)

    # Dither as needed

    if dither is True and src.format.bits_per_sample != clip.format.bits_per_sample:
        clip = core.fmtc.bitdepth(clip=clip, bits=src.format.bits_per_sample)

    return clip
