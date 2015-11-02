# vsTAAmbk 0.3
A ported AA-script from Avisynth


Usage：

import vsTAAmbk as taa

aa = taa.TAAmbk(src)


For more detials NMM-HD：https://www.nmm-hd.org/newbbs/viewtopic.php?f=23&t=1666

Requirements:

VapourSynth R28 or newer

Plugins:

EEDI2						
nnedi3						
RemoveGrain/Repair			
fmtconv						
GenericFilters				
MSmoosh						
MVTools						
TemporalSoften			
sangnom

Script:

HAvsFunc r18 or newer (and its requirements)

TIPS:

Only YUV colorfmaily is supported! And input bitdepth must be 8 or 16 INTEGER !

Output is always 16bit INTEGER.

"mtype" and "mtype2" = 5 are DISABLED.
