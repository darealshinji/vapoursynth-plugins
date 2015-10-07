# sharpaamcmod.py 2015-05-15
import vapoursynth as vs
import havsfunc as haf
import vshelpers as vsh
import scoll


def sharpaamcmod(orig, dark=20, thin=10, sharp=0, smooth=0, stabilize=False, lcresize=False,
                 tradius=2, aapel=2, aaov=None, aablk=None, aatype='nnedi3'):
    # dark      = Line darkening amount, 0-256.
    # thin      = optional line thinning amount, 0-256.
    # sharp     = Postsharpening
    # smooth    = Postsmoothing
    # stabilize = Use post stabilization with Motion Compensation
    # Tradius   = 2 = MDegrain2 / 3 = MDegrain3
    # aapel     = Accuracy of the motion estimation (Value can only be 1, 2 or 4. 1 means a precision to the pixel.
    #             2 means a precision to half a pixel, 4 means a precision to quarter a pixel, produced by spatial
    #             interpolation (better but slower).)
    # aaov      = Block overlap value (horizontal). Must be even and less than block size.
    # aablk     = Size of a block (horizontal). It's either 4, 8 or 16 ( default is 8 ).
    #             Larger blocks are less sensitive to noise, are faster, but also less accurate.
    # aatype    = Use Sangnom, EEDI2 or NNEDI3 for anti-aliasing
    c = vs.get_core()

    # Vars and stuff

    w = orig.width
    h = orig.height
    if w > 1100 and aaov is None:
        aaov = 8
    else:
        aaov = 4
    if w > 1100 and aablk is None:
        aablk = 16
    else:
        aablk = 8

    shift = orig.format.bits_per_sample - 8
    multiple = 2 ** shift
    peak = (1 << orig.format.bits_per_sample) - 1

    # Cheks

    if orig.format.id != vs.YUV420P8:
        raise ValueError('Input video format should be YUV420P8.')

    # Mask

    m = vsh.get_luma(orig)

    def get_lut2(x):
        return min(round((x / multiple / 128) ** 0.86 * 255 * multiple), peak)

    m = c.std.Lut(c.std.Expr([c.std.Convolution(m, [5, 10, 5, 0, 0, 0, -5, -10, -5], divisor=4, saturate=False),
                              c.std.Convolution(m, [5, 0, -5, 10, 0, -10, 5, 0, -5], divisor=4, saturate=False)],
                             ['x y max']), function=get_lut2)

    # darkening and thining work different than in the original script because of effort

    if dark != 0 or thin != 0:
        preaa = haf.FastLineDarkenMOD(orig, strength=dark, thinning=thin)
    else:
        preaa = orig

    # Antialiasing

    if aatype == 'sangnom':
        aa = sangnomaa(preaa, lcresize=lcresize)
    elif aatype == 'eedi2':
        aa = ediaa(preaa, lcresize=lcresize)
    elif aatype == 'nnedi3':
        aa = nnedi3aa(preaa, lcresize=lcresize)
    else:
        raise ValueError('Wrong aatype, it should be "sangnom", "eedi2" or "nnedi3".')

    # Post sharpen

    if sharp == 0 and smooth == 0:
        postsh = aa
    else:
        postsh = haf.LSFmod(aa, edgemode=1, strength=sharp, overshoot=1, soft=smooth)

    # Merge results

    merged = c.std.MaskedMerge(orig, postsh, m)

    # Motion compensate AA clip

    sdiff = c.std.MakeDiff(orig, merged)

    origsuper = c.mv.Super(orig, pel=aapel)
    sdiffsuper = c.mv.Super(sdiff, pel=aapel, levels=1)

    fvec3 = c.mv.Analyse(origsuper, delta=3, isb=False, blksize=aablk, overlap=aaov)
    fvec2 = c.mv.Analyse(origsuper, delta=2, isb=False, blksize=aablk, overlap=aaov)
    fvec1 = c.mv.Analyse(origsuper, delta=1, isb=False, blksize=aablk, overlap=aaov)
    bvec1 = c.mv.Analyse(origsuper, delta=1, isb=True, blksize=aablk, overlap=aaov)
    bvec2 = c.mv.Analyse(origsuper, delta=2, isb=True, blksize=aablk, overlap=aaov)
    bvec3 = c.mv.Analyse(origsuper, delta=3, isb=True, blksize=aablk, overlap=aaov)

    if tradius > 0:
        sdd = c.mv.Degrain1(clip=sdiff, super=sdiffsuper, mvbw=bvec1, mvfw=fvec1)
    if tradius > 1:
        sdd = c.mv.Degrain2(clip=sdiff, super=sdiffsuper,
                            mvbw=bvec1, mvfw=fvec1, mvbw2=bvec2, mvfw2=fvec2)
    if tradius > 2:
        sdd = c.mv.Degrain3(clip=sdiff, super=sdiffsuper,
                            mvbw=bvec1, mvfw=fvec1, mvbw2=bvec2,
                            mvfw2=fvec2, mvbw3=bvec3, mvfw3=fvec3)

    reduct = 0.4
    tmp = c.std.Expr([sdiff, sdd], 'x 128 - abs y 128 - abs < x y ?')
    sdd = c.std.Merge(tmp, sdd, [1.0 - reduct, 0])

    return c.std.MakeDiff(orig, sdd) if stabilize is True else merged


def sangnomaa(src, lcresize):
    c = vs.get_core()

    if not isinstance(src, vs.VideoNode):
        raise ValueError('sangnomaa: This is not a clip')
    if src.format.color_family != vs.YUV or src.format.color_family != vs.GRAY and src.format.bits_per_sample != 8:
        raise ValueError('sangnomaa: Input video format should be 8 bits YUV or GRAY8.')

    if lcresize is True:
        ss = scoll.resamplehq(src, src.width*2, src.height*2)
    else:
        ss = haf.Resize(src, src.width*2, src.height*2)

    aa = c.std.Transpose(ss).sangnom.SangNomMod().std.Transpose().sangnom.SangNomMod()

    if lcresize is True:
        result = scoll.resamplehq(aa, src.width, src.height)
    else:
        result = haf.Resize(aa, src.width, src.height)

    return result


def ediaa(src, lcresize):
    c = vs.get_core()

    if not isinstance(src, vs.VideoNode):
        raise ValueError('ediaa: This is not a clip')

    aa = c.std.Transpose(src).eedi2.EEDI2(field=1).std.Transpose().eedi2.EEDI2(field=1)

    if lcresize is True:
        result = scoll.resamplehq(aa, src.width, src.height, -0.5, -0.5)
    else:
        result = haf.Resize(aa, src.width, src.height, -0.5, -0.5)

    return result


def nnedi3aa(src, lcresize):
    c = vs.get_core()

    if not isinstance(src, vs.VideoNode):
        raise ValueError('nnedi3aa: This is not a clip')

    aa = c.std.Transpose(src).nnedi3.nnedi3(field=1).std.Transpose().nnedi3.nnedi3(field=1)

    if lcresize is True:
        result = scoll.resamplehq(aa, src.width, src.height, -0.5, -0.5)
    else:
        result = haf.Resize(aa, src.width, src.height, -0.5, -0.5)

    return result
