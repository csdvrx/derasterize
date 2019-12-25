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

// New Macro

#include "binary.h"

// https://stackoverflow.com/questions/21511533/reverse-integer-bitwise-without-using-loop
// ghefcdab -> hgfedcba
#define R1(B) ((B & 0xaa) >> 1) | ((B & 0x55) << 1)
// abcdefgh -> efghabcd
#define R2(B) ((B & 0xcc) >> 2) | ((B & 0x33) << 2)
// abcdefgh -> efghabcddd
#define R3(B) ((B & 0xf0) >> 4) | ((B & 0x0f) << 4)

// FIXME: something is wrong with both B32 and BB128 byte ordering

// For 4x8=32 with B
#define B32(A,B,C,D,E,F,G,H) ( \
  ((uint32_t)(G)<<28) \
+ ((uint32_t)(H)<<24) \
+ ((uint32_t)(E)<<20) \
+ ((uint32_t)(F)<<16) \
+ ((uint32_t)(C)<<12) \
+ ((uint32_t)(D)<<8) \
+ ((uint32_t)(A)<<4) \
+            (B))

// For 8x16=128 with BB
#define BB128( \
                A,B,C,D,E,F,G,H \
                , \
                I,J,K,L,M,N,O,P \
                ) ( \
  ((uint128_t)(M)<<120) \
+ ((uint128_t)(N)<<112) \
+ ((uint128_t)(O)<<104) \
+ ((uint128_t)(P)<<96) \
+ ((uint128_t)(I)<<88) \
+ ((uint128_t)(J)<<80) \
+ ((uint128_t)(K)<<72) \
+ ((uint128_t)(L)<<64) \
+ (( uint64_t)(E)<<56) \
+ (( uint64_t)(F)<<48) \
+ (( uint64_t)(G)<<40) \
+ (( uint64_t)(H)<<32) \
+ (( uint64_t)(A)<<24) \
+ (( uint64_t)(B)<<16) \
+ (( uint64_t)(C)<<8) \
+  ( uint64_t)(D))

// For 8x8=64 with BB
#define BB64(I,J,K,L,M,N,O,P) (\
+ (( uint64_t)I<<56) \
+ (( uint64_t)J<<48) \
+ (( uint64_t)J<<40) \
+ (( uint64_t)L<<32) \
+ (( uint64_t)M<<24) \
+ (( uint64_t)N<<16) \
+ (( uint64_t)O<<8) \
+  ( uint64_t)P)

uint32_t oldfull32= 
    G(0,0,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,0,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1);

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
      BB11111111,
      BB11111111,
      BB11111111,
      BB11111111,
      BB11111111,
      BB11111111,
      BB11111111,
      BB11111111,
      BB11111111,
      BB11111111);

uint64_t testfull64 = 
BB64(BB11111111,
      BB11111111,
      BB11111111,
      BB11111111,
      BB11111111,
      BB11111111,
      BB11111111,
      BB11111111);

int main() {
  // Check the new macro works
  printf ("oldfull32 is b00111111111111101111111111111111\n");
  printf ("oldfull32: %x\n", oldfull32);
  printf ("newfull32: %x\n", newfull32);
  printf ("BB00111111: %x\n", BB00111111);
  printf ("BB11111100: %x\n", BB11111100);
  printf ("BB11111110: %x\n", BB11111110);
  printf ("BB01111111: %x\n", BB01111111);

  newfull32--;
  testfull64--;
  full128--;
  printf ("Did -1 on each \n");
  uint64_t high64 = (full128 & 0xFFFFFFFFFFFFFFFF);
  uint64_t low64 = (full128 >> 64);
  printf ("Was 2^32=4294967296=FFFFFFFF\nnewfull32: %x\n", newfull32);
  printf ("Was 2^64=FFFFFFFFFFFFFFFF\ntestfull64: %llx\n", testfull64);
  printf ("Was 2^128=FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF\nfull128: %llx %llx\n", low64, high64);
}
