# VapourSynth-IT

VS_IT.dll v0103.1.1 Copyright(C) 2002 thejam79, 2003 minamina, 2014 msg7086

VapourSynth Plugin - Inverse Telecine (YV12 Only, IT-0051 base, IT_YV12-0103 base)

- Original plugin: IT 0.051 by thejam79
- Original plugin: IT_YV12 v0.1.03 by minamina
- Original modify: 64bit/8k mod by poodle from (avisynth64bitplugin)
- All credits go to them.
- Special thanks to: macromizer

## License 

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

## Usage

    core.it.it(clip clip, int fps = 24, int threshold = 20, int pthreshold = 75)

    clip                      - clip to be processed. Width < 8192 and YV12 only
    fps                       - 24: IVTC; 30: frame-matching only
    threshold / pthreshold    - original developer never mentioned about this
    (removed) blend = false
    (removed) ref = "TOP"
    (removed) diMode = 3

## Example

```python
v = core.std.BlankClip(format=vs.YUV420P8, color=[40,60,240], fpsnum=30000, fpsden=1001)
v = core.it.it(v)
```

## Caution

This is a partial porting.

- Only YV12 is ported and supported.
- 64bit has not been tested yet.
- Source code is rarely changed, and help function calls are used to organize variables and core calls.
- You are welcome to send PR if want to improve this.
- `blend` has been removed. No blend.
- `ref` has been removed. It will always point to "TOP".
- `diMode` has been removed. Only mode = 3 is ported, AFAIK mode 3 is working best.
- You are welcome to send PR if you think it's necessary.

This plugin has 3 code base,

- The original MMX
- Pure C routine (`-D__C`)
- SSE2 intrinsics (`-D__SSE`)

You are welcome to send PR if you can help to improve the code quality.

## ChangeLog

- v1.1  14/10/08 Move 2 thread variables back to object to improve speed. Now we support compiling under Linux using g++ and clang++. Clang version appears to be faster but YMMV.
- v1.0  14/10/06 All inline asm code has been translated to pure C and SSE2 intrinsics. SSE2 comes with 30% speed up compared to original MMX code.
- v0.4  14/10/04 Code cleanup. Special thanks to macromizer for cleaning up macros.
- v0.3  14/10/03 Code cleanup, multiple instances problem fixed
- v0.2  14/10/01 64-bit/8k mod patched
- v0.1  14/09/30 Initial porting