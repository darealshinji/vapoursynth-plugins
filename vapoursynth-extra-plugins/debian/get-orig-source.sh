#!/bin/sh

src=vapoursynth-extra-plugins-src

git clone "https://github.com/darealshinji/vapoursynth-plugins.git" $src
cd $src

VERSION=$(git log -1 --format=%ci | head -c10 | sed -e 's/-//g')

./autogen.sh
rm -rf models .git autom4te.cache plugins/imagereader/libjpeg-turbo/autom4te.cache
cd ..

tar cvfJ vapoursynth-extra-plugins_${VERSION}.orig.tar.xz $src

