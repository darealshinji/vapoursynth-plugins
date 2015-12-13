#!/bin/sh

type autoconf >/dev/null 2>&1 || { echo >&2 "Cannot find \`autoconf'.  Aborting."; exit 1; }
type automake >/dev/null 2>&1 || { echo >&2 "Cannot find \`automake'.  Aborting."; exit 1; }
type autoreconf >/dev/null 2>&1 || { echo >&2 "Cannot find \`autoreconf'.  Aborting."; exit 1; }

mkdir -p include/build-aux

echo "running autoconf"; autoconf
test -x configure || { echo >&2 "\`configure' was not generated.  Aborting."; exit 1; }

# use automake only to copy files
echo "copy needed files to include/build-aux"
automake --add-missing --copy 2>/dev/null >/dev/null

echo "running autoreconf on imagereader/libjpeg-turbo"
autoreconf --install imagereader/libjpeg-turbo 2>/dev/null >/dev/null

if [ ! -d ffms2/src ]; then
  type git >/dev/null 2>&1 || { echo >&2 "Cannot find \`git'.  Aborting."; exit 1; }
  echo "cloning ffms2 sources into ffms2/src"
  git clone -q --depth 1 "https://github.com/FFMS/ffms2.git" ffms2/src
  rm -rf ffms2/src/.git
fi

if [ ! -d lsmashsource/ffmpeg ]; then
  type git >/dev/null 2>&1 || { echo >&2 "Cannot find \`git'.  Aborting."; exit 1; }
  echo "cloning ffmpeg sources into lsmashsource/ffmpeg"
  git clone -q --depth 1 "git://source.ffmpeg.org/ffmpeg.git" lsmashsource/ffmpeg
  rm -rf lsmashsource/ffmpeg/.git
fi
