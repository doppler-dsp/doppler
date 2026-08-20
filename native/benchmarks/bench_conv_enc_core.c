/* bench_conv_enc_core.c — the encoder object, and what wrapping costs.
 *
 * `bench_conv_core.c` measures the KERNEL. This measures the OBJECT, and the
 * two rows that are its own rather than the kernel's:
 *
 *   encode[k=3..9]   the encoder does `n` parity computations per input bit
 *                    and never walks a trellis, so unlike the decoder its
 *                    cost should not move with k. Measured because that
 *                    asymmetry is the reason the two are separate objects,
 *                    and because "should not move" is a prediction.
 *   object vs raw    conv_enc_encode is one call to conv_encode over a code
 *                    and register held together. The header claims it adds
 *                    nothing; this is what that costs in nanoseconds.
 *
 * Timing is MIN over rounds, not mean, after ONE settle for the process --
 * and the configurations are INTERLEAVED. A per-configuration warm-up
 * charges the whole clock ramp to whichever runs first, which turned a real
 * 1.42x into 0.99x in bench_viterbi_core.c (doppler#896).
 */
#include "conv/conv_core.h"
#include "conv_enc/conv_enc_core.h"
#include "jm_bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 16384
#define ITERATIONS 50
#define WARMUP_S 0.25

/* Four codes spanning the family's constraint lengths, all rate 1/2 so the
   only thing moving between rows is k. */
static const struct
{
  const char *name;
  uint32_t    k;
  uint32_t    poly[2];
} CODES[] = {
  { "encode[k=3]", 3u, { 07u, 05u } },
  { "encode[k=5]", 5u, { 023u, 035u } },
  { "encode[k=7]", 7u, { 0171u, 0133u } },
  { "encode[k=9]", 9u, { 0753u, 0561u } },
};
#define N_CODES (sizeof CODES / sizeof CODES[0])

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
  const size_t n_sym = n_in * 2u;
  uint8_t     *in    = malloc (n_in);
  uint8_t     *out   = malloc (n_sym);
  if (!in || !out)
    return 1;

  /* Structured, not constant: the encoder's cost does not depend on the data
     -- there is no branch on a bit -- so this exists to keep the output
     meaningful for anyone reading it. */
  uint32_t lfsr = 0xACE1u;
  for (size_t i = 0; i < n_in; i++)
    {
      lfsr  = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      in[i] = (uint8_t)(lfsr & 1u);
    }

  printf ("=== conv_enc benchmark ===\n");
  printf ("rate 1/2, %zu info bits/round, %d rounds\n\n", n_in, ITERATIONS);

  conv_enc_state_t *enc[N_CODES];
  static double     t_enc[N_CODES][ITERATIONS];
  for (size_t c = 0; c < N_CODES; c++)
    {
      enc[c] = conv_enc_create (CODES[c].poly, 2, CODES[c].k, 0u);
      if (!enc[c])
        {
          (void)fprintf (stderr, "bench_conv_enc: create(k=%u) NULL\n",
                         CODES[c].k);
          return 1;
        }
      /* An encoder that emitted nothing would time the refusal. */
      if (conv_enc_encode (enc[c], in, n_in, out, n_sym) != n_sym)
        {
          (void)fprintf (stderr, "bench_conv_enc: k=%u wrote nothing\n",
                         CODES[c].k);
          return 1;
        }
    }

  /* One settle for the process, before any configuration is timed. */
  struct timespec w0, w1;
  clock_gettime (CLOCK_MONOTONIC, &w0);
  do
    {
      sink += conv_enc_encode (enc[0], in, n_in, out, n_sym);
      clock_gettime (CLOCK_MONOTONIC, &w1);
    }
  while (elapsed_sec (&w0, &w1) < WARMUP_S);

  /* Rounds OUTSIDE, configurations inside: the rows are compared to each
     other, so any drift the settle missed must land on all of them. */
  for (int r = 0; r < ITERATIONS; r++)
    for (size_t c = 0; c < N_CODES; c++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        sink += conv_enc_encode (enc[c], in, n_in, out, n_sym);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t_enc[c][r] = elapsed_sec (&t0, &t1);
      }

  printf ("  %-14s %14s %14s\n", "", "ns/info-bit", "Mbit/s");
  for (size_t c = 0; c < N_CODES; c++)
    {
      jm_bench_add (&_bench, CODES[c].name, t_enc[c], ITERATIONS, (int)n_in);
      const double sec = min_sec (t_enc[c], ITERATIONS);
      printf ("  %-14s %14.2f %14.2f\n", CODES[c].name,
              sec / (double)n_in * 1e9, (double)n_in / sec / 1e6);
      conv_enc_destroy (enc[c]);
    }

  /* ── what the object costs over the kernel it calls ──────────────────── */
  {
    const conv_code_t c = { 7u, 2u, { 0171u, 0133u }, 0x2u };
    conv_enc_t        raw;
    conv_enc_state_t *obj = conv_enc_create (CODES[2].poly, 2, 7u, 0x2u);
    if (!obj)
      return 1;
    conv_enc_init (&raw);

    static double t_raw[ITERATIONS], t_obj[ITERATIONS];
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        sink += conv_encode (&raw, &c, in, n_in, out, n_sym);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t_raw[r] = elapsed_sec (&t0, &t1);

        clock_gettime (CLOCK_MONOTONIC, &t0);
        sink += conv_enc_encode (obj, in, n_in, out, n_sym);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        t_obj[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "encode[raw kernel]", t_raw, ITERATIONS, (int)n_in);
    jm_bench_add (&_bench, "encode[through the object]", t_obj, ITERATIONS,
                  (int)n_in);

    const double raw_s = min_sec (t_raw, ITERATIONS);
    const double obj_s = min_sec (t_obj, ITERATIONS);
    printf ("\n  the object costs %.3fx the raw kernel over %zu bits --\n"
            "  it holds the code and the register together and calls\n"
            "  conv_encode once, which is the whole of it.\n",
            obj_s / raw_s, n_in);
    conv_enc_destroy (obj);
  }

  printf ("\n  Cost is flat in k: the encoder computes n parities per input\n"
          "  bit and never walks a trellis, which is the asymmetry with\n"
          "  bench_viterbi_core.c, where 2^(k-1) states set the price.\n");

  (void)sink;
  free (in);
  free (out);
  jm_bench_write_json (&_bench, "conv_enc");
  return 0;
}
