Description
===========

FieldHint creates new frames by copying the top and bottom fields from
arbitrary frames chosen by the user.

This is mostly a port of the Avisynth plugin of the same name.


Usage
=====

::

    fh.FieldHint(clip clip[, string ovr, bint tff, string matches])

Parameters:
    *clip*
        Clip to mangle. Any format is supported, as long as it is constant.

    *ovr*
        Name of text file containing frame numbers. There must be one line
        for each frame in *clip*. Each line must contain two frame numbers
        separated by a comma, optionally followed by a comma and one of
        '+' or '-'. If there is a '+' or '-' character, the "_Combed" frame
        property will be attached to the corresponding frame, with the value
        1 for '+' and 0 for '-'.

    *tff*
        The clip's field order. This parameter is mandatory when *matches*
        is used.

    *matches*
        String containing the matches to be used for each frame in *clip*.
        It must have exactly the same length as the number of frames in
        *clip*. Acceptable characters are 'p', 'c', 'n', 'b', and 'u'.
        Other characters will have the same effect as 'c'. See the
        documentation for the TFM Avisynth filter (part of TIVTC) for an
        explanation of each character.

        *matches* and *ovr* cannot be used together.


Compilation
===========

::

    ./autogen.sh
    ./configure
    make
