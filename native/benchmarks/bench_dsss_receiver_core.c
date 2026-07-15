/* bench_dsss_receiver_core.c — the composed receiver's two regimes.
 *
 *   search   — throughput while feeding the embedded Acquisition alone
 *              (2-D correlation/FFT cost, no despread/resample/demod).
 *   track    — throughput once locked: Dll -> RateConverter -> MpskReceiver
 *              per sample, the C-level equivalent of async_dsss_receiver_
 *              demo.py's _receive() loop.
 *
 * These have very different per-sample costs; reporting both separately
 * (rather than one blended number) is the point.
 */
#include "dsss_receiver/dsss_receiver_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 50

static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

static float complex
cgauss (uint32_t *st)
{
  *st ^= *st << 13;
  *st ^= *st >> 17;
  *st ^= *st << 5;
  uint32_t a = *st;
  *st ^= *st << 13;
  *st ^= *st >> 17;
  *st ^= *st << 5;
  uint32_t b   = *st;
  double   u1  = ((double)a + 1.0) / 4294967297.0;
  double   u2  = ((double)b + 1.0) / 4294967297.0;
  double   mag = sqrt (-log (u1));
  double   th  = 6.283185307179586 * u2;
  return (float)(mag * cos (th)) + (float)(mag * sin (th)) * I;
}

int
main (void)
{
  jm_bench_t      _bench = { 0 };
  struct timespec t0, t1;
  double          times[ITERATIONS];

  printf ("=== dsss_receiver benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

  /* ── search: feed silence, never locks, pure Acquisition cost ─────────── */
  {
    dsss_receiver_state_t *rx
        = dsss_receiver_create (CODE7, 7, 1.0e6, 35714.29, 4, 2, 45.0, 1e-2,
                                0.9, 500.0, 8, 4, 4, 8, 0);
    float complex *x   = calloc (BENCH_N, sizeof *x);
    float complex *out = malloc (BENCH_N * sizeof *out);
    dsss_receiver_steps (rx, x, BENCH_N, out, BENCH_N); /* warmup */
    for (int r = 0; r < ITERATIONS; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        dsss_receiver_steps (rx, x, BENCH_N, out, BENCH_N);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        times[r] = elapsed_sec (&t0, &t1);
      }
    jm_bench_add (&_bench, "search", times, ITERATIONS, BENCH_N);
    double sum = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      sum += times[r];
    printf ("  search   %8.1f MSa/s (tracking=%d)\n",
            (double)BENCH_N / (sum / ITERATIONS) / 1e6,
            dsss_receiver_get_tracking (rx));
    free (x);
    free (out);
    dsss_receiver_destroy (rx);
  }

  /* ── track: acquire once on a real signal, then benchmark steady-state
   * despread/resample/demod throughput on further blocks of the same
   * continuous signal ────────────────────────────────────────────────── */
  {
    const size_t sf = 7, spc = 4;
    const double fs          = 1.0e6 * (double)spc;
    const double sym_rate    = 35714.29;
    const double tsym        = fs / sym_rate;
    const size_t te          = sf * spc;
    const size_t pre_silence = te * 5 + 3;
    const size_t n_track     = BENCH_N * (size_t)(ITERATIONS + 2);
    const size_t total       = pre_silence + n_track;

    float csign[7];
    for (size_t i = 0; i < sf; i++)
      csign[i] = CODE7[i] & 1 ? -1.0f : 1.0f;

    uint32_t       st      = 7;
    float complex *x       = calloc (total, sizeof *x);
    double         amp_snr = sqrt (pow (10.0, 90.0 / 10.0) / fs);
    double         sigma   = 1.0 / amp_snr;
    for (size_t i = 0; i < total; i++)
      x[i] = (float complex) (sigma / sqrt (2.0)) * cgauss (&st);
    for (size_t idx = 0; idx < n_track; idx++)
      {
        double si  = (((size_t)((double)idx / tsym)) % 2 == 0) ? 1.0 : -1.0;
        size_t cph = (idx / spc) % sf;
        x[pre_silence + idx] += (float)si * csign[cph];
      }

    dsss_receiver_state_t *rx
        = dsss_receiver_create (CODE7, sf, 1.0e6, sym_rate, spc, 2, 45.0, 1e-2,
                                0.9, 500.0, 8, 4, 4, 8, 0);
    float complex *out = malloc (BENCH_N * sizeof *out);
    size_t         pos = 0;
    /* stream until locked (search cost not timed) */
    while (!dsss_receiver_get_tracking (rx) && pos + te <= total)
      {
        dsss_receiver_steps (rx, x + pos, te, out, BENCH_N);
        pos += te;
      }
    printf ("  (locked after %zu samples, tracking=%d)\n", pos,
            dsss_receiver_get_tracking (rx));

    for (int r = 0; r < ITERATIONS && pos + BENCH_N <= total; r++)
      {
        clock_gettime (CLOCK_MONOTONIC, &t0);
        dsss_receiver_steps (rx, x + pos, BENCH_N, out, BENCH_N);
        clock_gettime (CLOCK_MONOTONIC, &t1);
        times[r] = elapsed_sec (&t0, &t1);
        pos += BENCH_N;
      }
    jm_bench_add (&_bench, "track", times, ITERATIONS, BENCH_N);
    double sum = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      sum += times[r];
    printf ("  track    %8.1f MSa/s (tracking=%d)\n",
            (double)BENCH_N / (sum / ITERATIONS) / 1e6,
            dsss_receiver_get_tracking (rx));

    free (x);
    free (out);
    dsss_receiver_destroy (rx);
  }

  jm_bench_write_json (&_bench, "dsss_receiver");
  return 0;
}
