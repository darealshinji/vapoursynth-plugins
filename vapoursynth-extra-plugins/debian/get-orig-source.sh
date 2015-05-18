#!/bin/sh

src=vapoursynth-extra-plugins-src

git clone "https://github.com/darealshinji/vapoursynth-plugins.git" $src
cd $src

VERSION=$(git log -1 --format=%ci | head -c10 | sed -e 's/-//g')

git submodule init
git submodule update
rm -rf ffms2/.git .git nnedi3/src/nnedi3_weights.bin
cd ..

tar cvfJ vapoursynth-extra-plugins_${VERSION}.orig.tar.xz $src

