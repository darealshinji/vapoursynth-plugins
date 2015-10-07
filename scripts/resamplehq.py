import math
import vapoursynth as vs


def _LinearAndGamma(src, l2g_flag, fulls, fulld, curve, planes, gcor,
                    sigmoid, thr, cont):
    core = vs.get_core()

    if curve == 'srgb':
        c_num = 0
    elif curve in ['709', '601', '170']:
        c_num = 1
    elif curve == '240':
        c_num = 2
    elif curve == '2020':
        c_num = 3
    else:
        raise ValueError('LinearAndGamma: wrong curve value')

    if src.format.color_family == vs.GRAY:
        planes = [0]

    #                 BT-709/601
    #        sRGB     SMPTE 170M   SMPTE 240M   BT-2020
    k0 = [0.04045, 0.081,       0.0912,      0.08145][c_num]
    phi = [12.92,   4.5,         4.0,         4.5][c_num]
    alpha = [0.055,   0.099,       0.1115,      0.0993][c_num]
    gamma = [2.4,     2.22222,     2.22222,     2.22222][c_num]

    def g2l(x):
        expr = x / 65536 if fulls else (x - 4096) / 56064
        if expr <= k0:
            expr /= phi
        else:
            expr = ((expr + alpha) / (1 + alpha)) ** gamma
        if gcor != 1 and expr >= 0:
            expr **= gcor
        if sigmoid:
            x0 = 1 / (1 + math.exp(cont * thr))
            x1 = 1 / (1 + math.exp(cont * (thr - 1)))
            expr = thr - math.log(max(1 / max(expr * (x1 - x0) + x0, 0.000001) - 1, 0.000001)) / cont
        if fulld:
            return min(max(round(expr * 65536), 0), 65535)
        else:
            return min(max(round(expr * 56064 + 4096), 0), 65535)

    # E' = (E <= k0 / phi)   ?   E * phi   :   (E ^ (1 / gamma)) * (alpha + 1) - alpha
    def l2g(x):
        expr = x / 65536 if fulls else (x - 4096) / 56064
        if sigmoid:
            x0 = 1 / (1 + math.exp(cont * thr))
            x1 = 1 / (1 + math.exp(cont * (thr - 1)))
            expr = (1 / (1 + math.exp(cont * (thr - expr))) - x0) / (x1 - x0)
        if gcor != 1 and expr >= 0:
            expr **= gcor
        if expr <= k0 / phi:
            expr *= phi
        else:
            expr = expr ** (1 / gamma) * (alpha + 1) - alpha
        if fulld:
            return min(max(round(expr * 65536), 0), 65535)
        else:
            return min(max(round(expr * 56064 + 4096), 0), 65535)

    return core.std.Lut(src, planes=planes, function=l2g if l2g_flag else g2l)


def _Resize(src, w, h, sx=None, sy=None, sw=None, sh=None, kernel=None,
            taps=None, a1=None, a2=None, invks=None, invkstaps=None,
            css=None, planes=None, center=None, cplace=None, cplaces=None,
            cplaced=None, interlaced=None, interlacedd=None, tff=None,
            tffd=None, flt=None, noring=False, bits=None, fulls=None,
            fulld=None, dmode=None, ampo=None, ampn=None, dyn=None,
            staticnoise=None, patsize=None):
    core = vs.get_core()

    if not isinstance(src, vs.VideoNode):
        raise TypeError('Resize: This is not a clip')

    if bits is None:
        bits = src.format.bits_per_sample

    sr_h = w / src.width
    sr_v = h / src.height
    sr_up = max(sr_h, sr_v)
    sr_dw = 1 / min(sr_h, sr_v)
    sr = max(sr_up, sr_dw)
    assert(sr >= 1)

    # Depending on the scale ratio, we may blend or totally disable
    # the ringing cancellation
    thr = 2.5
    nrb = sr > thr
    nrf = sr < thr + 1 and noring
    if nrb:
        nrr = min(sr - thr, 1)
        nrv = round((1 - nrr) * 255)
        nrv = [nrv * 256 + nrv] * src.format.num_planes

    main = core.fmtc.resample(src, w, h, sx, sy, sw, sh, kernel=kernel,
                              taps=taps, a1=a1, a2=a2, invks=invks,
                              invkstaps=invkstaps, css=css, planes=planes,
                              center=center, cplace=cplace, cplaces=cplaces,
                              cplaced=cplaced, interlaced=interlaced,
                              interlacedd=interlacedd, tff=tff, tffd=tffd,
                              flt=flt)

    if nrf:
        nrng = core.fmtc.resample(src, w, h, sx, sy, sw, sh, kernel='gauss',
                                  taps=taps, a1=100, invks=invks,
                                  invkstaps=invkstaps, css=css, planes=planes,
                                  center=center, cplace=cplace,
                                  cplaces=cplaces, cplaced=cplaced,
                                  interlaced=interlaced,
                                  interlacedd=interlacedd, tff=tff,
                                  tffd=tffd, flt=flt)

        # To do: use a simple frame blending instead of Merge
        last = core.rgvs.Repair(main, nrng, 1)
        if nrb:
            nrm = core.std.BlankClip(main, color=nrv)
            last = core.std.MaskedMerge(main, last, nrm)
    else:
        last = main

    return core.fmtc.bitdepth(last, bits=bits, fulls=fulls, fulld=fulld,
                              dmode=dmode, ampo=ampo, ampn=ampn, dyn=dyn,
                              staticnoise=staticnoise, patsize=patsize)


def resamplehq(src, width=None, height=None, kernel='spline36',
               srcmatrix='709', dstmatrix=None, src_left=0, src_top=0,
               src_width=0, src_height=0, noring=False, sigmoid=True,
               dither=True):
    """Gamma correct resizing in linear light (RGB).

    Args:
        width (int): The target width.
        height (int): The target height.
        kernel (string): The kernel to use while resizing.
            Default is "spline36".
        srcmatrix (string): The source matrix. Default is "709".
            Ignored if source colorspace is RGB.
        dstmatrix (string): The target matrix. Default is source matrix.
        src_left (int): A sub‐pixel offset to crop the source from the left.
            Default 0.
        src_top (int): A sub‐pixel offset to crop the source from the top.
            Default 0.
        src_width (int): A sub‐pixel width to crop the source to. If negative,
            specifies offset from the right. Default is source width−src_left.
        src_height (int): A sub‐pixel height to crop the source to.
            If negative, specifies offset from the bottom.
            Default is source height − src_top.
        noring (bool): True use non-ringing algorithm in flat area scaling.
            It may produce blurring and aliasing when downscaling.
        sigmoid (bool): This can reduce the clipping of extreme halo or
            Ringing Artefacts that may develop along very sharp edges.
        dither (bool): If true, the output is dithered to the source bitdepth
             if this was lower thant 16. If false, the output will be kept at
             the internal precision of this filter of 16 bits.
    """
    core = vs.get_core()

    # Var stuff

    clip = src

    if dstmatrix is None:
        dstmatrix = srcmatrix

    if src.format.bits_per_sample != 16:
        clip = core.fmtc.bitdepth(clip=clip, bits=16)
    tid = clip.format.id

    # Convert to RGB

    if src.format.color_family != vs.RGB:
        clip = core.fmtc.resample(clip=clip, css='444')
        clip = core.fmtc.matrix(clip=clip, mat=srcmatrix, col_fam=vs.RGB)

    # Do stuff

    clip = _LinearAndGamma(clip, l2g_flag=False, fulls=True, fulld=True,
                           curve='709', planes=[0, 1, 2], gcor=1.,
                           sigmoid=sigmoid, thr=0.5, cont=6.5)
    clip = _Resize(clip, w=width, h=height, kernel=kernel, noring=noring,
                   sx=src_left, sy=src_top, sw=src_width, sh=src_height)
    clip = _LinearAndGamma(clip, l2g_flag=True, fulls=True, fulld=True,
                           curve='709', planes=[0, 1, 2], gcor=1.,
                           sigmoid=sigmoid, thr=0.5, cont=6.5)

    # Back to original format

    if src.format.color_family != vs.RGB:
        clip = core.fmtc.matrix(clip=clip, mat=dstmatrix,
                                col_fam=src.format.color_family)
        clip = core.fmtc.resample(clip=clip, csp=tid)

    # Dither as needed

    if dither is True and src.format.bits_per_sample != 16:
        clip = core.fmtc.bitdepth(clip=clip, bits=src.format.bits_per_sample)

    return clip
