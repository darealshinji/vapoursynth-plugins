**tc2cfr** is a project that provides an [Avisynth](http://www.avisynth.org) and [VapourSynth](http://www.vapoursynth.com/) plugin that can read a timecode file and convert a given video clip in to one with constant framerate by adding duplicate frames. It does the trick like what avi_tc_package does.

The precompiled binary has two dlls, one(tc2cfr.dll) is the release version, and the other(tc2cfr_debug) is the debug version, with it's pdb file together.

### Avisynth Function References

readtcv1(clip, string "timecode", int "fpsNum", int "fpsDen")

this function reads a timecode v1 file and convert a given clip to a constant-framerate one.

* timecode: the timecode v1 file to read
* fpsNum: the output fps_numerator
* fpsDen: the output fps_denominator, default value is the same as the fps_denominator of input clip

### VapourSynth Function References
tc2cfr.readtcv1(clip, data timecode, int fpsNum[, int fpsDen])

this function reads a timecode v1 file and convert a given clip to a constant-framerate one.

* timecode: the timecode v1 file to read
* fpsNum: the output fps_num
* fpsDen: the output fps_den, default value is the same as the fps_den of input clip

### How to compile
Compiling this project is a piece of pie. This project contains a VS2012 solution and a VC++ project file, so you can just simply open the solution file with VS2012(or later versions) and then build this project. No other depended libraries is required.

### TODO list
* Add support for timecode v2 file
* Port to POSIX and UNIX platforms where VapourSynth goes
* Add support for other Windows compiler such as older Visual Studio versions, and MinGW/MSYS, etc

### License
The source code, except for the Avisynth and VapourSynth header, which are under their own licenses, and the precompiled binary is available under BSD 2-clause license.