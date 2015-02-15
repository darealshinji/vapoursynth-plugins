#!/bin/sh -e

for d in . fluxsmooth mvtools nnedi3 tcomb imagereader/libjpeg-turbo ; do
    mkdir -p $d/m4 $d/build-aux
done

autoreconf -ivf
autoreconf -ivf imagereader/libjpeg-turbo

./configure.sh "$@"

