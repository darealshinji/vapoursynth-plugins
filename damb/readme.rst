Description
===========

Damb is a plugin that adds basic audio support to VapourSynth. It consists of
two filters: Read and Write.

libsndfile is used for reading and writing the audio files.


Usage
=====
::

    damb.Read(clip clip, string file)

**Read** will attach to each frame from *clip* the corresponding audio samples from *file*.

Parameters:
    clip
        Clip to which audio will be added. The number of frames and the frame rate must be known.

    file
        Name of the audio file. WAV, W64, FLAC, and OGG are supported.

::

    damb.Write(clip clip, string file[, string format, string sample_type, float quality=0.7])

**Write** takes the audio samples attached to each frame from *clip* and writes them to *file*.

Parameters:
    clip
        Clip with audio.

    file
        Name of the output audio file. By default it will have the same format as the input file.

    format
        Overrides the input audio format. Possible values: "wav", "w64", "flac", "ogg".

    sample_type
        Overrides the input sample type. Possible values: "u8", "s8", "s16", "s24", "s32", "float", "double".

        Only has effect on lossless formats.

    quality
        Quality level for Vorbis compression.

        Only has effect for the "ogg" format.


Compilation
===========

::

   ./autogen.sh
   ./configure
   make


License
=======

WTFPL, although binaries are LGPL because of libsndfile.

