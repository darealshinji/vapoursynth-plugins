import vapoursynth as vs
import havsfunc as haf

__version__ = '1.99'

def sharpaamcmod(src, dark=20, thin=10, sharp=0, smooth=0, stabilize=False,
                 tradius=2, aapel=2, aaov=None, aablk=None, aatype='sangnom'):
    """Ported from: http://forum.doom9.org/showthread.php?p=1673928

    Args:
        dark (int): Line darkening amount, 0-256.
        thin (int): optional line thinning amount, 0-256.
        sharp (int): Postsharpening
        smooth (int): Postsmoothing
        stabilize (bool): Use post stabilization with Motion Compensation
        Tradius (int): Temporal radius for Mdegrain (1, 2 or 3)
        aapel (int): Accuracy of the motion estimation. Value can only be 1, 2 or 4.
                    1 means a precision to the pixel. 2 means a precision to half a pixel,
                    4 means a precision to quarter a pixel, produced by spatial interpolation
                    (better but slower).
        aaov (int): Block overlap value (horizontal). Must be even and less than block size.
        aablk (int): Size of a block (horizontal). It's either 4, 8 or 16 ( default is 8 ).
                    Larger blocks are less sensitive to noise, are faster, but also less accurate.
        aatype (sting): Use sangnom or eedi2 for anti-aliasing
    """
    core = vs.get_core()

    # Vars and stuff

    if src.width > 1100 and aaov is None:
        aaov = 8
    else:
        aaov = 4

    if src.width > 1100 and aablk is None:
        aablk = 16
    else:
        aablk = 8

    _max = (1 << src.format.bits_per_sample) - 1
    _mid = (1 << src.format.bits_per_sample) / 2

    aatype = aatype.lower()

    # Mask

    mask = core.std.ShufflePlanes(src, planes=0, colorfamily=vs.GRAY)

    mask = core.std.Expr(core.std.Expr([core.std.Convolution(mask, [5, 10, 5, 0, 0, 0, -5, -10, -5],
                                                             divisor=4, saturate=False),
                                        core.std.Convolution(mask, [5, 0, -5, 10, 0, -10, 5, 0, -5],
                                                             divisor=4, saturate=False)],
                                       ['x y max']),
                         ['x {_mid} / 0.86 pow {_max} *'.format(_max=_max, _mid=_mid)])

    # darkening and thining work different than in the original script because of effort

    if dark != 0 or thin != 0:
        preaa = haf.FastLineDarkenMOD(src, strength=dark, thinning=thin)
    else:
        preaa = src

    # Antialiasing

    if aatype == 'sangnom':
        aa_clip = _sangnomaa(preaa)
    elif aatype == 'eedi2':
        aa_clip = _ediaa(preaa)
    else:
        raise ValueError('Wrong aatype, it should be "sangnom" or "eedi2".')

    # Post sharpen

    if sharp == 0 and smooth == 0:
        postsh = aa_clip
    else:
        postsh = haf.LSFmod(aa_clip, edgemode=1, strength=sharp, overshoot=1, soft=smooth)

    # Merge results

    merged = core.std.MaskedMerge(src, postsh, mask, planes=0)

    # Motion compensate AA clip

    sdiff = core.std.MakeDiff(src, merged)

    srcsuper = core.mv.Super(src, pel=aapel)
    sdiffsuper = core.mv.Super(sdiff, pel=aapel, levels=1)

    fvec3 = core.mv.Analyse(srcsuper, delta=3, isb=False, blksize=aablk, overlap=aaov)
    fvec2 = core.mv.Analyse(srcsuper, delta=2, isb=False, blksize=aablk, overlap=aaov)
    fvec1 = core.mv.Analyse(srcsuper, delta=1, isb=False, blksize=aablk, overlap=aaov)
    bvec1 = core.mv.Analyse(srcsuper, delta=1, isb=True, blksize=aablk, overlap=aaov)
    bvec2 = core.mv.Analyse(srcsuper, delta=2, isb=True, blksize=aablk, overlap=aaov)
    bvec3 = core.mv.Analyse(srcsuper, delta=3, isb=True, blksize=aablk, overlap=aaov)

    if tradius > 0:
        sdd = core.mv.Degrain1(clip=sdiff, super=sdiffsuper, mvbw=bvec1, mvfw=fvec1)
    if tradius > 1:
        sdd = core.mv.Degrain2(clip=sdiff, super=sdiffsuper,
                               mvbw=bvec1, mvfw=fvec1, mvbw2=bvec2, mvfw2=fvec2)
    if tradius > 2:
        sdd = core.mv.Degrain3(clip=sdiff, super=sdiffsuper,
                               mvbw=bvec1, mvfw=fvec1, mvbw2=bvec2,
                               mvfw2=fvec2, mvbw3=bvec3, mvfw3=fvec3)

    reduct = 0.4
    tmp = core.std.Expr([sdiff, sdd], 'x {_mid} - abs y {_mid} - abs < x y ?'.format(_mid=_mid))
    sdd = core.std.Merge(tmp, sdd, [1.0 - reduct, 0])

    return core.std.MakeDiff(src, sdd) if stabilize is True else merged



def _sangnomaa(src):
    core = vs.get_core()

    if not isinstance(src, vs.VideoNode):
        raise ValueError('sangnomaa: This is not a clip')

    ss_clip = core.resize.Spline36(src, src.width*2, src.height*2)
    aa_clip = core.std.Transpose(ss_clip).sangnom.SangNom().std.Transpose().sangnom.SangNom()
    result = core.resize.Spline36(aa_clip, src.width, src.height)

    return result


def _ediaa(src):
    core = vs.get_core()

    if not isinstance(src, vs.VideoNode):
        raise ValueError('ediaa: This is not a clip')

    aa_clip = core.std.Transpose(src).eedi2.EEDI2(field=1).std.Transpose().eedi2.EEDI2(field=1)
    result = core.resize.Spline36(aa_clip, src.width, src.height, src_left=-0.5, src_top=-0.5)

    return result
