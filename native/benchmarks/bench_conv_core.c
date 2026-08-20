/* bench_conv_core.c — convolutional coding, both directions.
 *
 * The two directions of this component differ in cost by orders of
 * magnitude, and the asymmetry is the whole point of measuring it. Encoding
 * is a shift register and a parity lookup per bit. Decoding is a Viterbi
 * trellis: 2^(k-1) states, each with two incoming branches, every one of
 * them touched per received symbol pair. At the CCSDS k=7 rate-1/2 code
 * that is 64 states x 2 branches x n_bits add-compare-selects, and it is
 * the most expensive kernel in a receiver chain that uses it.
 *
 * So this measures:
 *
 *   encode              conv_encode over a block of information bits
 *   decode[depth=35]    viterbi_decode at the conventional 5*k traceback
 *   decode[depth=96]    ... and at a depth long enough to be safe at low
 *                       Es/N0, which is what a real link runs
 *
 * Two depths because traceback is the parameter a caller actually chooses,
 * and "how much does a safer depth cost" is not answerable from one number.
 * The ACS work is identical between them -- only the traceback walk grows --
 * so the pair also says how much of the decode is trellis and how much is
 * survivor memory.
 *
 * The code here is the k=7, rate-1/2 (171, 133) octal pair: NASA's standard
 * and CCSDS 131.0-B-3 section 3's inner code. It is declared locally rather
 * than pulled from ccsds_tm on purpose -- conv's CMakeLists deliberately
 * does not link ccsds_tm, because the code family is not CCSDS's (a caller
 * configures it, the way ccsds_tm does).
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
  uint8_t *out = malloc (n_in + 64);
  float   *llr = malloc (n_cod * sizeof *llr);
  if (!in || !cod || !out || !llr)
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

  /* Soft LLRs at a healthy operating point: +/-2.0 with the sign carrying
     the coded bit. A decoder's cost does not depend on the values, but
     feeding it noise-free input keeps the survivor paths meaningful if
     anyone reads the decoded output while debugging this file. */
  for (size_t i = 0; i < n_cod; i++)
    llr[i] = cod[i] ? -2.0f : 2.0f;

  printf ("=== conv benchmark ===\n");
  printf ("k=%u rate 1/%u (0%o, 0%o), %zu info bits/round, %d rounds\n\n",
          CODE.k, CODE.n, CODE.poly[0], CODE.poly[1], n_in, ITERATIONS);

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

  static double     t_d35[ITERATIONS], t_d96[ITERATIONS];
  const unsigned    depths[2] = { DEPTH_SHORT, DEPTH_LONG };
  double *const     slots[2]  = { t_d35, t_d96 };
  const char *const names[2]  = { "decode[depth=35]", "decode[depth=96]" };

  for (int d = 0; d < 2; d++)
    {
      viterbi_state_t *v = viterbi_create (&CODE, depths[d]);
      if (!v)
        return 1;
      size_t   max_out = viterbi_decode_max_out (v, n_cod);
      uint8_t *dec     = malloc (max_out ? max_out : 1);
      if (!dec)
        return 1;
      for (int r = 0; r < ITERATIONS; r++)
        {
          viterbi_reset (v);
          clock_gettime (CLOCK_MONOTONIC, &t0);
          viterbi_decode (v, llr, n_cod, dec, max_out);
          clock_gettime (CLOCK_MONOTONIC, &t1);
          slots[d][r] = elapsed_sec (&t0, &t1);
        }
      jm_bench_add (&_bench, names[d], slots[d], ITERATIONS, (int)n_in);
      report (names[d], slots[d], n_in);
      free (dec);
      viterbi_destroy (v);
    }

  printf ("\n  decode/encode = %.0fx at depth %d. The trellis is 2^(k-1)=%u\n"
          "  states x 2 branches per information bit; the encoder is one\n"
          "  shift and %u parity lookups.\n",
          min_sec (t_d35, ITERATIONS) / min_sec (t_enc, ITERATIONS),
          DEPTH_SHORT, 1u << (CODE.k - 1u), CODE.n);

  free (in);
  free (cod);
  free (out);
  free (llr);
  jm_bench_write_json (&_bench, "conv");
  return 0;
}
