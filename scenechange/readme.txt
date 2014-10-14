
This is a scene change detection plugin for VapourSynth.
This plugin detects the scene changes of a clip and attaches _SceneChangePrev/_SceneChangeNext properties (0 or 1) to all the frames.

I think that this will make it a bit easy to develop temporal filters.

usage:

>>> import vapoursynth as vs
>>> core = vs.Core()
>>> core.std.LoadPlugin('/path/to/scenechange.dll')
>>> clip = something

>>> clip = core.scd.Detect(clip clip[, int thresh, int interval_h, int interval_v, data log])

Detect scene changes and attach _SceneChange properies to the clip.

thresh: The threshold of average of differences of the luma value between previous/next frame.
        When the average of luma differences exceeds this, it is judged with a scene change.
        (range: 1 to 254*2^(bitdepth - 8), default(or out of range) is assumed 15*2^(bitdepth-8))

interval_h: The horizontal interval of the pixel used for measurement of difference.
            (1 to width, default(or out of range) is auto adjust)

interval_v: The vertical interval of the pixel used for measurement of difference.
            (1 to height, default(or out of range) is auto adjust)

log: The name of the log file to output.
     If this is set, the frame numbers applicable to a scene change are outputted as a text.
     Describing full path is recommended.
     (default is unset)

>>> clip = core.scd.ApplyLog(clip clip, data log)

Apply _SceneChangePrev/_SceneChangeNext properties to the clip based on the log outputted by scd.Detect.


note:
suppouted color families are GRAY(8/16bits) and YUV(8/9/10/16bits).


author Oka Motofumi (chikuzen.mo at gmail dot com)
