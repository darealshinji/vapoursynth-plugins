#!/bin/sh

type autoconf >/dev/null 2>&1 || { echo >&2 "Cannot find \`autoconf'.  Aborting."; exit 1; }
type automake >/dev/null 2>&1 || { echo >&2 "Cannot find \`automake'.  Aborting."; exit 1; }
type autoreconf >/dev/null 2>&1 || { echo >&2 "Cannot find \`autoreconf'.  Aborting."; exit 1; }

mkdir -p include/build-aux

echo "autoconf"; autoconf
test -x configure || { echo >&2 "\`configure' was not generated.  Aborting."; exit 1; }

# use automake only to copy files
echo "automake --add-missing --copy"; automake --add-missing --copy 2>/dev/null >/dev/null

echo "autoreconf --install imagereader/libjpeg-turbo"
autoreconf --install imagereader/libjpeg-turbo 2>/dev/null >/dev/null

if type git >/dev/null 2>&1 && [ -d ".git" ]; then
   git submodule init
   git submodule update --depth 1
fi
