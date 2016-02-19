#!/bin/sh

src=vapoursynth-extra-plugins-src

git clone "https://github.com/darealshinji/vapoursynth-plugins.git" $src
cd $src

VERSION=$(git log -1 --format=%ci | head -c10 | sed -e 's/-//g')

./autogen.sh
rm -rf model-weights .git
cd ..

tar cvfJ vapoursynth-extra-plugins_${VERSION}.orig.tar.xz $src

