/* bench_carrier_acq_core.c — coarse carrier acquisition, per decision.
 *
 * Acquisition is not a per-sample cost like the tracking loops: steps()
 * folds n_fft-sample blocks into a running PSD and STOPS READING the moment
 * a detection fires, so a call offered 64k samples reads however many
 * blocks the decision needed. Crediting it with all 64k would report
 * samples it never read as throughput -- the 16.7 GSa/s this row once
 * published (doppler#2388) -- so each row is credited with the samples it
 * FOLDED, read back from the object.
 *
 * The stimulus is the Python twin's `detect` case
 * (src/doppler/acquire/benchmarks/bench_carrier_acq.py): rect-pulse BPSK at
 * 4 samples per symbol (1 Msym/s at 4 MSa/s), 44 kHz off centre, at 20 dB
 * Es/N0. The block is FS / 4096 Hz of resolution, 4096 samples, so the
 * folded count is n_blocks * 4096; the default resolution (symbol rate / 10)
 * would make it a 40-sample block.
 *
 * Two rows, the zero-pad's two ends (4 and 16): the pad sets the transform
 * length, next_pow_two(4096 * pad), and so the cost of every look.
 * Sequential mode tests after every block, which is what lets a decision
 * stop early; reset() runs before every timed call, because a detector that
 * has decided reads nothing more.
 *
 * Each timed call must decide, decide right (within two bins of 44 kHz) and
 * stop early (fold fewer than BENCH_N samples), or the bench fails: a
 * detector that times a give-up or a wrong peak is not one worth timing
 * (doppler#1967).
 *
 * Timing is MIN over rounds, not mean, after a WARMUP_S settle.
 */
#include "doppler/carrier_acq/carrier_acq_core.h"
#include "doppler/dp_complex.h"
#include "jm_bench.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 50
#define WARMUP_S 0.25
#define SAMPLE_RATE_HZ 4.0e6
#define SPS 4
#define SYMBOL_RATE_HZ (SAMPLE_RATE_HZ / SPS)
/* One block is 4096 samples, so BENCH_N is 16 looks. */
#define N_FFT 4096
#define RESOLUTION_HZ (SAMPLE_RATE_HZ / N_FFT)
#define OFFSET_HZ 44000.0
#define ESN0_DB 20.0
/* The API defaults (objects/carrier_acq.toml). */
#define PFA 1e-3
#define PD 0.9
#define DESIGN_SNR 2.0
#define MAX_N_BLOCKS 100000

static double
min_sec (const double *t, int n)
{
  double m = t[0];
  for (int r = 1; r < n; r++)
    if (t[r] < m)
      m = t[r];
  return m;
}

/* Fail unless the call just timed decided, near 44 kHz, before the end. */
static int
check_decision (const dp_carrier_acq_state_t *ca, size_t pad)
{
  double err = fabs (ca->residual_hz - OFFSET_HZ);
  if (!ca->ready || err >= 2.0 * RESOLUTION_HZ
      || ca->n_blocks * N_FFT >= BENCH_N)
    {
      (void)fprintf (stderr,
                     "bench_carrier_acq: pad=%zu: ready=%d, residual %.0f Hz "
                     "(%.0f Hz off), %zu blocks folded -- a give-up, a wrong "
                     "peak or a search is not a decision\n",
                     pad, (int)ca->ready, ca->residual_hz, err, ca->n_blocks);
      return 1;
    }
  return 0;
}

int
main (void)
{
  uint64_t   t0, t1;
  jm_bench_t _bench = { 0 };

  float _Complex *x = malloc (BENCH_N * sizeof *x);
  if (!x)
    return 1;

  /* Unit-power rect BPSK: a +/-1 symbol held SPS samples, turned to the
     offset. Es is SPS samples of unit power, so 20 dB Es/N0 is
     20 - 10 log10(SPS) dB over fs, the conversion wfm_synth makes for a
     BPSK source (wfm_synth_snr_over_fs, mode 3), and each rail's sigma is
     sqrt(1 / (2 * snr)), dp_awgn_amplitude_for_snr's convention. The noise
     is drawn here (xorshift32, Box-Muller) rather than by dp_awgn: a
     test_only dependency would leak onto every dependent's PUBLIC link
     line (just-buildit/just-makeit#2318). */
  const double snr_fs
      = pow (10.0, (ESN0_DB - 10.0 * log10 ((double)SPS)) / 10.0);
  const double sigma = sqrt (1.0 / (2.0 * snr_fs));
  uint32_t     st    = 0x9E37u;
  uint32_t     lfsr  = 0x31A7u;
  double       s     = 1.0;
  for (int i = 0; i < BENCH_N; i++)
    {
      if (i % SPS == 0)
        {
          lfsr = (lfsr >> 1) ^ (uint32_t)(-(int32_t)(lfsr & 1u) & 0xB400u);
          s    = (lfsr & 1u) ? -1.0 : 1.0;
        }
      st ^= st << 13;
      st ^= st >> 17;
      st ^= st << 5;
      double u1 = ((double)st + 1.0) / 4294967297.0;
      st ^= st << 13;
      st ^= st >> 17;
      st ^= st << 5;
      double u2 = ((double)st + 1.0) / 4294967297.0;
      double r  = sigma * sqrt (-2.0 * log (u1));
      double th = 2.0 * M_PI * u2;
      double p  = 2.0 * M_PI * OFFSET_HZ / SAMPLE_RATE_HZ * (double)i;
      x[i]      = (float)(s * cos (p) + r * cos (th))
                  + (float)(s * sin (p) + r * sin (th)) * I;
    }

  printf ("=== carrier_acq benchmark ===\n");
  printf ("fs = %.1f MHz, Rs = %.1f MHz, %.0f kHz off, %.0f dB Es/N0, "
          "%d offered, %d rounds\n\n",
          SAMPLE_RATE_HZ / 1e6, SYMBOL_RATE_HZ / 1e6, OFFSET_HZ / 1e3, ESN0_DB,
          BENCH_N, ITERATIONS);

  const size_t  pads[2] = { 4, 16 };
  static double t_dt[2][ITERATIONS];

  for (int p = 0; p < 2; p++)
    {
      dp_carrier_acq_state_t *ca = dp_carrier_acq_create (
          SAMPLE_RATE_HZ, SYMBOL_RATE_HZ, RESOLUTION_HZ, pads[p],
          DP_PSD_WINDOW_HANN, 0.0f, NULL, 0, PFA, PD, DESIGN_SNR, true,
          MAX_N_BLOCKS);
      if (!ca)
        {
          (void)fprintf (stderr,
                         "bench_carrier_acq: create(pad=%zu) returned NULL\n",
                         pads[p]);
          return 1;
        }

      uint64_t w0, w1;
      w0 = jm_bench_now_ns ();
      do
        {
          dp_carrier_acq_reset (ca);
          dp_carrier_acq_steps (ca, x, BENCH_N);
          w1 = jm_bench_now_ns ();
        }
      while (jm_bench_elapsed_sec (w0, w1) < WARMUP_S);

      /* Every round decides on the same input, so it folds the same count;
         checked per round all the same, since that is the count credited. */
      size_t folded = 0;
      for (int r = 0; r < ITERATIONS; r++)
        {
          dp_carrier_acq_reset (ca);
          t0 = jm_bench_now_ns ();
          dp_carrier_acq_steps (ca, x, BENCH_N);
          t1         = jm_bench_now_ns ();
          t_dt[p][r] = jm_bench_elapsed_sec (t0, t1);
          if (check_decision (ca, pads[p]))
            return 1;
          if (r > 0 && ca->n_blocks * N_FFT != folded)
            {
              (void)fprintf (stderr,
                             "bench_carrier_acq: pad=%zu: round %d folded "
                             "%zu samples, round 0 %zu\n",
                             pads[p], r, ca->n_blocks * N_FFT, folded);
              return 1;
            }
          folded = ca->n_blocks * N_FFT;
        }
      char name[64];
      (void)snprintf (name, sizeof name, "detect[pad=%zu]", pads[p]);
      jm_bench_add_unit (&_bench, name, t_dt[p], ITERATIONS, folded,
                         JM_BENCH_SAMPLE);
      double sec = min_sec (t_dt[p], ITERATIONS);
      printf ("  %-16s %8.1f us/decision  %zu blocks  %7.2f ns/sample\n", name,
              sec * 1e6, folded / N_FFT, sec / (double)folded * 1e9);
      dp_carrier_acq_destroy (ca);
    }

  printf ("\n  Paid once per capture (and again after a loss of lock), so it\n"
          "  belongs in a time-to-first-lock budget beside the loops'\n"
          "  settling time, not in the per-sample throughput the tracking\n"
          "  rows report.\n");

  free (x);
  jm_bench_write_json (&_bench, "carrier_acq");
  return 0;
}
