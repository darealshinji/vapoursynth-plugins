# VapourSynth-DeLogo

VS_DELOGO 005a.0.2 Copyright(C) 2003 MakKi, 2014 msg7086

VapourSynth Plugin - DeLogo (YV12 Only, delogo-005a base)

- Original plugin: delogo_avisynth 0.05a by MakKi
- All credits go to him.

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

    core.delogo.AddLogo(clip clip, string logofile, ...)
    core.delogo.EraseLogo(clip clip, string logofile, ...)

    clip                      - clip to be processed.
    logofile                  - lgd file, scanned by logoscan tool.
    logoname                  - (optional) the name of the logo. (default = first found in file)
    start / end               - (optional) beginning / end of the logo, in frames. (default = 0 / -1)
    fadein / fadeout          - (optional) fade in / out of the logo, in frames. (default = 0 /  0)
    pos_x / pos_y             - (optional) adjust logo position, in a quarter pixel. (default = 0 / 0)
    depth                     - (optional) adjust logo color depth. (default = 128)
    yc_y / yc_u / yc_v        - (optional) adjust logo color y/u/v, in 1/256. (default = 0)
    cutoff (new)              - (optional) zerofill logo pixels whose alpha all below cutoff. (default = 0)

## Example

```python
v = core.std.BlankClip(format=vs.YUV420P8, color=[40,60,240], fpsnum=30000, fpsden=1001)
v = core.delogo.eraselogo(v, r"CCAV 1440x1080.lgd", start = 5, end = 95, fadein = 10, fadeout = 10)
```

## Caution

This is a partial porting.

- Only YV12 is ported and supported.
- 64bit has not been tested yet.
- Source code is rarely changed, and some function calls are replaced by inline functions in mock object.
- You are welcome to send PR if want to improve this.

## ChangeLog

- v0.2  14/10/14 Code clean ups. Rename functions to PascalCase.
- v0.11 14/10/03 Replace some code with VSHelper's
- v0.1  14/10/03 Initial porting