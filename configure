#!/bin/sh

pkgname="vapoursynth-extra-plugins"

if test x"$1" = x"-h" -o x"$1" = x"--help" ; then
cat << EOF
Usage: $0 [options]

options:
  -h, --help               print this message
  --prefix=PREFIX          install files into PREFIX [/usr/local]
  --libdir=LIBDIR          install VapourSynth plugins directory
                           into LIBDIR [PREFIX/lib]
  --docdir=DOCDIR          install documents into DOCDIR
                           [PREFIX/share/doc/${pkgname}]
EOF
exit 1
fi

for opt; do
    optarg="${opt#*=}"
    case "$opt" in
        --prefix=*)
            prefix="$optarg"
            ;;
        --libdir=*)
            libdir="$optarg"
            ;;
        --docdir=*)
            docdir="$optarg"
            ;;
        *)
            ;;
    esac
done

test -n "$prefix" || prefix="/usr/local"
test -n "$libdir" || libdir="\${prefix}/lib"
test -n "$docdir" || docdir="\${prefix}/share/doc/${pkgname}"

rm -f config.mak
cat >> config.mak << EOF
prefix = $prefix
libdir = $libdir
docdir = $docdir
EOF

echo ""
cat config.mak
echo "Now run 'make && make install'"

exit 0
