import traceback
import vapoursynth as vs

__version__ = '1.99'


def i_m():
    """Returns the name of the function within it is called."""
    return traceback.extract_stack(None, 2)[0][2]



def clamp(minimum, number, maximum):
    """Clamps "x" bettween "minimum" and "maximum"."""
    return int(max(minimum, min(round(number), maximum)))



def m4(number, multiple=4.0):
    """Returns a number multiple of "multiple", by default 4."""
    return 16 if number < 16 else int(round(number / multiple) * multiple)



def build_mask(clip, edgelvl=40, edgelvl2=60, mode=0):
    """This function builds a mask that trys to catch the edges of the image.

    Args:
        edgelvl: Threshold of the mask (range 0 - 255).
        edgelvl2: High threshold of the mask (range 0 - 255), only used in mode 2.
        mode:   0 uses Prewitt to build the mask.
                1 uses custom Convolution.
                2 uses hystesis.
    """
    core = vs.get_core()

    if mode < 0 or mode > 2:
        raise ValueError(
            i_m() + ': mode should be an int between 0 and 2.')

    if edgelvl < 0 or edgelvl > 255:
        raise ValueError(
            i_m() + ': edgelvl should be an int between 0 and 255.')

    edgelvl = edgelvl << clip.format.bits_per_sample-8

    if clip.format.id != vs.GRAY:
        clip = core.std.ShufflePlanes(clip, planes=[0], colorfamily=vs.GRAY)

    if mode == 0:
        mask = core.std.Prewitt(clip, _min=edgelvl, _max=edgelvl)
        mask = core.std.Maximum(mask)
    elif mode == 1:
        mask = core.std.Convolution(clip, matrix=[1, 2, 1, 0, 0, 0, -1, -2, -1])
        mask = core.std.Binarize(mask)
        mask = core.std.Maximum(mask)
    elif mode == 2:
        mask = core.std.Prewitt(clip, _min=edgelvl, _max=edgelvl)
        clip_b = core.std.Binarize(mask, edgelvl)
        clip_a = core.std.Binarize(mask, edgelvl2)  # FIXME check if this actually works.
        mask = core.generic.Hysteresis(clip_b, clip_a)
        mask = core.std.Maximum(mask)

    return mask



def logic(clipa, clipb, mode, th1=0, th2=0, planes=0):
    """Produces a new mask which is the result of a binary operation between
    two masks. The operation is chosen with the parameter mode.

    Args:
        mode (string): See the list at the end for more details.
            Default mode is "and".
        th1 (int): Threshold 1 used in some modes.
        th2 (int): Threshold 2 used in some modes.
        planes (list, int): Which planes will be processed,
            by default only luma will be processed ([0]).

    List of modes and their description:

    .. code-block:: none

        and:
            Works only with binary masks.
            The output mask is the intersection of the two masks.
            It means that if both corresponding pixels are 255,
            the resulting pixel will be 255, else 0.

        or:
            Works only with binary masks.
            The output mask is the union of the two masks.
            It means that if one of the corresponding pixels are 255,
            the resulting pixel will be 255, else 0.

        xor:
            Works only with binary masks.
            The output mask is the difference between the two masks.
            It means that if one ( exclusively ) of the corresponding
            pixels are 255, the resulting pixel will be 255, else 0.

        andn:
            Works only with binary masks.
            The output mask is the subtraction of the second mask from the
            first one. It means that if the pixel of the first mask is 255
            and the second is 0, it will return 255, else 0.

        min:
            Returns for each pixel the minimum value between the two pixels
            of the input masks. It amounts to mode="and", but for non binary
            masks.

        max:
            Returns for each pixel the maximum value between the two pixels of
            the input masks. It amounts to mode="or", but for non binary masks.

    Note:
        If a logical operator is used with a non binary mask,
        the results are unpredictable.
    """
    core = vs.get_core()
    mode = mode.lower()
    if mode == 'and':
        expr = 'x y and'
    elif mode == 'andn':
        expr = 'x y not and'
    elif mode == 'or':
        expr = 'x y or'
    elif mode == 'xor':
        expr = 'x y xor'
    elif mode == 'min':
        expr = 'x {th1} + y {th2} + min'.format(th1=th1, th2=th2)
    elif mode == 'max':
        expr = 'x {th1} + y {th2} + max'.format(th1=th1, th2=th2)
    else:
        raise ValueError(
            i_m() + (': "{}" is not a valid mode for logic '
                     '(and, andn, or, xor, min, max)').format(mode))
    expr = [expr if 0 in planes else '',
            expr if 1 in planes else '',
            expr if 2 in planes else '']

    return core.std.Expr([clipa, clipb], expr=expr)



def css_value(clip):
    """Returns a string with chroma subsampling
    in fmtconv format ("444", "422", "422").
    """
    ssh = clip.format.subsampling_h
    ssw = clip.format.subsampling_w

    if ssh == 0 and ssw == 0:
        cssv = "444"
    elif ssh == 0 and ssw == 1:
        cssv = "422"
    elif ssh == 1 and ssw == 1:
        cssv = "420"
    elif ssh == 1 and ssw == 0:
        cssv = "411"

    return cssv



def get_plane(clip, plane=0):
    """Returns a GRAY clip containing the specified plane."""
    core = vs.get_core()
    return core.std.ShufflePlanes(clip, planes=plane, colorfamily=vs.GRAY)



def get_luma(clip):
    """Returns a GRAY clip containing the luma from input clip."""
    core = vs.get_core()

    return core.std.ShufflePlanes(clips=clip, planes=[0], colorfamily=vs.GRAY)



def merge_chroma(clip1, clip2):
    """Given clip1 and clip2, returns a clip with the luma from clip1
    and chroma from clip2.

    Args:
        clip1: Clip from the one the luma plane is taken.
        clip2: Clip from the one the chroma planes and color family are taken.
    """
    core = vs.get_core()
    return core.std.ShufflePlanes(clips=[clip1, clip2], planes=[0, 1, 2],
                                  colorfamily=clip2.format.color_family)



def fit(clipa, clipb):
    """This function fits clipb within the dimensions of clipa.
    It is aligned to the top-left and it crops/addborders as needed.

    When borders are added they are pure white.
    """
    core = vs.get_core()

    max_ = (1 << clipb.format.bits_per_sample) - 1
    mid = 1 << (clipb.format.bits_per_sample - 1)

    if clipb.format.num_planes > 1:
        if clipb.format.color_family == vs.RGB:
            color = [max_, max_, max_]
        else:
            color = [max_, mid, mid]
    else:
        color = [max_]

    if clipa.width > clipb.width:
        clipb = core.std.AddBorders(clip=clipb,
                                    left=0, right=clipa.width - clipb.width,
                                    color=color)
    elif clipa.width < clipb.width:
        clipb = core.std.CropRel(clip=clipb,
                                 left=0, right=clipb.width - clipa.width)

    if clipa.height > clipb.height:
        clipb = core.std.AddBorders(clip=clipb,
                                    top=0, bottom=clipa.height - clipb.height,
                                    color=color)
    elif clipa.height < clipb.height:
        clipb = core.std.CropRel(clip=clipb,
                                 top=0, bottom=clipb.height - clipa.height)

    return clipb



def move(clips, x, y):
    """It "moves" an image relative to its center without modifing its size.

    Args:
        x: pixels to move within the x axis.
        y: pixels to move within the y axis.
    """
    core = vs.get_core()

    moved = None

    for clip in clips:
        if clip.format.num_planes == 1:
            color = [(2 ** clip.format.bits_per_sample) - 1]
        else:
            color = None

        if x != 0 or y != 0:
            if x >= 0:
                right = 0
                left = x
            else:
                right = abs(x)
                left = 0
            if y >= 0:
                top = 0
                bottom = y
            else:
                top = abs(y)
                bottom = 0

            clip = core.std.AddBorders(clip=clip, left=left, right=right,
                                       top=top, bottom=bottom, color=color)
            clip = core.std.CropRel(clip=clip, left=right, right=left,
                                    top=bottom, bottom=top)

        if clip is isinstance(list()):
            moved.append(clip)
        else:
            moved = clip

    return moved



def get_decoder():
    """It returns an usable decoder prefering lsmash over ffms2."""
    core = vs.get_core()

    try:
        core.lsmas
    except NameError:
        try:
            core.ffms2
        except NameError:
            raise NameError(
                i_m() + ': No suitable source filter was found, '
                        'please, install either ffms2 or lsmas.')
        else:
            vsource = core.ffms2.Source
    else:
        vsource = core.lsmas.LWLibavSource

    return vsource



def subtract(clip1, clip2, planes=None):
    """Shows de differences between clip1 and clip2."""
    core = vs.get_core()

    if planes is None:
        planes = [0, 1, 2]

    luma = 1 << (clip1.format.bits_per_sample - 1)

    expr = ('{luma} x + y -').format(luma=luma)
    expr = [(i in planes) * expr for i in range(clip1.format.num_planes)]

    return core.std.Expr([clip1, clip2], expr)



def invert(src, planes=None):
    """Inverts one or several color channels of a clip."""
    core = vs.get_core()

    if planes is None:
        planes = [0, 1, 2]

    max_ = (1 << src.format.bits_per_sample) - 1

    expr = '{} x -'.format(max_)
    expr = [expr if 0 in planes else '',
            expr if 1 in planes else '',
            expr if 2 in planes else '']
    inv = core.std.Expr(clips=[src], expr=expr)

    return inv



def multi_inflate(src, iterations):
    """Applies Inflate to a clip multiple times."""
    core = vs.get_core()
    inflated = src
    for i in range(iterations):
        inflated = core.std.Inflate(inflated)

    return inflated



def scale_bd(value, nbits=16, obits=8):
    """It translates a value for one bitdepth to another one."""
    oldrange = (1 << obits) - 1
    newrange = (1 << nbits) - 1

    return int(value * newrange / oldrange)



def padding(src, left, top, right, bottom):
    """It applyes the specified padding to the image.

    TODO: Make it use internall resizers instead of fmtconv
    """
    core = vs.get_core()

    last = core.fmtc.resample(src,
                              w=src.width + left+right,
                              h=src.height + top+bottom,
                              sx=-left,
                              sy=-top,
                              sw=src.width + left+right,
                              sh=src.height + top+bottom,
                              kernel="point")

    if last.format.bits_per_sample != src.format.bits_per_sample:
        last = core.fmtc.bitdepth(last, bits=src.format.bits_per_sample)

    return last



def populate_list(var):
    """Converts any variable in a list of three elements.
    The previous value is copied in the subsecuent values.
    """
    if not isinstance(var, list):
        var = [var] * 3
    if len(var) == 1:
        var.append(var[0])
    if len(var) == 2:
        var.append(var[1])

    return var
