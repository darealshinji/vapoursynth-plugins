#!/bin/sh

fftw3_minimal="$1"
compile="$2"

rm -f test test.c
cat << EOF > test.c
#include <stdio.h>
#include <fftw3.h>

int main() {
	printf("%s\n", fftw_version);
	return 0;
}
EOF

if $compile test.c -o test -lfftw3 2>/dev/null >/dev/null; then
  fftw3_version=$( ./test | sed 's/fftw//; s/sse.//; s/sse//; s/avx.//; s/avx//; s/-//g;' )

  verlte() {
    [ "$1" = "`echo -e "$1\n$2" | sort -V | head -n1`" ]
  }
  verlte $fftw3_version $fftw3_minimal && echo "yes" || echo "no"
else
  echo "no"
fi

rm -f test test.c
exit 0
