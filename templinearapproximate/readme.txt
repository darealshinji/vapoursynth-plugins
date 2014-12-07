TempLinearApproximate VapourSynth plugin by Mystery Keeper
mystkeeper@gmail.com
https://bitbucket.org/mystery_keeper/templinearapproximate-vapoursynth

Takes several frames and for each pixel calculates linear approximation of its values through time, then returns its value in the middle (unless close to beginning or end of clip) of that line. Meant mainly for denoising aid in motion compensated clips. If you use it on plain video - you'll just get heavy ghosting.

Only works on integer and single precision float planar formats.

[USAGE]

tla.TempLinearApproximate(
	clip:clip
	radius:int:opt
	planes:int[]:opt:empty
	gamma:int:opt
	)

radius: (default 5) The size of neighborhood of the current frame. Approximation is made by (radius * 2 + 1) points, unless close to beginning or end - it is trimmed in these cases.

planes: The array of planes you want to process. If not specified - all planes are processed. Empty array is allowed for testing purposes.

gamma: Boolean. True by default. Indicates if luma values are converted from gamma corrected to linear light before processing and back after. Never applied to chroma planes. Setting this argument to False gives a speed up.

[CHANGELOG]

r3: Support for single precision float input.
Processing in linear light. On by default.

r2: Fallback to averaging when current frame is the middle in the range (it is not the middle only when close to the beginning or the end). Speedup.
License change to MIT.
Fallback to C90. Compilable with MSVC.

r1: Initial release.
