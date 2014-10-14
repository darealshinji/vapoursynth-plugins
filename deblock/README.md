Description
===========

It does a deblocking of the picture, using the deblocking filter of h264. Ported from AviSynth plugin http://www.avisynth.nl/users/fizick/mvtools/deblock.html


Usage
=====

    deblock.Deblock(clip clip[, int quant=25, int aoffset=0, int boffset=0, int[] planes])

- quant: the higher the quant, the stronger the deblocking. It can range from 0 to 60.

- aoffset: quant modifier to the blocking detector threshold. Setting it higher means than more edges will deblocked.

- boffset: another quant modifier, for block detecting and for deblocking's strength. There again, the higher, the stronger.

- planes: a list of the planes to process. By default all planes are processed.

If quant + aoffset is inferior to 16, the filter does nothing at all. The same goes for quant + boffset.
