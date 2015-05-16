#!/bin/sh -e

for d in . fluxsmooth mvtools nnedi3 tcomb imagereader/libjpeg-turbo ; do
    mkdir -p $d/m4 $d/build-aux
done

if [ -d ".git" ] && [ "x$(type -p git)" != "x" ]; then
    git submodule init
    git submodule update
fi

./configure.sh "$@" >/dev/null
autoreconf -ivf
autoreconf -ivf imagereader/libjpeg-turbo
autoreconf -ivf ffms2

./configure.sh "$@"

