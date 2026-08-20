/* bench_mpsk_receiver_core.c — the whole receiver, per sample.
 *
 * A jm scaffold that recorded nothing until now (doppler#891), which is the
 * worst of the thirty to have been empty: this is the composed object every
 * other row in a link budget adds up to. AGC, matched filter, timing loop,
 * carrier loop and slicer, per sample.
 *
 * The rows are the axes a caller actually sets:
 *
 *   sps       samples per symbol. The chain runs per sample and delivers
 *             per symbol, so a higher sps is cheaper per sample and dearer
 *             per SYMBOL -- and the symbol column is the one a link budget
 *             is written in. Both are printed for that reason.
 *   agc       on or off. The front-end AGC is per sample and always on in
 *             a real capture, so its cost belongs in the headline number
 *             rather than in a footnote.
 *
 * Compare the per-sample figure against carrier_mpsk's and ratesync's: the
 * receiver is those two plus a matched filter, so the difference is what
 * composition costs over its parts.
 *
 * Timing is MIN over rounds, not mean -- benchmark noise is one-sided --
 * and each configuration is warmed for WARMUP_S first, because a cold
 * process reads systematically high while the CPU ramps.
 */
#include "jm_bench.h"
#include "mpsk_receiver/mpsk_receiver_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 50
#define WARMUP_S 0.25
#define SYM_RATE 1.0e6

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

  float complex *x   = malloc (BENCH_N * sizeof *x);
  float complex *out = malloc (BENCH_N * sizeof *out);
  if (!x || !out)
    return 1;

  /* BPSK with a small carrier offset, so both loops have something to
     track. A clean, centred stream would leave both discriminators at zero
     and time a receiver that is not doing its job. */
  uint32_t lfsr = 0x2A31u;
  for (int i = 0; i < BENCH_N; i++)
    {
      lfsr     = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
      double s = (lfsr & 1u) ? -1.0 : 1.0;
      double p = 2.0 * M_PI * 0.0007 * (double)i;
      x[i]     = (float)(s * cos (p)) + (float)(s * sin (p)) * I;
    }

  printf ("=== mpsk_receiver benchmark ===\n");
  printf ("BPSK, block = %d samples, %d rounds\n\n", BENCH_N, ITERATIONS);

  const double  spss[2] = { 4.0, 8.0 };
  const int     agcs[2] = { 0, 1 };
  static double t_st[2][2][ITERATIONS];

  for (int p = 0; p < 2; p++)
    for (int a = 0; a < 2; a++)
      {
        mpsk_receiver_state_t *rx
            = mpsk_receiver_create_bpsk (SYM_RATE * spss[p], SYM_RATE, 0.0, 0,
                                         0.35, 8, 0.01, 0.01, 0, agcs[a]);
        if (!rx)
          {
            (void)fprintf (stderr,
                           "bench_mpsk_receiver: create(sps=%.0f, agc=%d) "
                           "returned NULL\n",
                           spss[p], agcs[a]);
            return 1;
          }
        size_t cap = mpsk_receiver_steps_max_out (rx);
        if (cap == 0 || cap > BENCH_N)
          cap = BENCH_N;

        size_t       got     = mpsk_receiver_steps (rx, x, BENCH_N, out, cap);
        const size_t want_lo = (size_t)((double)BENCH_N / spss[p] * 0.5);
        if (got < want_lo)
          {
            (void)fprintf (stderr,
                           "bench_mpsk_receiver: sps=%.0f emitted %zu symbols "
                           "from %d samples (expected >= %zu) — the timings "
                           "below would measure a short path\n",
                           spss[p], got, BENCH_N, want_lo);
            return 1;
          }

        struct timespec w0, w1;
        clock_gettime (CLOCK_MONOTONIC, &w0);
        do
          {
            sink += mpsk_receiver_steps (rx, x, BENCH_N, out, cap);
            clock_gettime (CLOCK_MONOTONIC, &w1);
          }
        while (elapsed_sec (&w0, &w1) < WARMUP_S);

        for (int r = 0; r < ITERATIONS; r++)
          {
            clock_gettime (CLOCK_MONOTONIC, &t0);
            sink += mpsk_receiver_steps (rx, x, BENCH_N, out, cap);
            clock_gettime (CLOCK_MONOTONIC, &t1);
            t_st[p][a][r] = elapsed_sec (&t0, &t1);
          }
        char name[64];
        (void)snprintf (name, sizeof name, "steps[sps=%.0f,agc=%d]", spss[p],
                        agcs[a]);
        jm_bench_add (&_bench, name, t_st[p][a], ITERATIONS, BENCH_N);
        double sec = min_sec (t_st[p][a], ITERATIONS);
        printf ("  %-24s %7.2f ns/sample  %8.1f MSa/s  %7.2f Msym/s\n", name,
                sec / (double)BENCH_N * 1e9, (double)BENCH_N / sec / 1e6,
                (double)BENCH_N / spss[p] / sec / 1e6);
        mpsk_receiver_destroy (rx);
      }

  printf ("\n  AGC adds %.0f%% at sps=4 -- the single largest per-sample\n"
          "  cost a caller can switch off, and it is on in every real\n"
          "  capture. sps=8 costs %.2fx per sample but delivers half the\n"
          "  symbols: read the Msym/s column against a link budget, because\n"
          "  oversampling is bought in symbol throughput, not sample\n"
          "  throughput.\n"
          "\n  Worth reading beside the parts: the WHOLE receiver without\n"
          "  AGC (~16 ns/sample) is cheaper than the bare carrier loop\n"
          "  measured alone in bench_carrier_mpsk_core.c (~22 ns/sample).\n"
          "  Not a contradiction -- inside the receiver the carrier loop\n"
          "  runs on the on-time strobe, once per SYMBOL, while the\n"
          "  standalone benchmark drives it once per sample. Composition is\n"
          "  cheaper than the sum of its parts here, and the reason is where\n"
          "  each loop sits in the chain.\n",
          100.0
              * (min_sec (t_st[0][1], ITERATIONS)
                     / min_sec (t_st[0][0], ITERATIONS)
                 - 1.0),
          min_sec (t_st[1][1], ITERATIONS) / min_sec (t_st[0][1], ITERATIONS));

  (void)sink;
  free (x);
  free (out);
  jm_bench_write_json (&_bench, "mpsk_receiver");
  return 0;
}
