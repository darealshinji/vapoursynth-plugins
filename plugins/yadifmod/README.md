Description
===========

Modified version of Fizick's avisynth filter port of yadif from mplayer. This version doesn't internally generate spatial predictions, but takes them from an external clip.

Ported from AviSynth plugin http://web.missouri.edu/~kes25c/


Usage
=====

    yadifmod.Yadifmod(clip clip, clip edeint, int order[, int field=-1, int mode=0])

* edeint: Clip from which to take spatial predictions. This clip must be the same width, height, and colorspace as the input clip. If using same rate output, this clip should have the same number of frames as the input. If using double rate output, this clip should have twice as many frames as the input.

* order: Sets the field order. The filter will use the field order specified in the source frames and will only fall back to the specified order if not present.
  * 0 = bff
  * 1 = tff

* field: Controls which field to keep when using same rate output. This parameter doesn't do anything when using double rate output.
  * -1 = set equal to order
  * 0 = keep bottom field
  * 1 = keep top field

* mode: Controls double rate vs same rate output, and whether or not the spatial interlacing check is performed.
  * 0 = same rate, do spatial check
  * 1 = double rate, do spatial check
  * 2 = same rate, no spatial check
  * 3 = double rate, no spatial check
