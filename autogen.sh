#!/bin/sh

type autoconf >/dev/null 2>&1 || { echo >&2 "Cannot find \`autoconf'.  Aborting."; exit 1; }
type automake >/dev/null 2>&1 || { echo >&2 "Cannot find \`automake'.  Aborting."; exit 1; }
type autoreconf >/dev/null 2>&1 || { echo >&2 "Cannot find \`autoreconf'.  Aborting."; exit 1; }

echo "autoconf"; autoconf 2>/dev/null >/dev/null
echo "automake --add-missing --copy"; automake --add-missing --copy 2>/dev/null >/dev/null

echo "autoreconf --install imagereader/libjpeg-turbo"
autoreconf --install imagereader/libjpeg-turbo 2>/dev/null >/dev/null

if type git >/dev/null 2>&1 && [ -d ".git" ]; then
   git submodule init
   git submodule update
fi
