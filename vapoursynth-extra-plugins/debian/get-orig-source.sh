#!/bin/sh

src=vapoursynth-extra-plugins-src

git clone "https://github.com/darealshinji/vapoursynth-plugins.git" $src
cd $src

VERSION=$(git log -1 --format=%ci | head -c10 | sed -e 's/-//g')

git submodule init
git submodule update
rm -rf ffms2/.git .git

echo '' > nnedi3/src/nnedi3_weights.bin
echo '' > waifu2x-models/anime_style_art/scale2.0x_model.json
echo '' > waifu2x-models/anime_style_art/noise2_model.json
echo '' > waifu2x-models/anime_style_art/noise1_model.json
echo '' > waifu2x-models/anime_style_art_rgb/scale2.0x_model.json
echo '' > waifu2x-models/anime_style_art_rgb/noise2_model.json
echo '' > waifu2x-models/anime_style_art_rgb/noise1_model.json
echo '' > waifu2x-models/photo/scale2.0x_model.json
echo '' > waifu2x-models/photo/noise2_model.json
echo '' > waifu2x-models/photo/noise1_model.json

cd ..

tar cvfJ vapoursynth-extra-plugins_${VERSION}.orig.tar.xz $src

