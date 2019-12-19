/*bin/echo  ' -*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2019 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for any     │
│ purpose with or without fee is hereby granted, provided that the above       │
│ copyright notice and this permission notice appear in all copies.            │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES     │
│ WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF             │
│ MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR      │
│ ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES       │
│ WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN        │
│ ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF      │
│ OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.               │
╚────────────────────────────────────────────────────────────────────'>/dev/null
  if [ -z "$CC" ]; then
    CC=$(command -v clang-9) ||
    CC=$(command -v clang-8) ||
    CC=$(command -v clang) ||
    CC=$(command -v gcc) ||
    CC=$(command -v cc)
  fi
  if ! [ "${0%.*}.exe" -nt "$0" ]; then
    $CC -march=native -Ofast $CFLAGS -o "${0%.*}.exe" "$0" -lm || exit
  fi
  exec "${0%.*}.exe" "$@"
  exit

NAME

  derasterize - textmode supremacy

SYNOPSIS

  derasterize [PNG|JPG|ETC]...

DESCRIPTION

  This script converts things like photographs into UNICODE text with
  ANSI colors for display within a terminal. It performs lots of AVX2
  optimized math to deliver the best quality on modern terminals with
  24-bit color support, e.g. Kitty, Gnome Terminal, CMD.EXE, etc.

EXAMPLES

  $ apt install build-essential imagemagick
  $ ./derasterize.c lemur.png

AUTHORS

  Justine Tunney <jtunney@gmail.com>

*/
#ifdef __ELF__
__asm__(".ident\t\"\\n\\n\
derasterize (ISC License)\\n\
Copyright 2019 Justine Alexandra Roberts Tunney\"");
#endif
#include <assert.h>
#include <fcntl.h>
#include <fenv.h>
#include <limits.h>
#include <locale.h>
#include <malloc.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <uchar.h>
#include <unistd.h>

#define CN 3u        /* # channels (rgb) */
#define YS 8u        /* row stride -or- block height */
#define XS 4u        /* column stride -or- block width */
#define GT 44u       /* total glyphs */
#define GN (GT - 3)  /* # glyphs to consider */
#define MC 8u        /* log2(#) colors to consider; quality/speed tradeoff */
#define BN (YS * XS) /* # scalars in block/glyph plane */

#define PHIPRIME 0x9E3779B1u
#define DIST(X, Y) ((X) - (Y))
#define SQR(X) ((X) * (X))
#define ABS(X) abs(X)
#define SAD(X, Y) ABS(DIST(X, Y)) /* faster */
#define SSD(X, Y) SQR(DIST(X, Y)) /* better */
#define ROUNDUP(W, K) (((W) + ((K)-1)) & ~((K)-1))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define ARRAYLEN(A) \
  ((sizeof(A) / sizeof(*(A))) / ((unsigned)!(sizeof(A) % sizeof(*(A)))))

#define ORDIE(X)  \
  do              \
    if (!(X)) {   \
      perror(#X); \
      exit(1);    \
    }             \
  while (0)

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

static const uint32_t kGlyphs[GT] = /* clang-format off */ {
    /* 0020 ' ' empty block [ascii:20,cp437:20] */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0),
    /* 2588 '█' full block [cp437] */
    G(1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1),
    /* 2584 '▄' lower half block [cp437:dc] */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1),
    /* 2580 '▀' upper half block [cp437] */
    G(1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0),
    /* 2590 '▐' right half block [cp437:de] */
    G(0,0,1,1,
      0,0,1,1,
      0,0,1,1,
      0,0,1,1,
      0,0,1,1,
      0,0,1,1,
      0,0,1,1,
      0,0,1,1),
    /* 258c '▌' left half block [cp437] */
    G(1,1,0,0,
      1,1,0,0,
      1,1,0,0,
      1,1,0,0,
      1,1,0,0,
      1,1,0,0,
      1,1,0,0,
      1,1,0,0),
    /* 259d '▝' quadrant upper right */
    G(0,0,1,1,
      0,0,1,1,
      0,0,1,1,
      0,0,1,1,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0),
    /* TODO '▙' */
    G(1,1,0,0,
      1,1,0,0,
      1,1,0,0,
      1,1,0,0,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,0),
    /* 2597 '▗' quadrant lower right */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,1,1,
      0,0,1,1,
      0,0,1,1,
      0,0,1,1),
    /* TODO '▛' */
    G(1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,0,0,
      1,1,0,0,
      1,1,0,0,
      1,1,0,1),
    /* 2596 '▖' quadrant lower left */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      1,1,0,0,
      1,1,0,0,
      1,1,0,0,
      1,1,0,0),
    /* TODO '▜' */
    G(1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      0,0,1,1,
      0,0,1,1,
      0,0,1,1,
      0,0,1,0),
    /* 2598 '▘' quadrant upper left */
    G(1,1,0,0,
      1,1,0,0,
      1,1,0,0,
      1,1,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0),
    /* TODO '▟' */
    G(0,0,1,1,
      0,0,1,1,
      0,0,1,1,
      0,0,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,0),
    /* 259e '▞' quadrant upper right and lower left */
    G(0,0,1,1,
      0,0,1,1,
      0,0,1,1,
      0,0,1,1,
      1,1,0,0,
      1,1,0,0,
      1,1,0,0,
      1,1,0,0),
    /* TODO '▚' */
    G(1,1,0,0,
      1,1,0,0,
      1,1,0,0,
      1,1,0,0,
      0,0,1,1,
      0,0,1,1,
      0,0,1,1,
      0,0,1,0),
    /* 2594 '▔' upper one eighth block */
    G(1,1,1,1,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0),
    /* 2581 '▁' lower one eighth block */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      1,1,1,1),
    /* 2582 '▂' lower one quarter block */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      1,1,1,1,
      1,1,1,1),
    /* 2583 '▃' lower three eighths block */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1),
    /* 2585 '▃' lower five eighths block */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1),
    /* 2586 '▆' lower three quarters block */
    G(0,0,0,0,
      0,0,0,0,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1),
    /* 2587 '▇' lower seven eighths block */
    G(0,0,0,0,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1,
      1,1,1,1),
    /* 258e '▎' left one quarter block */
    G(1,0,0,0,
      1,0,0,0,
      1,0,0,0,
      1,0,0,0,
      1,0,0,0,
      1,0,0,0,
      1,0,0,0,
      1,0,0,0),
    /* 258a '▊' left three quarters block */
    G(1,1,1,0,
      1,1,1,0,
      1,1,1,0,
      1,1,1,0,
      1,1,1,0,
      1,1,1,0,
      1,1,1,0,
      1,1,1,0),
      /* ▁ *\
    2501▕━▎box drawings heavy horizontal
      \* ▔ */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      1,1,1,1,
      1,1,1,1,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0),
      /* ▁ *\
   25019▕┉▎box drawings heavy quadruple dash horizontal
      \* ▔ */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      1,0,1,0,
      0,1,0,1,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0),
      /* ▁ *\
    2503▕┃▎box drawings heavy vertical
      \* ▔ */
    G(0,1,1,0,
      0,1,1,0,
      0,1,1,0,
      0,1,1,0,
      0,1,1,0,
      0,1,1,0,
      0,1,1,0,
      0,1,1,0),
      /* ▁ *\
    254b▕╋▎box drawings heavy vertical and horizontal
      \* ▔ */
    G(0,1,1,0,
      0,1,1,0,
      0,1,1,0,
      1,1,1,1,
      1,1,1,1,
      0,1,1,0,
      0,1,1,0,
      0,1,1,0),
      /* ▁ *\
    2579▕╹▎box drawings heavy up
      \* ▔ */
    G(0,1,1,0,
      0,1,1,0,
      0,1,1,0,
      0,1,1,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0),
      /* ▁ *\
    257a▕╺▎box drawings heavy right
      \* ▔ */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,1,1,
      0,0,1,1,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0),
      /* ▁ *\
    257b▕╻▎box drawings heavy down
      \* ▔ */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,1,1,0,
      0,1,1,0,
      0,1,1,0,
      0,1,1,0),
      /* ▁ *\
    2578▕╸▎box drawings heavy left
      \* ▔ */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      1,1,0,0,
      1,1,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0),
      /* ▁ *\
    250f▕┏▎box drawings heavy down and right
      \* ▔ */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,1,1,1,
      0,1,1,1,
      0,1,1,0,
      0,1,1,0,
      0,1,1,0),
      /* ▁ *\
    251b▕┛▎box drawings heavy up and left
      \* ▔ */
    G(0,1,1,0,
      0,1,1,0,
      0,1,1,0,
      1,1,1,0,
      1,1,1,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0),
      /* ▁ *\
    2513▕┓▎box drawings heavy down and left
      \* ▔ */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      1,1,1,0,
      1,1,1,0,
      0,1,1,0,
      0,1,1,0,
      0,1,1,0),
      /* ▁ *\
    2517▕┗▎box drawings heavy up and right
      \* ▔ */
    G(0,1,1,0,
      0,1,1,0,
      0,1,1,0,
      0,1,1,1,
      0,1,1,1,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0),
      /* ▁ *\
    25E2▕◢▎black lower right triangle
      \* ▔ */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,1,
      0,0,1,1,
      1,1,1,1,
      0,0,0,0,
      0,0,0,0),
      /* ▁ *\
    25E3▕◣▎black lower left triangle
      \* ▔ */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      1,0,0,0,
      1,1,0,0,
      1,1,1,1,
      0,0,0,0,
      0,0,0,0),
      /* ▁ *\
    25E4▕◥▎black upper right triangle
      \* ▔ */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      1,1,1,1,
      0,0,1,1,
      0,0,0,1,
      0,0,0,0,
      0,0,0,0),
      /* ▁ *\
    25E5▕◤▎black upper left triangle
      \* ▔ */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      1,1,1,1,
      1,1,0,0,
      1,0,0,0,
      0,0,0,0,
      0,0,0,0),
      /* ▁ *\
    2500▕═▎box drawings double horizontal
      \* ▔ */
    G(0,0,0,0,
      0,0,0,0,
      1,1,1,1,
      0,0,0,0,
      1,1,1,1,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0),
      /* ▁ *\
    23BB▕⎻▎horizontal scan line 3
      \* ▔ */
    G(0,0,0,0,
      0,0,0,0,
      1,1,1,1,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0),
      /* ▁ *\
    23BD▕⎼▎horizontal scan line 9
      \* ▔ */
    G(0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      0,0,0,0,
      1,1,1,1,
      0,0,0,0,
      0,0,0,0),
} /* clang-format on */;

static const char16_t kRunes[GT] = {
    u' ', /* 0020 empty block [ascii:20,cp437:20] */
    u'█', /* 2588 full block [cp437] */
    u'▄', /* 2584 lower half block [cp437:dc] */
    u'▀', /* 2580 upper half block [cp437] */
    u'▐', /* 2590 right half block [cp437:de] */
    u'▌', /* 258C left half block */
    u'▝', /* 259D quadrant upper right */
    u'▙', /* TODO */
    u'▗', /* 2597 quadrant lower right */
    u'▛', /* TODO */
    u'▖', /* 2596 quadrant lower left */
    u'▜', /* TODO */
    u'▘', /* 2598 quadrant upper left */
    u'▟', /* TODO */
    u'▞', /* 259E quadrant upper right and lower left */
    u'▚', /* TODO */
    u'▔', /* 2594 upper one eighth block */
    u'▁', /* 2581 lower one eighth block */
    u'▂', /* 2582 lower one quarter block */
    u'▃', /* 2583 lower three eighths block */
    u'▅', /* 2585 lower five eighths block */
    u'▆', /* 2586 lower three quarters block */
    u'▇', /* 2587 lower seven eighths block */
    u'▎', /* 258E left one quarter block */
    u'▊', /* 258A left three quarters block */
    u'━', /* 2501 box drawings heavy horizontal */
    u'┉', /* 2509 box drawings heavy quadruple dash horizontal */
    u'┃', /* 2503 box drawings heavy vertical */
    u'╋', /* 254B box drawings heavy vertical & horiz. */
    u'╹', /* 2579 box drawings heavy up */
    u'╺', /* 257A box drawings heavy right */
    u'╻', /* 257B box drawings heavy down */
    u'╸', /* 2578 box drawings heavy left */
    u'┏', /* 250F box drawings heavy down and right */
    u'┛', /* 251B box drawings heavy up and left */
    u'┓', /* 2513 box drawings heavy down and left */
    u'┗', /* 2517 box drawings heavy up and right */
    u'◢', /* 25E2 black lower right triangle */
    u'◣', /* 25E3 black lower left triangle */
    u'◥', /* 25E4 black upper right triangle */
    u'◤', /* 25E5 black upper left triangle */
    u'═', /* 2550 box drawings double horizontal */
    u'⎻', /* 23BB horizontal scan line 3 */
    u'⎼', /* 23BD horizontal scan line 9 */
};

/*───────────────────────────────────────────────────────────────────────────│─╗
│ derasterize § encoding                                                   ─╬─│┼
╚────────────────────────────────────────────────────────────────────────────│*/

/**
 * Returns binary logarithm of integer.
 * @dominion 𝑥≥1 ∧ 𝑥∊ℤ
 * @return [0,31)
 */
static unsigned bsr(unsigned x) {
#if (defined(__GNUC__) && !defined(__STRICT_ANSI__) && \
     (defined(__x86_64__) || defined(__i386__)))
  asm("bsr\t%1,%0" : "=r"(x) : "r"(x) : "cc");
#else
  static const unsigned char kDebruijn[32] = {
      0, 9,  1,  10, 13, 21, 2,  29, 11, 14, 16, 18, 22, 25, 3, 30,
      8, 12, 20, 28, 15, 17, 24, 7,  19, 27, 23, 6,  26, 5,  4, 31};
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x = kDebruijn[(x * 0x07c4acddu) >> 27];
#endif
  return x;
}

/**
 * Encodes Thompson-Pike variable length integer.
 *
 * @return word-encoded multibyte string
 * @note canonicalizes multibyte NUL and other numbers banned by the IETF
 * @note designed on a napkin in a New Jersey diner
 */
static unsigned long tpenc(wchar_t x) {
  if (0x00 <= x && x <= 0x7f) {
    return x;
  } else {
    static const struct ThomPike {
      unsigned char len, mark;
    } kThomPike[32 - 7] = {
        {1, 0xc0}, {1, 0xc0}, {1, 0xc0}, {1, 0xc0}, {2, 0xe0},
        {2, 0xe0}, {2, 0xe0}, {2, 0xe0}, {2, 0xe0}, {3, 0xf0},
        {3, 0xf0}, {3, 0xf0}, {3, 0xf0}, {3, 0xf0}, {4, 0xf8},
        {4, 0xf8}, {4, 0xf8}, {4, 0xf8}, {4, 0xf8}, {5, 0xfc},
        {5, 0xfc}, {5, 0xfc}, {5, 0xfc}, {5, 0xfc}, {5, 0xfc},
    };
    wchar_t wc;
    unsigned long ec;
    struct ThomPike op;
    ec = 0;
    wc = x;
    op = kThomPike[bsr(wc) - 7];
    do {
      ec |= 0x3f & wc;
      ec |= 0x80;
      ec <<= 010;
      wc >>= 006;
    } while (--op.len);
    return ec | wc | op.mark;
  }
}

/**
 * Formats Thompson-Pike variable length integer to array.
 *
 * @param p needs at least 8 bytes
 * @return p + number of bytes written, cf. mempcpy
 * @note no NUL-terminator is added
 */
static char *tptoa(char *p, wchar_t x) {
  unsigned long w;
  for (w = tpenc(x); w; w >>= 010) *p++ = w & 0xff;
  return p;
}

/**
 * Formats byte to array as decimal.
 *
 * @param p needs at least 4 bytes
 * @return p + number of bytes written, cf. mempcpy
 * @note call btoa(0,0) once at startup
 * @note no NUL-terminator is added
 */
static char *btoa(char *p, int c) {
  static char kBtoa[256][4];
  if (p) {
    memcpy(p, kBtoa[c & 0xff], 4);
    return p + (p[3] & 0xff);
  } else {
    for (c = 0; c < 256; ++c) {
      if (c < 10) {
        kBtoa[c][0] = '0' + c;
        kBtoa[c][3] = 1;
      } else if (c < 100) {
        kBtoa[c][0] = '0' + c / 10;
        kBtoa[c][1] = '0' + c % 10;
        kBtoa[c][3] = 2;
      } else {
        kBtoa[c][0] = '0' + c / 100;
        kBtoa[c][1] = '0' + c % 100 / 10;
        kBtoa[c][2] = '0' + c % 100 % 10;
        kBtoa[c][3] = 3;
      }
    }
    return 0;
  }
}

/*───────────────────────────────────────────────────────────────────────────│─╗
│ derasterize § colors                                                     ─╬─│┼
╚────────────────────────────────────────────────────────────────────────────│*/

#if (defined(__GNUC__) && !defined(__STRICT_ANSI__) && \
     (defined(__x86_64__) || defined(__i386__)))
#define pow(X, Y)              \
  ({                           \
    long double St0, St1;      \
    asm("fyl2x\n\t"            \
        "fld1\n\t"             \
        "fld\t%1\n\t"          \
        "fprem\n\t"            \
        "f2xm1\n\t"            \
        "faddp\n\t"            \
        "fscale"               \
        : "=t"(St0), "=u"(St1) \
        : "0"(X), "1"(Y));     \
    St0;                       \
  })
#endif

static double frgb2linl(double x) {
  double r1, r2;
  r1 = x / 12.92;
  r2 = pow((x + 0.055) / (1 + 0.055), 2.4);
  return x <= 0.04045 ? r1 : r2;
}

/**
 * Converts standard RGB to linear RGB.
 *
 * This makes subtraction look good by flattening out the bias curve
 * that PC display manufacturers like to use.
 */
void rgb2lin(unsigned char *o, const unsigned char *u, unsigned n) {
  unsigned i;
  double *f;
  if ((f = memalign(32, 1024 + n * sizeof(double)))) {
    for (i = 0; i < n; ++i) f[i] = u[i];
    for (i = 0; i < n; ++i) f[i] /= 255;
    for (i = 0; i < n; ++i) f[i] = frgb2linl(f[i]);
    for (i = 0; i < n; ++i) f[i] = MAX(0, MIN(1, f[i]));
    for (i = 0; i < n; ++i) f[i] *= 255;
    for (i = 0; i < n; ++i) o[i] = lrintf(f[i]);
    free(f);
  } else {
    memcpy(o, u, n);
  }
}

/*───────────────────────────────────────────────────────────────────────────│─╗
│ derasterize § blocks                                                     ─╬─│┼
╚────────────────────────────────────────────────────────────────────────────│*/

struct Cell {
  char16_t rune;
  unsigned char bg[CN], fg[CN];
};

/**
 * Serializes ANSI background, foreground, and UNICODE glyph to wire.
 */
char *celltoa(char *p, struct Cell cell, struct Cell last) {
  *p++ = 033;
  *p++ = '[';
  *p++ = '4';
  *p++ = '8';
  *p++ = ';';
  *p++ = '2';
  *p++ = ';';
  p = btoa(p, cell.bg[0]);
  *p++ = ';';
  p = btoa(p, cell.bg[1]);
  *p++ = ';';
  p = btoa(p, cell.bg[2]);
  *p++ = ';';
  *p++ = '3';
  *p++ = '8';
  *p++ = ';';
  *p++ = '2';
  *p++ = ';';
  p = btoa(p, cell.fg[0]);
  *p++ = ';';
  p = btoa(p, cell.fg[1]);
  *p++ = ';';
  p = btoa(p, cell.fg[2]);
  *p++ = 'm';
  p = tptoa(p, cell.rune);
  return p;
}

/**
 * Picks ≤2**MC unique (bg,fg) pairs from product of lb.
 */
unsigned combinecolors(unsigned char bf[1u << MC][2],
                       const unsigned char lb[CN * BN]) {
  uint64_t hv, ht[(1u << MC) * 2];
  unsigned i, j, n, b, f, h, hi, bu, fu;
  memset(ht, 0, sizeof(ht));
  for (n = b = 0; b < BN && n < (1u << MC); ++b) {
    bu = lb[2 * BN + b] << 020 | lb[1 * BN + b] << 010 | lb[0 * BN + b];
    hi = 0;
    hi = (((bu >> 000) & 0xff) + hi) * PHIPRIME;
    hi = (((bu >> 010) & 0xff) + hi) * PHIPRIME;
    hi = (((bu >> 020) & 0xff) + hi) * PHIPRIME;
    for (f = b + 1; f < BN && n < (1u << MC); ++f) {
      fu = lb[2 * BN + f] << 020 | lb[1 * BN + f] << 010 | lb[0 * BN + f];
      h = hi;
      h = (((fu >> 000) & 0xff) + h) * PHIPRIME;
      h = (((fu >> 010) & 0xff) + h) * PHIPRIME;
      h = (((fu >> 020) & 0xff) + h) * PHIPRIME;
      h = h & 0xffff;
      h = MAX(1, h);
      hv = 0;
      hv <<= 030;
      hv |= fu;
      hv <<= 030;
      hv |= bu;
      hv <<= 020;
      hv |= h;
      for (i = 0;; ++i) {
        j = (h + i * (i + 1) / 2) & (ARRAYLEN(ht) - 1);
        if (!ht[j]) {
          ht[j] = hv;
          bf[n][0] = b;
          bf[n][1] = f;
          n++;
          break;
        } else if (ht[j] == hv) {
          break;
        }
      }
    }
  }
  return n;
}

/**
 * Computes distance between synthetic block and actual.
 */
unsigned adjudicate(unsigned b, unsigned f, unsigned g,
                    const unsigned char lb[CN * BN]) {
  unsigned i, k, r, fu, bu, gu;
  unsigned char p1[BN], p2[BN];
  r = 0;
  gu = kGlyphs[g];
  for (k = 0; k < CN; ++k) {
    bu = lb[k * BN + b];
    fu = lb[k * BN + f];
    for (i = 0; i < BN; ++i) p1[i] = (gu & (1u << i)) ? fu : bu;
    for (i = 0; i < BN; ++i) p2[i] = lb[k * BN + i];
    for (i = 0; i < BN; ++i) r += SSD(p1[i], p2[i]);
  }
  return r;
}

/**
 * Converts tiny bitmap graphic into unicode glyph.
 */
struct Cell derasterize(unsigned char block[CN * BN]) {
  struct Cell cell;
  unsigned n, b, f, g, i, best, r;
  unsigned char lb[CN * BN], bf[1u << MC][2];
  rgb2lin(lb, block, ARRAYLEN(lb));
  n = combinecolors(bf, lb);
  best = -1u;
  cell.rune = 0;
  for (i = 0; i < n; ++i) {
    b = bf[i][0];
    f = bf[i][1];
    for (g = 0; g < GN; ++g) {
      r = adjudicate(b, f, g, lb);
      if (r < best) {
        best = r;
        cell.rune = kRunes[g];
        cell.bg[0] = block[0 * BN + b];
        cell.bg[1] = block[1 * BN + b];
        cell.bg[2] = block[2 * BN + b];
        cell.fg[0] = block[0 * BN + f];
        cell.fg[1] = block[1 * BN + f];
        cell.fg[2] = block[2 * BN + f];
        if (!r) return cell;
      }
    }
  }
  assert(cell.rune);
  return cell;
}

/*───────────────────────────────────────────────────────────────────────────│─╗
│ derasterize § graphics                                                   ─╬─│┼
╚────────────────────────────────────────────────────────────────────────────│*/

/**
 * Turns packed 8-bit RGB graphic into ANSI UNICODE text.
 */
char *RenderImage(char *vt, const unsigned char *rgb, unsigned yn,
                  unsigned xn) {
  char *v;
  struct Cell c1, c2;
  unsigned y, x, i, j, k, w;
  unsigned char block[CN * BN];
  const unsigned char *rows[YS];
  v = vt;
  c1.rune = 0;
  rgb -= (w = xn * XS * CN);
  for (y = 0; y < yn; ++y) {
    if (y) {
      while (v > vt && v[-1] == ' ') --v;
      *v++ = '\r';
      *v++ = '\n';
    }
    for (i = 0; i < YS; ++i) {
      rows[i] = (rgb += w) - XS * CN;
    }
    for (x = 0; x < xn; ++x) {
      for (i = 0; i < YS; ++i) {
        rows[i] += XS * CN;
        for (j = 0; j < XS; ++j) {
          for (k = 0; k < CN; ++k) {
            block[(k * YS + i) * XS + j] = rows[i][j * CN + k];
          }
        }
      }
      c2 = derasterize(block);
      v = celltoa(v, c2, c1);
      c1 = c2;
    }
  }
  return v;
}

/*───────────────────────────────────────────────────────────────────────────│─╗
│ derasterize § systems                                                    ─╬─│┼
╚────────────────────────────────────────────────────────────────────────────│*/

void PrintImage(void *rgb, unsigned yn, unsigned xn) {
  char *v, *vt;
  vt = malloc(yn * (xn * (32 + (2 + (1 + 3) * 3) * 2 + 1 + 3)) * 1 + 5 + 1);
  v = RenderImage(vt, rgb, yn, xn);
  *v++ = '\r';
  *v++ = 033;
  *v++ = '[';
  *v++ = '0';
  *v++ = 'm';
  write(1, vt, v - vt);
  free(vt);
}

/**
 * Determines dimensions of teletypewriter.
 */
void GetTermSize(unsigned *out_rows, unsigned *out_cols) {
  struct winsize ws;
  ws.ws_row = 24;
  ws.ws_col = 80;
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
  }
  *out_rows = ws.ws_row;
  *out_cols = ws.ws_col;
}

void ReadAll(int fd, char *p, size_t n) {
  ssize_t rc;
  size_t got;
  do {
    ORDIE((rc = read(fd, p, n)) != -1);
    got = rc;
    if (!got && n) {
      fprintf(stderr, "error: expected eof\n");
      exit(EXIT_FAILURE);
    }
    p += got;
    n -= got;
  } while (n);
}

unsigned char *LoadImageOrDie(char *path, unsigned yn, unsigned xn) {
  void *rgb;
  size_t size;
  int pid, ws, rw[2];
  char dim[10 + 1 + 10 + 1 + 1];
  sprintf(dim, "%ux%u!", xn * XS, yn * YS);
  pipe(rw);
  if (!(pid = fork())) {
    close(rw[0]);
    dup2(rw[1], STDOUT_FILENO);
    execlp("convert", "convert", path, "-resize", dim, "-colorspace", "RGB",
           "-depth", "8", "rgb:-", NULL);
    _exit(EXIT_FAILURE);
  }
  close(rw[1]);
  ORDIE((rgb = malloc((size = yn * YS * xn * XS * 3))));
  ReadAll(rw[0], rgb, size);
  ORDIE(close(rw[0]) != -1);
  ORDIE(waitpid(pid, &ws, 0) != -1);
  ORDIE(WEXITSTATUS(ws) == 0);
  return rgb;
}

int main(int argc, char *argv[]) {
  int i;
  void *rgb;
  unsigned yn, xn;
  btoa(0, 0);
  setlocale(LC_ALL, "C.UTF-8");
  GetTermSize(&yn, &xn);
  for (i = 1; i < argc; ++i) {
    rgb = LoadImageOrDie(argv[i], yn, xn);
    PrintImage(rgb, yn, xn);
    free(rgb);
  }
  return 0;
}
