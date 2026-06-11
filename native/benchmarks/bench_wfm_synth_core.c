/* bench_wfm_synth_core.c — synth engine throughput (MSa/s) per waveform type.
 *
 * Covers the two axes that dominate cost: AWGN (snr >= 100 dB is "clean" → no
 * noise generated) and the LO (freq 0 → baseband, no NCO). So each type is
 * benched clean vs +noise, and the LFSR types are benched at baseband (the raw
 * bit-manipulation path). Emits pytest-benchmark-compatible JSON via make
 * bench. */
#include "jm_bench.h"
#include "wfm_synth/wfm_synth_core.h"
#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

/* Bench wfm_synth_steps for one configuration; print MSa/s and record JSON.
 * snr >= 100 ⇒ clean (no AWGN); freq == 0 ⇒ baseband (no LO). */
static void
bench_cfg (const char *name, int type, int sps, int pnlen, int lfsr,
           double snr, double freq, float complex *out, jm_bench_t *bench)
{
  wfm_synth_state_t *obj = wfm_synth_create (type, 1e6, freq, snr, 0, 1, sps,
                                             pnlen, 0, lfsr, 0.0);
  if (!obj)
    {
      printf ("  %-26s   (create failed)\n", name);
      return;
    }
  wfm_synth_steps (obj, out, BENCH_N); /* warm up */

  struct timespec t0, t1;
  double          times[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      clock_gettime (CLOCK_MONOTONIC, &t0);
      wfm_synth_steps (obj, out, BENCH_N);
      clock_gettime (CLOCK_MONOTONIC, &t1);
      times[r] = elapsed_sec (&t0, &t1);
    }
  double mean = 0.0;
  for (int r = 0; r < ITERATIONS; r++)
    mean += times[r];
  mean /= ITERATIONS;
  double msas = (double)BENCH_N / mean / 1e6;
  printf ("  %-26s %8.1f MSa/s  (%.2f GSa/s)\n", name, msas, msas / 1000.0);
  jm_bench_add (bench, name, times, ITERATIONS, BENCH_N);

  wfm_synth_destroy (obj);
}

int
main (void)
{
  float complex *out = malloc (BENCH_N * sizeof (float complex));
  if (!out)
    {
      fprintf (stderr, "OOM\n");
      return 1;
    }

  jm_bench_t bench = { 0 };
  printf ("=== synth benchmark ===\n");
  printf ("block = %d samples, %d iterations\n", BENCH_N, ITERATIONS);
  printf ("snr 100 = clean (no AWGN); freq 0 = baseband (no LO)\n\n");

  /*        name                       type sps   n  lfsr   snr   freq */
  bench_cfg ("noise (AWGN)", 1, 8, 7, 0, 20.0, 0.0, out, &bench);
  bench_cfg ("tone  clean", 0, 8, 7, 0, 100.0, 1e5, out, &bench);
  bench_cfg ("tone  +noise", 0, 8, 7, 0, 20.0, 1e5, out, &bench);
  bench_cfg ("pn    baseband clean", 2, 1, 23, 0, 100.0, 0.0, out, &bench);
  bench_cfg ("pn    +LO +noise", 2, 1, 23, 0, 20.0, 1e5, out, &bench);
  bench_cfg ("pn    n=40 baseband(64b)", 2, 1, 40, 0, 100.0, 0.0, out, &bench);
  bench_cfg ("pn    fibonacci baseband", 2, 1, 23, 1, 100.0, 0.0, out, &bench);
  bench_cfg ("bpsk  clean", 3, 8, 7, 0, 100.0, 1e5, out, &bench);
  bench_cfg ("bpsk  +noise", 3, 8, 7, 0, 20.0, 1e5, out, &bench);
  bench_cfg ("qpsk  clean", 4, 8, 7, 0, 100.0, 1e5, out, &bench);
  bench_cfg ("qpsk  +noise", 4, 8, 7, 0, 20.0, 1e5, out, &bench);

  jm_bench_write_json (&bench, "synth");
  free (out);
  return 0;
}
