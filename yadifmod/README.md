Description
===========

Modified version of Fizick's avisynth filter port of yadif from mplayer. This version doesn't internally generate spatial predictions, but takes them from an external clip.

Ported from AviSynth plugin http://web.missouri.edu/~kes25c/


Usage
=====

    yadifmod.Yadifmod(clip clip, clip edeint, int order[, int field=order, int mode=0])

- edeint: Clip from which to take spatial predictions. This clip must be the same width, height, and colorspace as the input clip. If using same rate output, this clip should have the same number of frames as the input. If using double rate output, this clip should have twice as many frames as the input.

- order: Sets the field order.<br />
0 = bff<br />
1 = tff

- field: Controls which field to keep when using same rate output. This parameter doesn't do anything when using double rate output. When omitted, it is set equal to order.<br />
0 = keep bottom field<br />
1 = keep top field

- mode: Controls double rate vs same rate output, and whether or not the spatial interlacing check is performed.<br />
0 = same rate, do spatial check<br />
1 = double rate, do spatial check<br />
2 = same rate, no spatial check<br />
3 = double rate, no spatial check
