/* bench_rs_codec_core.c — what the OBJECT costs on top of the code.
 *
 * `bench_rs_core.c` already measures the arithmetic: encode, verify, and
 * decode at e = 0, 8 and 16, and it found that syndrome computation
 * dominates. Repeating that here would be two files timing one kernel.
 *
 * What only this component can answer is what wrapping the code in an object
 * costs, and there are exactly two such numbers:
 *
 *   create[J=8]      the TABLE BUILD -- exp/log over the field plus g(x),
 *   create[J=4]      which `rs_init` does once per constructor. Nothing had
 *                    measured it, and it is the number that decides whether
 *                    a caller may construct per frame or must hoist. Two
 *                    field widths, because the tables are O(2^J) and a
 *                    single row cannot show that.
 *
 *   encode           the object's whole-codeword encode...
 *   encode_kernel    ...against `rs_encode` alone, on the same code. The
 *                    difference IS the memcpy of k information symbols that
 *                    turns parity into a codeword -- the object's one
 *                    addition to the encode path, and the reason
 *                    `rs_encode` stays exposed for a frame assembler that
 *                    has already placed the information.
 *
 * `decode` is here for one reason the family benchmark cannot cover: it is
 * the method the binding calls in place, on the caller's own buffer, so this
 * is the row a Python-side number is comparable against.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided.
 */
#include "jm_bench.h"
#include "rs_codec/rs_codec_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* CCSDS's RS(255,223) numbers, written out locally for the reason
   bench_rs_core.c gives: this measures the object, not the standard. */
#define NROOTS 32u
#define POLY 0x87u
#define J0 112u
#define STRIDE 11u

#define CODEWORDS 256
#define ITERATIONS 100
#define CREATES 200

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };
  volatile long   sink   = 0;

  rs_codec_state_t *rs = rs_codec_create (NROOTS, 8u, POLY, J0, STRIDE);
  if (!rs)
    return 1;
  const size_t n = rs_codec_get_n (rs), k = rs_codec_get_k (rs),
               e = rs_codec_get_e (rs);

  uint8_t *info  = malloc (k);
  uint8_t *clean = malloc (n);
  uint8_t *work  = malloc (n);
  uint8_t *par   = malloc (NROOTS);
  if (!info || !clean || !work || !par)
    return 1;

  uint32_t lfsr = 0xBEEFu;
  for (size_t i = 0; i < k; i++)
    {
      lfsr    = lfsr * 1664525u + 1013904223u;
      info[i] = (uint8_t)(lfsr >> 24);
    }
  if (rs_codec_encode (rs, info, k, clean, n) != n)
    return 1;

  printf ("=== rs_codec benchmark ===\n");
  printf ("RS(%zu,%zu) over GF(256), E = %zu\n\n", n, k, e);

  /* ── the table build ─────────────────────────────────────────────────── */
  {
    static double t_c8[ITERATIONS], t_c4[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < CREATES; i++)
          {
            rs_codec_state_t *s
                = rs_codec_create (NROOTS, 8u, POLY, J0, STRIDE);
            sink += (s != NULL);
            rs_codec_destroy (s);
          }
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t_c8[r] = elapsed_sec (&t0, &t1);

        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < CREATES; i++)
          {
            rs_codec_state_t *s = rs_codec_create (4u, 4u, 0x3u, 1u, 1u);
            sink += (s != NULL);
            rs_codec_destroy (s);
          }
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t_c4[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "create[J=8]", t_c8, ITERATIONS, CREATES);
    jm_bench_add (&_bench, "create[J=4]", t_c4, ITERATIONS, CREATES);
    printf ("  %-16s %8.2f us/call\n", "create[J=8]",
            min_sec (t_c8, ITERATIONS) / CREATES * 1e6);
    printf ("  %-16s %8.2f us/call\n", "create[J=4]",
            min_sec (t_c4, ITERATIONS) / CREATES * 1e6);
  }

  /* ── the object's encode, against the kernel it wraps ────────────────── */
  {
    static double t_obj[ITERATIONS], t_ker[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < CODEWORDS; i++)
          sink += (long)rs_codec_encode (rs, info, k, work, n);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t_obj[r] = elapsed_sec (&t0, &t1);

        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < CODEWORDS; i++)
          {
            rs_encode (&rs->rs, info, par);
            sink += par[0];
          }
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t_ker[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "encode", t_obj, ITERATIONS, CODEWORDS);
    jm_bench_add (&_bench, "encode_kernel", t_ker, ITERATIONS, CODEWORDS);

    const double so = min_sec (t_obj, ITERATIONS) / CODEWORDS;
    const double sk = min_sec (t_ker, ITERATIONS) / CODEWORDS;
    printf ("  %-16s %8.2f us/codeword\n", "encode", so * 1e6);
    printf ("  %-16s %8.2f us/codeword   (placement costs %.1f%%)\n",
            "encode_kernel", sk * 1e6, 100.0 * (so - sk) / sk);
  }

  /* ── decode, in place, as the binding calls it ───────────────────────── */
  {
    static double t_d0[ITERATIONS], t_de[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < CODEWORDS; i++)
          {
            memcpy (work, clean, n);
            sink += rs_codec_decode (rs, work, n);
          }
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t_d0[r] = elapsed_sec (&t0, &t1);

        clock_gettime (CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < CODEWORDS; i++)
          {
            memcpy (work, clean, n);
            for (size_t j = 0; j < e; j++)
              work[j * 7u] ^= 0xA5u;
            sink += rs_codec_decode (rs, work, n);
          }
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t_de[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "decode[e=0]", t_d0, ITERATIONS, CODEWORDS);
    jm_bench_add (&_bench, "decode[e=E]", t_de, ITERATIONS, CODEWORDS);
    printf ("  %-16s %8.2f us/codeword\n", "decode[e=0]",
            min_sec (t_d0, ITERATIONS) / CODEWORDS * 1e6);
    printf ("  %-16s %8.2f us/codeword\n", "decode[e=E]",
            min_sec (t_de, ITERATIONS) / CODEWORDS * 1e6);
  }

  jm_bench_write_json (&_bench, "rs_codec");
  free (info);
  free (clean);
  free (work);
  free (par);
  rs_codec_destroy (rs);
  (void)sink;
  return 0;
}
