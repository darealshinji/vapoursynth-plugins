import vapoursynth as vs
import vshelpers as vsh

__version__ = '1.99'


def show_diff(clip1, clip2, damp=10, stack=True):
    """Return Clip Difference of input clips.

    Args:
        clip1 (clip): Untouched clip.
        clip2 (clip): Altered clip.
        damp (float): Strength of the atenuation for the differnece clip.
        stack (bool): If True, the four output clips will be stacked,
            else they will be interleaved.
    """
    core = vs.get_core()

    if damp >= 50 or damp <= 0:
        raise ValueError('"damp" must be greater than 0 and smaller than 50')

    medadd = ((1 << (clip1.format.bits_per_sample-1)) +
              _perc(damp, 1 << clip1.format.bits_per_sample))
    medsub = ((1 << (clip1.format.bits_per_sample-1)) -
              _perc(damp, 1 << clip1.format.bits_per_sample))

    blank = core.std.BlankClip(clip1)

    clip3 = core.hist.Luma(clip2)
    clip3 = vsh.merge_chroma(clip3, blank)

    clip4 = vsh.subtract(clip1, clip2)
    clip4 = core.std.Levels(clip4, min_in=medsub, max_in=medadd)

    clip1 = core.text.Text(clip1, "Original")
    clip2 = core.text.Text(clip2, "Changed")
    clip3 = core.text.Text(clip3, "Luma amp")
    clip4 = core.text.Text(clip4, "Difference")

    if stack is True:
        top = core.std.StackHorizontal([clip1, clip2])
        bot = core.std.StackHorizontal([clip3, clip4])
        ret = core.std.StackVertical([top, bot])
    else:
        ret = core.std.Interleave([clip1, clip2, clip3, clip4])
    return ret



def _perc(percent, whole):
    """Returns percentage's value."""
    return (percent / 100.0) * whole
