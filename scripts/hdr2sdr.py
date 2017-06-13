import vapoursynth as vs

__version__ = '1.1'

def hdr2sdr(clip, source_peak=1000, ldr_nits=100, output_foramt=vs.YUV420P8, input_range='limited', output_range='limited', dither='error_diffusion'):
    """Converts HDR footage to SDR.
    https://forum.doom9.org/showthread.php?p=1800667#post1800667

    Args:
        ldr_nits: Set at 150 or 200 to lower the brightness.
    """
    core = vs.get_core()

    clip = core.resize.Bicubic(clip, format=vs.YUV444PS, range_in_s=input_range, range_s='full', chromaloc_in_s='center', dither_type='none')
    clip = core.resize.Bicubic(clip, format=vs.RGBS, matrix_in_s='2020ncl', range_in_s='full', dither_type='none')
    clip = core.resize.Bicubic(clip, format=vs.RGBS, transfer_in_s='st2084', transfer_s='linear', dither_type='none', nominal_luminance=source_peak)

    exposure_bias = source_peak / ldr_nits

    # hable tone mapping  correct parameters
    # ["A"] = 0.22
    # ["B"] = 0.3
    # ["C"] = 0.1
    # ["D"] = 0.2
    # ["E"] = 0.01
    # ["F"] = 0.3
    # ["W"] = 11.2
    # ((x * (A*x + C*B) + D*E) / (x * (A*x+B) + D*F)) - E/F"

    tm = core.std.Expr(clip, expr='x {eb} * 0.22 x {eb} * * 0.03 + * 0.002 + x {eb} * 0.22 x {eb} * * 0.3 + * 0.06 + / 0.01 0.30 / -'.format(eb=exposure_bias), format=vs.RGBS)

    w = core.std.Expr(clip, expr='{eb} 0.22 {eb} * 0.03 + * 0.002 + {eb} 0.22 {eb} * 0.3 + * 0.06 + / 0.01 0.30 / -'.format(eb=exposure_bias), format=vs.RGBS)

    clip = core.std.Expr([tm, w], expr='x  1 y  / *', format=vs.RGBS)

    clip = core.resize.Bicubic(clip, format=vs.RGBS, primaries_in_s='2020', primaries_s='709', dither_type=dither)
    clip = core.resize.Bicubic(clip, format=vs.RGBS, transfer_in_s='linear', transfer_s='709', dither_type=dither)
    clip = core.resize.Bicubic(clip, format=output_foramt, matrix_s='709', range_in_s='full', range_s=output_range, chromaloc_in_s='center')

    return clip
