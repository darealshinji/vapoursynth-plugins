import vapoursynth as vs


def knlm_yuv(clip, d=[None, None, None], a=[None, None, None], s=[None, None, None], cmode=None, wmode=None,
             h=[None, None, None], rclip=None, device_type=None, info=None, planes=[0]):
    """
    Enhanced version of knlmeanscl that supports filtering of all planes on the same call and
    pads the image so borders don't be left unfiltered.

    Args:
        <Default knlmeanscl args>: See knlmeanscl for help.
        planes: Default to "0", so only luma is filtered. Set to "[0, 1, 2]" to filter
            luma and chroma.
    """

    core = vs.get_core()

    d = populate(d)
    a = populate(a)
    s = populate(s)
    h = populate(h)

    if not isinstance(planes, list):
        planes = [planes]

    packed_y = dict(d=d[0], a=a[0], s=s[0], cmode=cmode, wmode=wmode, h=h[0], rclip=rclip,
                    device_type=device_type, info=info)
    packed_u = dict(d=d[1], a=a[1], s=s[1], cmode=cmode, wmode=wmode, h=h[1], rclip=rclip,
                    device_type=device_type, info=info)
    packed_v = dict(d=d[2], a=a[2], s=s[2], cmode=cmode, wmode=wmode, h=h[2], rclip=rclip,
                    device_type=device_type, info=info)

    pad = max(a)
    ret = padding(clip, pad, pad, pad, pad)

    y = core.std.ShufflePlanes(ret, planes=[0], colorfamily=vs.GRAY)
    u = core.std.ShufflePlanes(ret, planes=[1], colorfamily=vs.GRAY)
    v = core.std.ShufflePlanes(ret, planes=[2], colorfamily=vs.GRAY)

    if 0 in planes:
        y = core.knlm.KNLMeansCL(y, **packed_y)
    if 1 in planes:
        u = core.knlm.KNLMeansCL(u, **packed_u)
    if 2 in planes:
        v = core.knlm.KNLMeansCL(v, **packed_v)

    ret = core.std.ShufflePlanes([y, u, v], planes=[0, 0, 0], colorfamily=vs.YUV)
    ret = core.std.CropRel(ret, pad, pad, pad, pad)

    return ret


def populate(var):
    """
    Converts any variable in a list of three elements.
    The previous value is copied in the subsecuent values.
    """
    if not isinstance(var, list):
        var = [var] * 3
    if len(var) == 1:
        var.append(var[0])
    if len(var) == 2:
        var.append(var[1])
    return var


def padding(src, left, top, right, bottom):
    """
    Adds specified padding to a video.
    """
    core = vs.get_core()

    w = src.width
    h = src.height

    last = core.fmtc.resample(src, w=w+left+right, h=h+top+bottom,
                              sx=-left, sy=-top, sw=w+left+right, sh=h+top+bottom,
                              kernel="point")

    if last.format.bits_per_sample != src.format.bits_per_sample:
        last = core.fmtc.bitdepth(last, bits=src.format.bits_per_sample)

    return last
