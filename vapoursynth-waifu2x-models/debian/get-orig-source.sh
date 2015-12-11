#!/bin/sh

VERSION="$(dpkg-parsechangelog -SVersion | cut -d- -f1)"

git clone --depth 1 "https://github.com/darealshinji/vapoursynth-plugins.git" tmp

mv tmp/waifu2x-models models
rm -rf tmp

tar cvfJ vapoursynth-waifu2x-models_${VERSION}.orig.tar.xz models

