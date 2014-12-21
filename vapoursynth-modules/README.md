vapoursynth-modules
===================
A collection of vapoursynth modules ported from various avisynth scripts.

The functions contained in every script are as listed below:
* scoll:
  * ssaa
  * nediAA
  * naa
  * dehalo_alpha
  * yahr
  * fastlinedarken
  * contrasharpening
  * RemoveDirt
  * UnsharpMask
  * ModerateSharpen
  * SharpenBelow
  * RemoveDust

* rangeutils
  * replacerange
  * deleterange
  * freezeloop
  * blankit

* edgecleaner
  * edgecleaner

Usage and examples
------------------
You should put the modules in the "Lib/site-packages/" directory of your
python installation.

And then:
```Python
import vapoursynth as vs
import scoll
core = vs.get_core()
sc = scoll.SColl()
#...
clip = sc.ssaa(clip, th_mask=15, aamode=0, ssmode=1, sharpen=False, smask=False)
clip = sc.dehalo_alpha(src, rx=2.0, ry=2.0, darkstr=1.0, brightstr=1.0, lowsens=50, highsens=50, ss=1.5)
clip = sc.fastlinedarken(src, strength=48, luma_cap=191, threshold=4, prot=5, thinning=0)
clip = sc.contrasharpening(filtered, original)
# ...
```
```Python
import vapoursynth as vs
import edgecleaner
core = vs.get_core()
core.avs.LoadPlugin(path=r'mt_masktools-25.dll')
core.avs.LoadPlugin(path=r'aWarpSharp.dll') # aWarpSharp2 flavour
core.avs.LoadPlugin(path=r'Deen.dll') # You only need this for 'smode=2'
ec = edgecleaner.EdgeCleaner()
# ...
clip = ec.edgecleaner(src, strength=8, rep=True, rmode=17, smode=0, hot=False)
```
```Python
import vapoursynth as vs
import rangeutils
core = vs.get_core()
ru = rangeutils.RangeUtils()
# ...
clip = ru.replacerange(clip1, clip2, start, end)
clip = ru.deleterange(src, start, end)
clip = ru.freezeloop(src, start, end, loopStart, loopEnd)
clip = ru.blankit(src, start, end)
```

Troubleshooting
---------------
If it complains on loading it's most probable you need to pass "r21_arg_compat=False" on initializatio:
```python3
core = vs.get_core(r21_arg_compat=False)
```
