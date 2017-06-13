import vapoursynth as vs

__version__ = '1.99'

def mcdegrainsharp(clip, frames=2, bblur=0.3, csharp=0.3, bsrch=True,
                   thsad=400, plane=4):
    """Based on MCDegrain By Didee:
    http://forum.doom9.org/showthread.php?t=161594
    Also based on DiDee observations in this thread:
    http://forum.doom9.org/showthread.php?t=161580

    "Denoise with MDegrainX, do slight sharpening where motionmatch is good,
    do slight blurring where motionmatch is bad"

    In areas where MAnalyse cannot find good matches,
    the blur() will be dominant.
    In areas where good matches are found,
    the sharpen()'ed pixels will overweight the blur()'ed pixels
    when the pixel averaging is performed.

    Args:
        frames (int): Strength of the denoising (1-3).
        bblur (float): Strength of the blurring for bad motion matching areas.
        csharp (float): Strength of the sharpening for god motion match areas.
        bsrch (bool): Blur the clip for the super clip for the motion search.
        thsad (int): Soft threshold of block sum absolute differences.
            Low values can result in staggered denoising,
            large values can result in ghosting and artefacts.
        plane (int): Sets processed color plane:
                0 - luma, 1 - chroma U, 2 - chroma V, 3 - both chromas, 4 - all.
    """
    core = vs.get_core()

    if bblur > 1.58 or bblur < 0.0:
        raise ValueError('"bblur" must be between 0.0 and 1.58')
    if csharp > 1.0 or csharp < 0.0:
        raise ValueError('"csharp" must be between 0.0 and 1.0')

    blksize = 16 if clip.width > 960 else 8
    bblur = ((bblur * 2.83) / 1.58)
    csharp = ((csharp * 2.83) / 1.0)

    if plane == 3:
        planes = [1, 2]
    elif plane == 4:
        planes = [0, 1, 2]
    else:
        planes = [plane]

        clip2 = core.tcanny.TCanny(clip, sigma=bblur, mode=-1, planes=planes)

    if bsrch is True:
        super_a = core.mv.Super(clip2, pel=2, sharp=1)
    else:
        super_a = core.mv.Super(clip, pel=2, sharp=1)

    super_rend = core.mv.Super(_sharpen(clip, csharp, planes=planes),
                               pel=2, sharp=1, levels=1)

    mvbw3 = core.mv.Analyse(super_a, isb=True, delta=3,
                            overlap=blksize//2, blksize=blksize)
    mvbw2 = core.mv.Analyse(super_a, isb=True, delta=2,
                            overlap=blksize//2, blksize=blksize)
    mvbw1 = core.mv.Analyse(super_a, isb=True, delta=1,
                            overlap=blksize//2, blksize=blksize)
    mvfw1 = core.mv.Analyse(super_a, isb=False, delta=1,
                            overlap=blksize//2, blksize=blksize)
    mvfw2 = core.mv.Analyse(super_a, isb=False, delta=2,
                            overlap=blksize//2, blksize=blksize)
    mvfw3 = core.mv.Analyse(super_a, isb=False, delta=3,
                            overlap=blksize//2, blksize=blksize)

    if frames == 1:
        last = core.mv.Degrain1(clip=clip2, super=super_rend,
                                mvbw=mvbw1, mvfw=mvfw1, thsad=thsad,
                                plane=plane)
    elif frames == 2:
        last = core.mv.Degrain2(clip=clip2, super=super_rend,
                                mvbw=mvbw1, mvfw=mvfw1,
                                mvbw2=mvbw2, mvfw2=mvfw2,
                                thsad=thsad, plane=plane)
    elif frames == 3:
        last = core.mv.Degrain3(clip=clip2, super=super_rend,
                                mvbw=mvbw1, mvfw=mvfw1, mvbw2=mvbw2,
                                mvfw2=mvfw2, mvbw3=mvbw3, mvfw3=mvfw3,
                                thsad=thsad, plane=plane)
    else:
        raise ValueError('"frames" must be 1, 2 or 3.')

    return last



def _sharpen(clip, strength, planes):
    core = vs.get_core()
    blur = core.tcanny.TCanny(clip, sigma=strength, mode=-1, planes=planes)
    return core.std.Expr([clip, blur], "x x + y -")
