/*-
 * Copyright (c) 2001, 2002 Allan Saddi <allan@saddi.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Define WORDS_BIGENDIAN if compiling on a big-endian architecture.
 *
 * Define SHA1_TEST to test the implementation using the NIST's
 * sample messages. The output should be:
 *
 *   a9993e36 4706816a ba3e2571 7850c26c 9cd0d89d
 *   84983e44 1c3bd26e baae4aa1 f95129e5 e54670f1
 *   34aa973c d4c4daa4 f61eeb2b dbad2731 6534016f
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <stdio.h>
#include <glib.h>

#include "lm-sha.h"

#define SHA1_HASH_SIZE 20

/* Hash size in 32-bit words */
#define SHA1_HASH_WORDS 5

struct _SHA1Context {
  guint64 totalLength;
  guint32 hash[SHA1_HASH_WORDS];
  guint32 bufferLength;
  union {
    guint32 words[16];
    guint8 bytes[64];
  } buffer;
};

typedef struct _SHA1Context SHA1Context;

#ifdef __cplusplus
extern "C" {
#endif

static void SHA1Init (SHA1Context *sc);
static void SHA1Update (SHA1Context *sc, const void *udata, guint32 len);
static void SHA1Final (SHA1Context *sc, guint8 hash[SHA1_HASH_SIZE]);

#ifdef __cplusplus
}
#endif

#ifndef lint
static const char rcsid[] =
	"$Id$";
#endif /* !lint */

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

#define F_0_19(x, y, z) ((z) ^ ((x) & ((y) ^ (z))))
#define F_20_39(x, y, z) ((x) ^ (y) ^ (z))
#define F_40_59(x, y, z) (((x) & ((y) | (z))) | ((y) & (z)))
#define F_60_79(x, y, z) ((x) ^ (y) ^ (z))

#define DO_ROUND(F, K) { \
  temp = ROTL(a, 5) + F(b, c, d) + e + *(W++) + K; \
  e = d; \
  d = c; \
  c = ROTL(b, 30); \
  b = a; \
  a = temp; \
}

#define K_0_19 0x5a827999L
#define K_20_39 0x6ed9eba1L
#define K_40_59 0x8f1bbcdcL
#define K_60_79 0xca62c1d6L

#ifndef RUNTIME_ENDIAN

#ifdef WORDS_BIGENDIAN

#define BYTESWAP(x) (x)
#define BYTESWAP64(x) (x)

#else /* WORDS_BIGENDIAN */

#define BYTESWAP(x) ((ROTR((x), 8) & 0xff00ff00L) | \
		     (ROTL((x), 8) & 0x00ff00ffL))
#define BYTESWAP64(x) _byteswap64(x)

static inline guint64 _byteswap64(guint64 x)
{
  guint32 a = x >> 32;
  guint32 b = (guint32) x;
  return ((guint64) BYTESWAP(b) << 32) | (guint64) BYTESWAP(a);
}

#endif /* WORDS_BIGENDIAN */

#else /* !RUNTIME_ENDIAN */

static int littleEndian;

#define BYTESWAP(x) _byteswap(x)
#define BYTESWAP64(x) _byteswap64(x)

#define _BYTESWAP(x) ((ROTR((x), 8) & 0xff00ff00L) | \
		      (ROTL((x), 8) & 0x00ff00ffL))
#define _BYTESWAP64(x) __byteswap64(x)

static inline guint64 __byteswap64(guint64 x)
{
  guint32 a = x >> 32;
  guint32 b = (guint32) x;
  return ((guint64) _BYTESWAP(b) << 32) | (guint64) _BYTESWAP(a);
}

static inline guint32 _byteswap(guint32 x)
{
  if (!littleEndian)
    return x;
  else
    return _BYTESWAP(x);
}

static inline guint64 _byteswap64(guint64 x)
{
  if (!littleEndian)
    return x;
  else
    return _BYTESWAP64(x);
}

static inline void setEndian(void)
{
  union {
    guint32 w;
    guint8 b[4];
  } endian;

  endian.w = 1L;
  littleEndian = endian.b[0] != 0;
}

#endif /* !RUNTIME_ENDIAN */

static const guint8 padding[64] = {
  0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static void
SHA1Init (SHA1Context *sc)
{
#ifdef RUNTIME_ENDIAN
  setEndian ();
#endif /* RUNTIME_ENDIAN */

#ifdef G_OS_WIN32
  sc->totalLength = 0L;
#else
  sc->totalLength = 0LL;
#endif
  sc->hash[0] = 0x67452301L;
  sc->hash[1] = 0xefcdab89L;
  sc->hash[2] = 0x98badcfeL;
  sc->hash[3] = 0x10325476L;
  sc->hash[4] = 0xc3d2e1f0L;
  sc->bufferLength = 0L;
}

static void
burnStack (int size)
{
  char buf[128];

  memset (buf, 0, sizeof (buf));
  size -= sizeof (buf);
  if (size > 0)
    burnStack (size);
}

static void
SHA1Guts (SHA1Context *sc, const guint32 *cbuf)
{
  guint32 buf[80];
  guint32 *W, *W3, *W8, *W14, *W16;
  guint32 a, b, c, d, e, temp;
  int i;

  W = buf;

  for (i = 15; i >= 0; i--) {
    *(W++) = BYTESWAP(*cbuf);
    cbuf++;
  }

  W16 = &buf[0];
  W14 = &buf[2];
  W8 = &buf[8];
  W3 = &buf[13];

  for (i = 63; i >= 0; i--) {
    *W = *(W3++) ^ *(W8++) ^ *(W14++) ^ *(W16++);
    *W = ROTL(*W, 1);
    W++;
  }

  a = sc->hash[0];
  b = sc->hash[1];
  c = sc->hash[2];
  d = sc->hash[3];
  e = sc->hash[4];

  W = buf;

#ifndef SHA1_UNROLL
#define SHA1_UNROLL 20
#endif /* !SHA1_UNROLL */

#if SHA1_UNROLL == 1
  for (i = 19; i >= 0; i--)
    DO_ROUND(F_0_19, K_0_19);

  for (i = 19; i >= 0; i--)
    DO_ROUND(F_20_39, K_20_39);

  for (i = 19; i >= 0; i--)
    DO_ROUND(F_40_59, K_40_59);

  for (i = 19; i >= 0; i--)
    DO_ROUND(F_60_79, K_60_79);
#elif SHA1_UNROLL == 2
  for (i = 9; i >= 0; i--) {
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
  }

  for (i = 9; i >= 0; i--) {
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
  }

  for (i = 9; i >= 0; i--) {
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
  }

  for (i = 9; i >= 0; i--) {
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
  }
#elif SHA1_UNROLL == 4
  for (i = 4; i >= 0; i--) {
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
  }

  for (i = 4; i >= 0; i--) {
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
  }

  for (i = 4; i >= 0; i--) {
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
  }

  for (i = 4; i >= 0; i--) {
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
  }
#elif SHA1_UNROLL == 5
  for (i = 3; i >= 0; i--) {
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
  }

  for (i = 3; i >= 0; i--) {
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
  }

  for (i = 3; i >= 0; i--) {
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
  }

  for (i = 3; i >= 0; i--) {
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
  }
#elif SHA1_UNROLL == 10
  for (i = 1; i >= 0; i--) {
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
    DO_ROUND(F_0_19, K_0_19);
  }

  for (i = 1; i >= 0; i--) {
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
    DO_ROUND(F_20_39, K_20_39);
  }

  for (i = 1; i >= 0; i--) {
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
    DO_ROUND(F_40_59, K_40_59);
  }

  for (i = 1; i >= 0; i--) {
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
    DO_ROUND(F_60_79, K_60_79);
  }
#elif SHA1_UNROLL == 20
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);
  DO_ROUND(F_0_19, K_0_19);

  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);
  DO_ROUND(F_20_39, K_20_39);

  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);
  DO_ROUND(F_40_59, K_40_59);

  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
  DO_ROUND(F_60_79, K_60_79);
#else /* SHA1_UNROLL */
#error SHA1_UNROLL must be 1, 2, 4, 5, 10 or 20!
#endif

  sc->hash[0] += a;
  sc->hash[1] += b;
  sc->hash[2] += c;
  sc->hash[3] += d;
  sc->hash[4] += e;
}

static void
SHA1Update (SHA1Context *sc, const void *udata, guint32 len)
{
  guint32 bufferBytesLeft;
  guint32 bytesToCopy;
  int needBurn = 0;
  guint8 *data = (guint8 *)udata;

#ifdef SHA1_FAST_COPY
  if (sc->bufferLength) {
    bufferBytesLeft = 64L - sc->bufferLength;

    bytesToCopy = bufferBytesLeft;
    if (bytesToCopy > len)
      bytesToCopy = len;

    memcpy (&sc->buffer.bytes[sc->bufferLength], data, bytesToCopy);

    sc->totalLength += bytesToCopy * 8L;

    sc->bufferLength += bytesToCopy;
    data += bytesToCopy;
    len -= bytesToCopy;

    if (sc->bufferLength == 64L) {
      SHA1Guts (sc, sc->buffer.words);
      needBurn = 1;
      sc->bufferLength = 0L;
    }
  }

  while (len > 63) {
    sc->totalLength += 512L;

    SHA1Guts (sc, data);
    needBurn = 1;

    data += 64L;
    len -= 64L;
  }

  if (len) {
    memcpy (&sc->buffer.bytes[sc->bufferLength], data, len);

    sc->totalLength += len * 8L;

    sc->bufferLength += len;
  }
#else /* SHA1_FAST_COPY */
  while (len) {
    bufferBytesLeft = 64L - sc->bufferLength;

    bytesToCopy = bufferBytesLeft;
    if (bytesToCopy > len)
      bytesToCopy = len;

    memcpy (&sc->buffer.bytes[sc->bufferLength], data, bytesToCopy);

    sc->totalLength += bytesToCopy * 8L;

    sc->bufferLength += bytesToCopy;
    data += bytesToCopy;
    len -= bytesToCopy;

    if (sc->bufferLength == 64L) {
      SHA1Guts (sc, sc->buffer.words);
      needBurn = 1;
      sc->bufferLength = 0L;
    }
  }
#endif /* SHA1_FAST_COPY */

  if (needBurn)
    burnStack (sizeof (guint32[86]) + sizeof (guint32 *[5]) + sizeof (int));
}

static void
SHA1Final (SHA1Context *sc, guint8 hash[SHA1_HASH_SIZE])
{
  guint32 bytesToPad;
  guint64 lengthPad;
  int i;

  bytesToPad = 120L - sc->bufferLength;
  if (bytesToPad > 64L)
    bytesToPad -= 64L;

  lengthPad = BYTESWAP64(sc->totalLength);

  SHA1Update (sc, padding, bytesToPad);
  SHA1Update (sc, &lengthPad, 8L);

  if (hash) {
    for (i = 0; i < SHA1_HASH_WORDS; i++) {
#ifdef SHA1_FAST_COPY
      *((guint32 *) hash) = BYTESWAP(sc->hash[i]);
#else /* SHA1_FAST_COPY */
      hash[0] = (guint8) (sc->hash[i] >> 24);
      hash[1] = (guint8) (sc->hash[i] >> 16);
      hash[2] = (guint8) (sc->hash[i] >> 8);
      hash[3] = (guint8) sc->hash[i];
#endif /* SHA1_FAST_COPY */
      hash += 4;
    }
  }
}

/**
 * lm_sha_hash:
 * @str: the input string
 *
 * Computes the SHA1 checksum of @str and prints it in text mode.
 *
 * Return value: A newly allocated string.
 **/
gchar *
lm_sha_hash (const gchar *str)
{
	gchar        *ret_val;
	SHA1Context   ctx;
	guint8        hash[SHA1_HASH_SIZE];
	gchar        *ch;
	guint         i;

	ret_val = g_malloc (SHA1_HASH_SIZE*2 + 1);

	SHA1Init (&ctx);
	SHA1Update (&ctx, str, strlen (str));
	SHA1Final (&ctx, hash);

	ch = ret_val;

	for (i = 0; i < SHA1_HASH_SIZE; ++i) {
		g_snprintf (ch, 3, "%02x", hash[i]);
		ch += 2;
	}

	return ret_val;
}
