#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>

#undef int128_t
#define int128_t our_int128_t
#undef uint128_t
#define uint128_t our_uint128_t

#if HAVE___INT128
typedef __int128                int128_t;
typedef unsigned __int128       uint128_t;
#elif HAVE_INT128
typedef int128                  int128_t;
typedef unsigned int128         uint128_t;
#else /* HAVE__INT128_T */
typedef __int128_t              int128_t;
typedef __uint128_t             uint128_t;
#endif

#define UINT128_MAX             ((uint128_t)-1)

// For assembling the fonts, MSB first, LSB last

// Old Macro

#define W(B, S) B##U << S
#define G(AA, AB, AC, AD, BA, BB, BC, BD, CA, CB, CC, CD, DA, DB, DC, DD, EA, \
          EB, EC, ED, FA, FB, FC, FD, GA, GB, GC, GD, HA, HB, HC, HD)         \
  (W(AA, 000) | W(AB, 001) | W(AC, 002) | W(AD, 003) | W(BA, 004) |           \
   W(BB, 005) | W(BC, 006) | W(BD, 007) | W(CA, 010) | W(CB, 011) |           \
   W(CC, 012) | W(CD, 013) | W(DA, 014) | W(DB, 015) | W(DC, 016) |           \
   W(DD, 017) | W(EA, 020) | W(EB, 021) | W(EC, 022) | W(ED, 023) |           \
   W(FA, 024) | W(FB, 025) | W(FC, 026) | W(FD, 027) | W(GA, 030) |           \
   W(GB, 031) | W(GC, 032) | W(GD, 033) | W(HA, 034) | W(HB, 035) |           \
   W(HC, 036) | W(HD, 037))

// New Macros

#include "binary.h"

// on 32 bit, swap 16 bits : ABCD EFGH -> EFGH ABCD
#define R16(B) ((((B) & 0xffff0000) >> 16) | (((B) & 0x0000ffff) << 16))
// AB00EF00 -> 00AB00EF |  00CD00GH -> CD00GH00
// So EFGH ABCD -> GHEF CDAB
#define R8(B) ((((B) & 0xff00ff00) >> 8 ) | (((B) & 0x00ff00ff) << 8))
// A0C0E0G0 -> 0A0C0E0G | 0B0D0F0H -> B0D0F0H 
// So GHEF CDAB -> HGFE DCBA
#define R4(B) ((((B) & 0xf0f0f0f0) >> 4 ) | ((B & 0x0f0f0f0f) << 4))
// Then at byte level, switch pack of 2
#define R2(B) ((((B) & 0xcccccccc) >> 2 ) | ((B & 0x33333333) << 2))
// Then at the bit level, switch invididual bits
#define R1(B) ((((B) & 0xaaaaaaaa) >> 1 ) | ((B & 0x55555555) << 1))

// on 128 bit, swap 64 bits : ABCD EFGH -> EFGH ABCD
#define R64(B) ((((B) & 0xffffffffffffffff0000000000000000) >> 64) | (((B) & 0x0000000000000000ffffffffffffffff) << 64))
// on 128 bit, swap 32 bits : ABCD EFGH -> EFGH ABCD
#define R32(B) ((((B) & 0xffffffff00000000) >> 32) | (((B) & 0x00000000ffffffff) << 32))

// For 4x8=32 with B
#define B32(A,B,C,D,E,F,G,H) ( \
  ((uint32_t)(A)<<28) \
+ ((uint32_t)(B)<<24) \
+ ((uint32_t)(C)<<20) \
+ ((uint32_t)(D)<<16) \
+ ((uint32_t)(E)<<12) \
+ ((uint32_t)(F)<<8) \
+ ((uint32_t)(G)<<4) \
+            (H))

// For 8x16=128 with BB
#define BB128( \
                A,B,C,D,E,F,G,H \
                , \
                I,J,K,L,M,N,O,P \
                ) ( \
  ((uint128_t)(A)<<120) \
+ ((uint128_t)(B)<<112) \
+ ((uint128_t)(C)<<104) \
+ ((uint128_t)(D)<<96) \
+ ((uint128_t)(E)<<88) \
+ ((uint128_t)(F)<<80) \
+ ((uint128_t)(G)<<72) \
+ ((uint128_t)(H)<<64) \
+ (( uint64_t)(I)<<56) \
+ (( uint64_t)(J)<<48) \
+ (( uint64_t)(K)<<40) \
+ (( uint64_t)(L)<<32) \
+ (( uint64_t)(M)<<24) \
+ (( uint64_t)(N)<<16) \
+ (( uint64_t)(O)<<8) \
+ (  uint64_t)(P))

// To reverse all the bits in 32 bits
#define R(B) R1(R2(R4(R8(R16(B)))))
// To reverse all the bits in 128 bit
#define RA(BB) R1(R2(R4(R8(R16(R32(R64(BB)))))))
// To partially reverse bits in 128 bit
#define RR(BB) R32(R64(BB))

// To read fonts like Justine
#define RB32(A,B,C,D,E,F,G,H) R(B32(A,B,C,D,E,F,G,H))
#define RRBB128(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P) RR(BB128(R(A),R(B),R(C),R(D),R(E),R(F),R(G),R(H),R(I),R(J),R(K),R(L),R(M),R(N),R(O),R(P)))

// For debugging with a byte
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0') 

// For 32bits, use masks like below
// m=32bitnumber;
// printf("R1(R2(R4(R8(R16("BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(m>>24), BYTE_TO_BINARY(m>>16), BYTE_TO_BINARY(m>>8), BYTE_TO_BINARY(m));

uint32_t oldfull32= 
    G(0,0,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,0,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1);

uint32_t roldfull32= 
    G(1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      0,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,0,0);


uint32_t newfull32 = 
  B32(B0011,
      B1111,
      B1111,
      B1110,
      B1111,
      B1111,
      B1111,
      B1111);

uint128_t full128 = 
BB128(BB11111111,
      BB11111111,
      BB11111111,
      BB11111111,
      BB11111111,
      BB11111111,
      BB11111000,
      BB11111100,
      BB11111111,
      BB11111111,
      BB11111110,
      BB11111111,
      BB11111100,
      BB11111111,
      BB11111111,
      BB11111000);

int main() {
  int32_t m;
  // Check the new macro works
  m=oldfull32;
  printf ("oldfull32 string b0011111111111110 1111111111111111\n");
  printf ("oldfull32: %x\n", oldfull32);
  printf("in oldfull32:  "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(m>>24), BYTE_TO_BINARY(m>>16), BYTE_TO_BINARY(m>>8), BYTE_TO_BINARY(m));
  m= newfull32;
printf("in newfull32:  "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(m>>24), BYTE_TO_BINARY(m>>16), BYTE_TO_BINARY(m>>8), BYTE_TO_BINARY(m));
m=R16(newfull32);
printf("R16(newfull32: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(m>>24), BYTE_TO_BINARY(m>>16), BYTE_TO_BINARY(m>>8), BYTE_TO_BINARY(m));
m=R8(R16(newfull32));
printf("R8(R16(full32: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(m>>24), BYTE_TO_BINARY(m>>16), BYTE_TO_BINARY(m>>8), BYTE_TO_BINARY(m));
m=R4(R8(R16(newfull32)));
printf("R4(R8(R16(l32: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(m>>24), BYTE_TO_BINARY(m>>16), BYTE_TO_BINARY(m>>8), BYTE_TO_BINARY(m));
printf("R2(R4(R8(R16(: "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(m>>24), BYTE_TO_BINARY(m>>16), BYTE_TO_BINARY(m>>8), BYTE_TO_BINARY(m));
m=R1(R2(R4(R8(R16(newfull32)))));
printf("R1(2(R4(R8(R16("BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN" "BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(m>>24), BYTE_TO_BINARY(m>>16), BYTE_TO_BINARY(m>>8), BYTE_TO_BINARY(m));

  printf ("oldfull32: %x\n", oldfull32);
  printf ("R1R2R4R8R16(newfull32): %x\n", R1(R2(R4(R8(R16(newfull32))))));
  printf ("R(newfull32): %x\n", R(newfull32));

//  printf ("BB00111111: %x\n", BB00111111);
//  printf ("BB11111100: %x\n", BB11111100);
//  printf ("BB11111110: %x\n", BB11111110);
//  printf ("BB01111111: %x\n", BB01111111);

//  newfull32--;
//  full128--;
//  printf ("Did -1 on each \n");
//  uint64_t high64 = (full128 & 0xFFFFFFFFFFFFFFFF);
//  uint64_t low64 = (full128 >> 64);
//  printf ("Was 2^32=4294967296=FFFFFFFF\nnewfull32: %x\n", newfull32);
//  printf ("Was 2^128=FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\nfull128: %llx %llx\n", low64, high64);
}
