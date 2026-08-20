/* bench_conv_core.c — convolutional ENCODING, at the code.
 *
 * The two directions of this component differ in cost by orders of
 * magnitude, and the asymmetry is the whole point of measuring it. Encoding
 * is a shift register and a parity lookup per bit. Decoding is a Viterbi
 * trellis: 2^(k-1) states, each with two incoming branches, every one of
 * them touched per received symbol pair. At the CCSDS k=7 rate-1/2 code
 * that is 64 states x 2 branches x n_bits add-compare-selects, and it is
 * the most expensive kernel in a receiver chain that uses it.
 *
 * So this measures `conv_encode` over a block of information bits: a shift
 * register and a parity lookup per bit. The DECODER moved to
 * bench_viterbi_core.c when `viterbi` became a declared object -- read the
 * two together for the asymmetry, which is more than an order of magnitude.
 *
 * Timing is MIN over rounds, not mean: a microbenchmark's noise is one-sided
 * -- an interrupt or a migration only ever adds time -- so the minimum is the
 * least-biased estimate and the mean reports whatever else the machine did.
 */
#include "conv/conv_core.h"
#include "jm_bench.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Information bits per round. Smaller than the library-wide 65536 because a
   64-state trellis over 2*N symbols is ~2.1M ACS operations at this size
   already -- enough that per-call overhead is invisible, without making one
   round take long enough that the machine's state drifts inside it. */
#define BENCH_N 16384
#define ITERATIONS 50

/* Traceback depths: the conventional 5*(k-1)+ rule of thumb, and a long one.
 */
#define DEPTH_SHORT 35
#define DEPTH_LONG 96

static const conv_code_t CODE
    = { .k = 7, .n = 2, .poly = { 0171u, 0133u }, .invert = 0u };

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

/* ns per INFORMATION bit, which is the unit a link budget is quoted in --
   the coded-bit rate is twice this and would flatter the decoder by 2x. */
static void
report (const char *name, const double *t, size_t bits)
{
  double s = min_sec (t, ITERATIONS);
  printf ("  %-18s %8.2f ns/info-bit   %7.2f Mbit/s\n", name,
          s / (double)bits * 1e9, (double)bits / s / 1e6);
}

int
main (void)
{
  struct timespec t0, t1;
  jm_bench_t      _bench = { 0 };

  const size_t n_in  = BENCH_N;
  const size_t n_cod = n_in * CODE.n;

  uint8_t *in  = malloc (n_in);
  uint8_t *cod = malloc (n_cod);
  if (!in || !cod)
    return 1;

  /* A deterministic, non-trivial bit pattern. The trellis does the same work
     whatever the data -- there is no early exit in a Viterbi decoder -- so
     this exists to keep the encoder honest rather than to shape the cost. */
  uint32_t lfsr = 0xACE1u;
  for (size_t i = 0; i < n_in; i++)
    {
      lfsr  = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      in[i] = (uint8_t)(lfsr & 1u);
    }

  conv_enc_t enc;
  conv_enc_init (&enc);
  conv_encode (&enc, &CODE, in, n_in, cod, n_cod);

  printf ("=== conv benchmark ===\n");
  printf ("k=%u rate 1/%u (0%o, 0%o), %zu info bits/round, %d rounds\n\n",
          CODE.k, CODE.n, CODE.poly[0], CODE.poly[1], n_in, ITERATIONS);

  /* One settle before any timing, then min over rounds. */
  {
    struct timespec w0, w1;
    clock_gettime (CLOCK_MONOTONIC, &w0);
    do
      {
        conv_enc_init (&enc);
        conv_encode (&enc, &CODE, in, n_in, cod, n_cod);
        clock_gettime (CLOCK_MONOTONIC, &w1);
      }
    while (elapsed_sec (&w0, &w1) < 0.25);
  }

  static double t_enc[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      conv_enc_init (&enc);
      clock_gettime (CLOCK_MONOTONIC, &t0);
      conv_encode (&enc, &CODE, in, n_in, cod, n_cod);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      t_enc[r] = elapsed_sec (&t0, &t1);
    }
  jm_bench_add (&_bench, "encode", t_enc, ITERATIONS, (int)n_in);
  report ("encode", t_enc, n_in);

  printf ("\n  Decoding is measured at its own object, in\n"
          "  bench_viterbi_core.c: `viterbi` is a declared jm component now\n"
          "  (doppler#893), so its benchmark, its CMake target and its\n"
          "  Python face are generated rather than hand-registered here. The\n"
          "  encoder stays with the code it belongs to.\n");

  free (in);
  free (cod);
  jm_bench_write_json (&_bench, "conv");
  return 0;
}
