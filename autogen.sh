#!/bin/sh

echo "autoconf"; autoconf
echo "automake --add-missing --copy"; automake --add-missing --copy 2>/dev/null

echo "autoreconf --install imagereader/libjpeg-turbo"
autoreconf --install imagereader/libjpeg-turbo 2>/dev/null

which git 2>/dev/null >/dev/null
exitCode="$(echo $?)"
if [ $exitCode = 0 ] && [ -d ".git" ]; then
   git submodule init
   git submodule update
   echo "autoreconf --install ffms2/src"
   autoreconf --install ffms2/src 2>/dev/null
fi
