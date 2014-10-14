Description
===========

AddGrain generates film like grain or other effects (like rain) by adding random noise to a video clip. This noise may optionally be horizontally or vertically correlated to cause streaking.

Ported from AviSynth plugin http://forum.doom9.org/showthread.php?t=111849


Usage
=====

    grain.Add(clip clip[, float var=1.0, float uvar=0.0, float hcorr=0.0, float vcorr=0.0, int seed=-1, bint constant=False])

- var, uvar: The standard deviation (strength) of the luma and chroma noise, 0 is disabled.

- hcorr, vcorr: Horizontal and vertical correlation, which causes a nifty streaking effect. Range 0.0-1.0

- seed: Specifies a repeatable grain sequence. Set to at least 0 to use.

- constant: Specifies a constant grain pattern on every frame.
