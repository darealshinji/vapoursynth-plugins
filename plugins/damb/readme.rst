Description
===========

Damb is a plugin that adds basic audio support to VapourSynth. It consists of
two filters: Read and Write.

libsndfile is used for reading and writing the audio files. To read and write
FLAC, OGG, and Vorbis, libsndfile must be compiled with support for those
formats.


Usage
=====
::

    damb.Read(clip clip, string file[, float delay=0.0])

**Read** will attach to each frame from *clip* the corresponding audio samples
from *file*.

Parameters:
    clip
        Clip to which audio will be added. The number of frames and the frame
        rate must be known.

    file
        Name of the audio file. WAV, W64, WAV with WAVEFORMATEX, FLAC, and OGG
        are supported.

    delay
        Delay applied to the audio, in seconds. If negative, the audio starts
        earlier, samples are discarded from the beginning, and silence is
        added at the end, if necessary. If positive, the audio starts later,
        silence is inserted at the beginning, and excess samples are discarded
        at the end. The duration of the clip is not changed.

::

    damb.Write(clip clip, string file[, string format, string sample_type, float quality=0.7])

**Write** takes the audio samples attached to each frame from *clip* and
writes them to *file*.

It is important to request frames from Write strictly in ascending order,
starting at 0.

Parameters:
    clip
        Clip with audio. If there is more than one type of audio in the clip,
        Write will abort at the first frame where a mismatch is detected. The
        properties attached to the first frame requested from Write will be
        used as reference (channel count, sample rate, sample type).

    file
        Name of the output audio file. If the extension is recognised, it sets
        the output format.

        Since both WAV and WAVEX formats can have the extension "wav", Write
        will output WAVEX if the input was WAVEX, otherwise WAV.

        Recognised extensions: "wav", "w64", "wavex", "flac", "ogg".

    format
        Sets the output audio format. If not specified, the output format is
        guessed from the extension, or if that fails, the output format will
        be the same as the input format.

        Possible values: same as the recognised extensions.

    sample_type
        Sets the output audio sample type. If not specified, the output sample
        type will be the same as the input sample type.

        Possible values: "u8", "s8", "s16", "s24", "s32", "float", "double".

        Only has effect on lossless output formats.

    quality
        Sets the quality level for the Vorbis compression. 0.0 is the lowest
        quality possible, while 1.0 is the highest. With libvorbis-aotuv,
        0.7 is more than sufficient for transparency.

        Only has effect for the "ogg" output format.


Compilation
===========

::

   ./autogen.sh
   ./configure
   make


License
=======

WTFPL, although binaries are LGPL because of libsndfile.

