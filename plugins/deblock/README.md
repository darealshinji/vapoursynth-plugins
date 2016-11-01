Description
===========

It does a deblocking of the picture, using the deblocking filter of h264. Ported from AviSynth plugin http://www.avisynth.nl/users/fizick/mvtools/deblock.html


Usage
=====

    deblock.Deblock(clip clip[, int quant=25, int aoffset=0, int boffset=0, int[] planes])

* clip: Clip to process. Any planar format with either integer sample type of 8-16 bit depth or float sample type of 32 bit depth is supported.

* quant: The higher the quant, the stronger the deblocking. It can range from 0 to 60.

* aoffset: Quant modifier to the blocking detector threshold. Setting it higher means than more edges will deblocked.

* boffset: Another quant modifier, for block detecting and for deblocking's strength. There again, the higher, the stronger.

* planes: A list of the planes to process. By default all planes are processed.

If `quant` + `aoffset` is inferior to 16, the filter does nothing at all. The same goes for `quant` + `boffset`.
