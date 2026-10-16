/* bench_dsss_receiver_core.c — the composed receiver, tracking.
 *
 *   track    — throughput once locked: Dll -> RateConverter -> MpskReceiver
 *              per sample, the C-level equivalent of async_dsss_receiver_
 *              demo.py's _receive() loop.
 *
 * The searching regime is timed from Python
 * (src/doppler/dsss/benchmarks/bench_dsss_receiver.py,
 * test_bench_steps_search), on a 1,023-chip code in real noise. A C search
 * row over a block of zeros on a 7-chip code was cut (doppler#2386): with no
 * noise the detector's reference is zero, which is not the idle case a
 * receiver has.
 */
#include "doppler/dp_complex.h"
#include "doppler/dsss_receiver/dsss_receiver_core.h"
#include "jm_bench.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 50

static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

static float _Complex cgauss (uint32_t *st)
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
  jm_bench_t _bench = { 0 };
  uint64_t   t0, t1;
  double     times[ITERATIONS];

  printf ("=== dsss_receiver benchmark ===\n");
  printf ("block = %d samples,  %d iterations\n\n", BENCH_N, ITERATIONS);

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

    uint32_t        st      = 7;
    float _Complex *x       = calloc (total, sizeof *x);
    double          amp_snr = sqrt (pow (10.0, 90.0 / 10.0) / fs);
    double          sigma   = 1.0 / amp_snr;
    for (size_t i = 0; i < total; i++)
      x[i] = (float _Complex) (sigma / sqrt (2.0)) * cgauss (&st);
    for (size_t idx = 0; idx < n_track; idx++)
      {
        double si  = (((size_t)((double)idx / tsym)) % 2 == 0) ? 1.0 : -1.0;
        size_t cph = (idx / spc) % sf;
        x[pre_silence + idx] += (float)si * csign[cph];
      }

    dp_dsss_receiver_state_t *rx = dp_dsss_receiver_create (
        CODE7, sf, 1.0e6, sym_rate, spc, 2, 45.0, 1e-2, 0.9, 500.0, 4, 8, 0);
    float _Complex *out = malloc (BENCH_N * sizeof *out);
    size_t          pos = 0;
    /* stream until locked (search cost not timed) */
    while (!dp_dsss_receiver_get_tracking (rx) && pos + te <= total)
      {
        dp_dsss_receiver_steps (rx, x + pos, te, out, BENCH_N);
        pos += te;
      }
    printf ("  (locked after %zu samples, tracking=%d)\n", pos,
            dp_dsss_receiver_get_tracking (rx));

    /* Every round must run, and on a locked receiver (#2363). The
       acquisition loop above exits unlocked only once the signal is used
       up, and a round that does not run leaves its `times[]` slot unset,
       so the row would record garbage, or acquisition, as tracking. The
       `pos` test is what keeps every round's reads inside `x`; `!tracking`
       guards the lock itself. The signal has two blocks of slack past
       ITERATIONS for the lock, so failing either means acquisition
       changed -- refuse rather than record. */
    if (!dp_dsss_receiver_get_tracking (rx)
        || pos + (size_t)ITERATIONS * BENCH_N > total)
      {
        (void)fprintf (
            stderr,
            "bench_dsss_receiver: track needs a lock with %d blocks of "
            "signal left; locked=%d after %zu of %zu samples. The row "
            "would record unset timings or acquisition as tracking -- "
            "lengthen n_track or check acquisition\n",
            ITERATIONS, dp_dsss_receiver_get_tracking (rx), pos, total);
        return 1;
      }

    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        dp_dsss_receiver_steps (rx, x + pos, BENCH_N, out, BENCH_N);
        t1       = jm_bench_now_ns ();
        times[r] = jm_bench_elapsed_sec (t0, t1);
        pos += BENCH_N;
      }
    jm_bench_add_unit (&_bench, "track", times, ITERATIONS, BENCH_N,
                       JM_BENCH_SAMPLE);
    double sum = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      sum += times[r];
    printf ("  track    %8.1f MSa/s (tracking=%d)\n",
            (double)BENCH_N / (sum / ITERATIONS) / 1e6,
            dp_dsss_receiver_get_tracking (rx));

    free (x);
    free (out);
    dp_dsss_receiver_destroy (rx);
  }

  jm_bench_write_json (&_bench, "dsss_receiver");
  return 0;
}
