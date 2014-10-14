# VapourSynth-SangNomMod

VS_SangNomMod.dll v0.1 Copyright(C) 2013 Victor Efimov, 2014 msg7086

VapourSynth Plugin - SangNomMod (YUVP8/Y8 Only)

- Original plugin: SangNom by MarcFD
- Original plugin: SangNom2 by TurboPascal7 (Victor Efimov)
- All credits go to them.

## License

I quote the original licence here.

> This project is licensed under the MIT license. Binaries are GPL v2 because if I understand licensing stuff right (please tell me if I don't) they must be.

## Usage

    core.sangnom.SangNomMod(clip clip, int order = 1, int aa = 48, int aac = 0)

    clip    - clip to be processed. Mod-16 width and YUVP8/Y8 only.
    order   - Order of deinterlacing. (Default: 1)
    aa      - Strength of luma anti-aliasing. (Default: 48)
    aac     - Strength of chroma anti-aliasing. (Default: 0)
              Set aac to -1 to completely skip processing on chroma planes.

## Example

```python
v = core.std.BlankClip(format=vs.YUV420P8, color=[40,60,240], fpsnum=30000, fpsden=1001)
v = core.sangnom.SangNomMod(v, aac = -1)
```

## Caution

- This plugin is incompatiable with the original SangNom2 due to aac=-1, and that's why it's renamed to SangNomMod.

## ChangeLog

- v0.1  14/10/09 Initial porting
