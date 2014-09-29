%include "include/x86inc.asm"

CPU Pentium4

SECTION_RODATA

multiplier3_8bit     times 8  dw 10923 ; 0x2aab
multiplier3_16bit    times 4  dd 87381 ; 0x15555
minus128             times 16 db 128


;int prevdiff = prev - curr;
;int nextdiff = next - curr;

;if ((prevdiff < 0 && nextdiff < 0) || (prevdiff > 0 && nextdiff > 0))

;if ((prev < curr && next < curr) || (prev > curr && next > curr))


SECTION .text


; parameter:
;  r0: srcpp
;  r1: srccp
;  r2: srcnp
;  r3: dstp
;  r4: width
;  r5: height
;  r6: stride
;  r7: threshold

INIT_XMM
cglobal temporal_uint8_sse2, 8, 15, 16
   ; zeroes for unpacking
   pxor m15, m15

   ; threes, for count's initial value
   pcmpeqb m14, m14
   psrlw m14, 14
   packuswb m14, m14

   ; threshold, 16 bytes
   movq m13, r7
   punpcklbw m13, m13
   punpcklwd m13, m13
   punpckldq m13, m13
   punpcklqdq m13, m13
   paddb m13, [minus128]

   movdqa m12, [multiplier3_8bit]

   movdqa m11, [minus128]


.yloop:
   xor r8, r8
.xloop:
   movdqa m0, [r0+r8] ; prev
   movdqa m1, [r1+r8] ; curr
   movdqa m2, [r2+r8] ; next

   ; keep count in m10
   movdqa m10, m14

   ; abs(prev - curr)
   movdqa m3, m0
   movdqa m4, m1
   psubusb m3, m1
   psubusb m4, m0
   por m3, m4
   paddb m3, m11 ; subtract 128

   ; prev pixels to add to the sum
   pcmpgtb m3, m13
   movdqa m9, m3 ; save for later
   paddb m10, m3 ; subtract 1 from count
   pandn m3, m0

   ; abs (next - curr)
   movdqa m4, m1
   movdqa m5, m2
   psubusb m4, m2
   psubusb m5, m1
   por m4, m5
   paddb m4, m11 ; subtract 128

   ; next pixels to add to the sum
   pcmpgtb m4, m13
   movdqa m8, m4 ; save for later
   paddb m10, m4 ; subtract 1 from count
   pandn m4, m2

   ; unpack low half of sum
   movdqa m5, m1
   punpcklbw m5, m15

   ; unpack low half of prev pixels
   movdqa m6, m3
   punpcklbw m6, m15

   ; add prev pixels to sum
   paddw m5, m6

   ; unpack low half of next pixels
   movdqa m6, m4
   punpcklbw m6, m15

   ; add next pixels to sum
   paddw m5, m6

   ; unpack high half of sum
   movdqa m6, m1
   punpckhbw m6, m15

   ; unpack high half of prev pixels
   movdqa m7, m3
   punpckhbw m7, m15

   ; add prev pixels to sum
   paddw m6, m7

   ; unpack high half of next pixels
   movdqa m7, m4
   punpckhbw m7, m15

   ; add next pixels to sum
   paddw m6, m7

   ; multiply sum by 2
;   paddw m5, m5
;   paddw m6, m6
   psllw m5, 1
   psllw m6, 1

   ; unpack count
   movdqa m3, m10
   punpcklbw m3, m15
   punpckhbw m10, m15

   ; add count to sum
   paddw m5, m3
   paddw m6, m10

   ; free registers: m3, m4, m7, m10
   ; m0..m2 contain the original pixels, m5 and m6 contain the numbers to multiply, m8 and m9 contain the inverted masks of pixels that got added to the sum, m11..m15 contain constants

   ; calculate average for count=3
   movdqa m3, m5
   movdqa m4, m6
   pmulhw m3, m12 ; built-in right shift by 16
   pmulhw m4, m12
   packuswb m3, m4

   ; calculate average for count=2
   psrlw m5, 2
   psrlw m6, 2
   packuswb m5, m6

   ; mask for pixels with count=1
   movdqa m4, m8
   pand m4, m9

   ; mask for pixels with count=2
   pxor m8, m9

   ; mask for pixels with count=1 or count=2
   movdqa m6, m4
   por m6, m8

   ; select pixels according to count
   pand m4, m1 ; count=1
   pand m8, m5 ; count=2
   pandn m6, m3 ; count=3

   ; combine them
   por m4, m6
   por m4, m8

   ; save curr pixels for later
   movdqa m3, m1

   ; detect fluctuating pixels
   paddb m0, m11 ; subtract 128
   paddb m1, m11
   paddb m2, m11

   movdqa m5, m1
   movdqa m6, m1
   pcmpgtb m5, m0
   pcmpgtb m6, m2
   pand m5, m6 ; prev < curr && next < curr

   movdqa m6, m0
   movdqa m7, m2
   pcmpgtb m6, m1
   pcmpgtb m7, m1
   pand m6, m7 ; prev > curr && next > curr

   por m5, m6 ; mask of fluctuating pixels

   ; select fluctuating pixels
   movdqa m6, m5
   pand m6, m4 ; pixels that got averaged
   pandn m5, m3 ; original pixels
   por m5, m6

   ; write
   movdqa [r3+r8], m5

   add r8, 16
   cmp r8, r6
   jnz .xloop

.xloopdone:
   add r0, r6
   add r1, r6
   add r2, r6
   add r3, r6

   sub r5, 1
   jnz .yloop

.yloopdone:
   RET


; parameters:
;  r0: srcpp
;  r1: srccp
;  r2: srcnp
;  r3: dstp
;  r4: width
;  r5: height
;  r6: stride
;  r7: threshold

INIT_XMM
cglobal temporal_uint16_sse2, 8, 15, 16
   ; zeroes for unpacking
   pxor m15, m15

   ; threes, for count's initial value
   pcmpeqb m14, m14
   psrlw m14, 14

   ; minus 32768
   movdqa m11, m14
   psrlw m11, 1
   psllw m11, 15

   ; threshold, 8 bytes
   movq m13, r7
   punpcklwd m13, m13
   punpckldq m13, m13
   punpcklqdq m13, m13
   paddw m13, m11

   movdqa m12, [multiplier3_16bit]



.yloop:
   xor r8, r8
.xloop:
   movdqa m0, [r0+r8] ; prev
   movdqa m1, [r1+r8] ; curr
   movdqa m2, [r2+r8] ; next

   ; keep count in m10
   movdqa m10, m14

   ; abs(prev - curr)
   movdqa m3, m0
   movdqa m4, m1
   psubusw m3, m1
   psubusw m4, m0
   por m3, m4
   paddw m3, m11 ; subtract 32768

   ; prev pixels to add to the sum
   pcmpgtw m3, m13
   movdqa m9, m3 ; save for later
   paddw m10, m3 ; subtract 1 from count
   pandn m3, m0

   ; abs (next - curr)
   movdqa m4, m1
   movdqa m5, m2
   psubusw m4, m2
   psubusw m5, m1
   por m4, m5
   paddw m4, m11 ; subtract 32768

   ; next pixels to add to the sum
   pcmpgtw m4, m13
   movdqa m8, m4 ; save for later
   paddw m10, m4 ; subtract 1 from count
   pandn m4, m2

   ; unpack low half of sum
   movdqa m5, m1
   punpcklwd m5, m15

   ; unpack low half of prev pixels
   movdqa m6, m3
   punpcklwd m6, m15

   ; add prev pixels to sum
   paddd m5, m6

   ; unpack low half of next pixels
   movdqa m6, m4
   punpcklwd m6, m15

   ; add next pixels to sum
   paddd m5, m6

   ; unpack high half of sum
   movdqa m6, m1
   punpckhwd m6, m15

   ; unpack high half of prev pixels
   movdqa m7, m3
   punpckhwd m7, m15

   ; add prev pixels to sum
   paddd m6, m7

   ; unpack high half of next pixels
   movdqa m7, m4
   punpckhwd m7, m15

   ; add next pixels to sum
   paddd m6, m7

   ; multiply sum by 2
   paddd m5, m5
   paddd m6, m6

   ; unpack count
   movdqa m3, m10
   punpcklwd m3, m15
   punpckhwd m10, m15

   ; add count to sum
   paddd m5, m3
   paddd m6, m10

   ; free registers: m3, m4, m7, m10
   ; m0..m2 contain the original pixels, m5 and m6 contain the numbers to multiply, m8 and m9 contain the inverted masks of pixels that got added to the sum, m11..m15 contain constants

   movdqa m3, m5
   pmuludq m3, m12
   psrlq m3, 19
   pshufd m3, m3, 11111000b ; first dword stays, third dword moves into the second's place, fourth dword (zeroes) moves into third and fourth's places

   movdqa m4, m5
   psrlq m4, 32
   pmuludq m4, m12
   psrlq m4, 19
   psllq m4, 16
   pshufd m4, m4, 11111000b ; same

   por m3, m4 ; the low four pixels are in the low half of m3

   movdqa m4, m6
   pmuludq m4, m12
   psrlq m4, 19
   pshufd m4, m4, 10001111b ; first dword goes into third's place, third dword goes into fourth's place, fourth dword (zeroes) goes into first and second's places

   movdqa m7, m6
   psrlq m7, 32
   pmuludq m7, m12
   psrlq m7, 19
   psllq m7, 16
   pshufd m7, m7, 10001111b ; same

   por m4, m7

   por m3, m4 ; contains all the averages for count=3


   ; calculate averages for count=2
   psrld m5, 2
   psrld m6, 2

   ; m3 contains the averages for count=3
   ; m5 and m6 contain the averages for count=2
   ; m4, m7, m10 are free

   movdqa m7, m5
   movdqa m10, m6
   psllw m7, 1
   psllw m10, 1
   psrlw m7, 1
   psrlw m10, 1

   packssdw m7, m10

   psrlw m5, 15
   psrlw m6, 15

   packssdw m5, m6
   psllw m5, 15

   por m5, m7

   ; mask for pixels with count=1
   movdqa m4, m8
   pand m4, m9

   ; mask for pixels with count=2
   pxor m8, m9

   ; mask for pixels with count=1 or count=2
   movdqa m6, m4
   por m6, m8

   ; select pixels according to count
   pand m4, m1 ; count=1
   pand m8, m5 ; count=2
   pandn m6, m3 ; count=3

   ; combine them
   por m4, m6
   por m4, m8

   ; save curr pixels for later
   movdqa m3, m1

   ; detect fluctuating pixels
   paddw m0, m11 ; subtract 32768
   paddw m1, m11
   paddw m2, m11

   movdqa m5, m1
   movdqa m6, m1
   pcmpgtw m5, m0
   pcmpgtw m6, m2
   pand m5, m6 ; prev < curr && next < curr

   movdqa m6, m0
   movdqa m7, m2
   pcmpgtw m6, m1
   pcmpgtw m7, m1
   pand m6, m7 ; prev > curr && next > curr

   por m5, m6 ; mask of fluctuating pixels

   ; select fluctuating pixels
   movdqa m6, m5
   pand m6, m4 ; pixels that got averaged
   pandn m5, m3 ; original pixels
   por m5, m6

   ; write
   movdqa [r3+r8], m5

   add r8, 16
   cmp r8, r6
   jnz .xloop

.xloopdone:
   add r0, r6
   add r1, r6
   add r2, r6
   add r3, r6

   sub r5, 1
   jnz .yloop

.yloopdone:
   RET

