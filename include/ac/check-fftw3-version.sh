#!/usr/bin/env bash

CC=gcc
fftw3_minimal=${1}

db() { ( printf " db, ";for _i;do printf "%s" "$_i";done;printf "\n" ) >&2 ; }
db() { : ; }

vercmp()
{
  local a1 b1 c1 a2 b2 c2
  db "input 1 \"$1\", 2 \"$2\" " 
  v1=$1
  v2=$2
  db "v1 $v1, v2 $v2"
  set -- $( echo "$v1" | sed 's/\./ /g' )
  a1=$1 b1=$2 c1=$3
  set -- $( echo "$v2" | sed 's/\./ /g' )
  a2=$1 b2=$2 c2=$3
  db "a1,b1,c1 $a1,$b1,$c1 ; a2,b2,c2 $a2,$b2,$c2"
  ret=$(( (a1-a2)*1000000+(b1-b2)*1000+c1-c2 ))
  db "ret is $ret"
  if [ $ret -lt 0 ] ; then
    v=-1
  elif [ $ret -eq 0 ] ; then
    v=0
  else
    v=1
  fi
  printf "%d" $v
  return
}


rm -f test test.c

cat << EOF > test.c
#include <stdio.h>
#include <fftw3.h>

int main() {
	printf("%s\n", fftw_version);
	return 0;
}
EOF

$CC $CFLAGS $CPPFLAGS $LDFLAGS test.c -o test -lfftw3  2>/dev/null

fftw3_version=$( ./test | sed 's/fftw//; s/sse.//; s/sse//; s/avx.//; s/avx//; s/-//g;' )

v=$( vercmp $fftw3_version $fftw3_minimal )
if [ $v -lt 0 ] ; then
  echo "1"
else
  echo "0"
fi

rm -f test test.c

exit 0
