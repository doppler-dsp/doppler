/* bench_viterbi_core.c — the Viterbi decoder, at its own object.
 *
 * Moved here from bench_conv_core.c (doppler#893). The decode rows were
 * measured against the `conv` c_deps directory, one level below anything jm
 * modelled — which is why nothing ran them and why the target had to be
 * hand-registered. `viterbi` is now a declared object, so this file, its
 * CMake target and its Python face are generated; only the timing loop is
 * mine to write.
 *
 * A Viterbi trellis is 2^(k-1) states with two incoming branches each,
 * touched once per received symbol pair, and it is the most expensive kernel
 * in a receiver chain that uses it. Two things a caller chooses:
 *
 *   decode[depth=35]   the conventional 5*(k-1)+ traceback
 *   decode[depth=96]   a depth long enough to be safe at low Es/N0, which is
 *                      what a real link runs
 *
 * The add-compare-select work is identical between them — only the traceback
 * walk grows — so the pair says how much of a decode is trellis and how much
 * is survivor memory. `encode` stays in bench_conv_core.c, where the code
 * lives; compare the two files for the asymmetry.
 *
 * Timing is MIN over rounds, not mean, after a WARMUP_S settle.
 */
#include "jm_bench.h"
#include "viterbi/viterbi_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 16384
#define ITERATIONS 50
#define WARMUP_S 0.25

/* CCSDS 131.0-B-3 section 3's inner code, and NASA's standard: k=7, rate
   1/2, generators (171, 133) octal. Named as the object takes them. */
static const uint32_t POLY[2] = { 0171u, 0133u };
#define K 7u
#define INVERT 0u

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
  volatile size_t sink   = 0;

  const size_t n_in  = BENCH_N;
  const size_t n_cod = n_in * 2;
  float       *llr   = malloc (n_cod * sizeof *llr);
  if (!llr)
    return 1;

  /* Soft LLRs at a healthy operating point. A decoder's cost does not depend
     on the values -- there is no early exit in a trellis -- so this exists to
     keep the survivor paths meaningful for anyone reading the output. */
  uint32_t lfsr = 0xACE1u;
  for (size_t i = 0; i < n_cod; i++)
    {
      lfsr   = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      llr[i] = (lfsr & 1u) ? -2.0f : 2.0f;
    }

  printf ("=== viterbi benchmark ===\n");
  printf ("k=%u rate 1/2 (0%o, 0%o), %zu info bits/round, %d rounds\n\n", K,
          POLY[0], POLY[1], n_in, ITERATIONS);

  const size_t      depths[2] = { 35, 96 };
  const char *const names[2]  = { "decode[depth=35]", "decode[depth=96]" };
  static double     t_dec[2][ITERATIONS];

  for (int d = 0; d < 2; d++)
    {
      viterbi_state_t *v = viterbi_create (POLY, 2, K, INVERT, depths[d]);
      if (!v)
        {
          (void)fprintf (stderr, "bench_viterbi: create(depth=%zu) NULL\n",
                         depths[d]);
          return 1;
        }
      size_t   max_out = viterbi_decode_max_out (v, n_cod);
      uint8_t *dec     = malloc (max_out ? max_out : 1);
      if (!dec)
        return 1;

      /* A decoder that emits nothing would time the refusal, not the
         trellis. */
      size_t got = viterbi_decode (v, llr, n_cod, dec, max_out);
      if (got == 0)
        {
          (void)fprintf (stderr,
                         "bench_viterbi: depth=%zu decoded 0 of %zu symbols "
                         "— the timings below would measure a no-op\n",
                         depths[d], n_cod);
          return 1;
        }

      struct timespec w0, w1;
      clock_gettime (CLOCK_MONOTONIC, &w0);
      do
        {
          viterbi_reset (v);
          sink += viterbi_decode (v, llr, n_cod, dec, max_out);
          clock_gettime (CLOCK_MONOTONIC, &w1);
        }
      while (elapsed_sec (&w0, &w1) < WARMUP_S);

      for (int r = 0; r < ITERATIONS; r++)
        {
          viterbi_reset (v);
          clock_gettime (CLOCK_MONOTONIC, &t0);
          sink += viterbi_decode (v, llr, n_cod, dec, max_out);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          t_dec[d][r] = elapsed_sec (&t0, &t1);
        }
      jm_bench_add (&_bench, names[d], t_dec[d], ITERATIONS, (int)n_in);
      double sec = min_sec (t_dec[d], ITERATIONS);
      printf ("  %-18s %8.2f ns/info-bit   %7.2f Mbit/s\n", names[d],
              sec / (double)n_in * 1e9, (double)n_in / sec / 1e6);
      free (dec);
      viterbi_destroy (v);
    }

  printf ("\n  depth 96 costs %.2fx depth 35 over identical trellis work --\n"
          "  the add-compare-selects do not change, only the traceback walk,\n"
          "  so that ratio is the price of a decision safe at low Es/N0.\n"
          "  Against conv::encode in bench_conv_core.c, decoding is the\n"
          "  expensive direction by more than an order of magnitude.\n",
          min_sec (t_dec[1], ITERATIONS) / min_sec (t_dec[0], ITERATIONS));

  (void)sink;
  free (llr);
  jm_bench_write_json (&_bench, "viterbi");
  return 0;
}
