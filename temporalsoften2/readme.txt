temporalsoften2 is a sample which uses scenechange plugin together.

usage:

    focus2.TemporalSoften2(clip clip[, int radius, int luma_threshold,
                                     int chroma_threshold, bint scenechange,
                                     int mode])

    radius: 1 to 7 (default is 4)
    luma_threshold: 0 to 255. If clip is RGB, all planes use this value.
                     (default is 4)
    chroma_threshold: 0 to 255. If clip is RGB or Gray, this value is ignored.
                     (default is 8)
    scenechange: 0 means never reference _SceneChange property. (default is 1)
    mode: 2

example:

>>> import vapoursynth as vs
>>> core = vs.Core()
>>> core.std.LoadPlugin('/path/to/scenechange.dll')
>>> core.std.LoadPlugin('/path/to/temporalsoften2.dll')

>>> clip = (YUV or Gray clip)
>>> clip = core.scd.Detect(clip, thresh=20)
>>> clip = core.focus2.TemporalSoften2(clip)

>>> clip = (RGB clip)
>>> def copy_sc(n, f):
...     fout = f[0].copy()
...     fout.props._SceneChange = f[1].props._SceneChange[0]
...     return fout
...
>>> tmp = core.resize.Point(clip, format=vs.GRAY8)
>>> tmp = core.scd.Detect(tmp, thresh=20)
>>> clip = core.std.ModifyFrame([clip, tmp], copy_sc)
>>> clip = core.focus2.TemporalSoften2(clip)


if you don't want to write complecated script, use temporalsoften2.py .
