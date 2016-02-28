#!/bin/sh

VERSION="$(dpkg-parsechangelog -SVersion | cut -d- -f1)"

git clone --depth 1 "https://github.com/darealshinji/vapoursynth-plugins.git" tmp

for d in anime_style_art anime_style_art_rgb photo ; do
    mkdir -p models/$d
    cp tmp/models/$d/*.json models/$d
done

tar cvfJ vapoursynth-waifu2x-models_${VERSION}.orig.tar.xz models

