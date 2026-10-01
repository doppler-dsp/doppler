/* bench_wfm_synth_core.c — synth engine throughput (MSa/s) per waveform type.
 *
 * Covers the two axes that dominate cost: AWGN (snr >= 100 dB is "clean" → no
 * noise generated) and the LO (freq 0 → baseband, no NCO). So each type is
 * benched clean vs +noise, and the LFSR types are benched at baseband (the raw
 * bit-manipulation path). Emits pytest-benchmark-compatible JSON via make
 * bench. */
#include "doppler/dp_complex.h"
#include "doppler/wfm/wfm_dsp.h" /* dp_wfm_rrc_taps — RRC pulse-shaping bench */
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "jm_bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BENCH_N 65536
#define ITERATIONS 200

/* Bench dp_wfm_synth_steps for one configuration; print MSa/s and record JSON.
 * snr >= 100 ⇒ clean (no AWGN); freq == 0 ⇒ baseband (no LO). */
static void
bench_cfg (const char *name, int type, int sps, int pnlen, int lfsr,
           double snr, double freq, float _Complex *out, jm_bench_t *bench)
{
  dp_wfm_synth_state_t *obj = dp_wfm_synth_create (type, 1e6, freq, snr, 0, 1,
                                                   sps, pnlen, 0, lfsr, 0.0);
  if (!obj)
    {
      printf ("  %-26s   (create failed)\n", name);
      return;
    }
  dp_wfm_synth_steps (obj, out, BENCH_N); /* warm up */

  uint64_t t0, t1;
  double   times[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      dp_wfm_synth_steps (obj, out, BENCH_N);
      t1       = jm_bench_now_ns ();
      times[r] = jm_bench_elapsed_sec (t0, t1);
    }
  double mean = 0.0;
  for (int r = 0; r < ITERATIONS; r++)
    mean += times[r];
  mean /= ITERATIONS;
  double msas = (double)BENCH_N / mean / 1e6;
  printf ("  %-26s %8.1f MSa/s  (%.2f GSa/s)\n", name, msas, msas / 1000.0);
  jm_bench_add (bench, name, times, ITERATIONS, BENCH_N);

  dp_wfm_synth_destroy (obj);
}

/* Bench dp_wfm_synth_steps with an RRC pulse shaper attached — the polyphase
 * resamp shaper for a power-of-two sps, the dense FIR otherwise. beta 0.35,
 * span 8. Isolates the pulse-shaping cost on top of the bit source. */
static void
bench_cfg_rrc (const char *name, int type, int sps, int pnlen, double snr,
               double freq, float _Complex *out, jm_bench_t *bench)
{
  dp_wfm_synth_state_t *obj = dp_wfm_synth_create (type, 1e6, freq, snr, 0, 1,
                                                   sps, pnlen, 0, 0, 0.0);
  if (!obj)
    {
      printf ("  %-26s   (create failed)\n", name);
      return;
    }
  size_t ntaps = wfm_rrc_ntaps (sps, 8);
  float *taps  = malloc (ntaps * sizeof (float));
  dp_wfm_rrc_taps (0.35, sps, 8, taps);
  dp_wfm_synth_set_rrc (obj, taps, ntaps);
  free (taps);
  dp_wfm_synth_steps (obj, out,
                      BENCH_N); /* warm up (also primes the shaper) */

  uint64_t t0, t1;
  double   times[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      t0 = jm_bench_now_ns ();
      dp_wfm_synth_steps (obj, out, BENCH_N);
      t1       = jm_bench_now_ns ();
      times[r] = jm_bench_elapsed_sec (t0, t1);
    }
  double mean = 0.0;
  for (int r = 0; r < ITERATIONS; r++)
    mean += times[r];
  mean /= ITERATIONS;
  double msas = (double)BENCH_N / mean / 1e6;
  printf ("  %-26s %8.1f MSa/s  (%.2f GSa/s)\n", name, msas, msas / 1000.0);
  jm_bench_add (bench, name, times, ITERATIONS, BENCH_N);

  dp_wfm_synth_destroy (obj);
}

/* Bench a type=bits synth over a set pattern -- the path a framed bpsk/qpsk
 * source and, from #1619, a data source drive. `mod` 0 is the 0/1
 * amplitude line: at sps 1 every sample reads a bit, the tightest per-bit
 * loop the synth has. No source is attached, so this is a set pattern's
 * cost -- the path a frame pulled from a data source must not slow down.
 *
 * A pattern is sent ONCE, then silence (doppler#1718), and silence is a
 * faster loop than bits: a pattern shorter than the block would time the
 * silence. So the pattern covers a whole block, reset() rewinds it outside
 * the timed region, and a block that ran dry aborts the bench rather than
 * reporting the wrong loop's speed. */
static void
bench_cfg_bits (const char *name, int sps, int mod, float _Complex *out,
                jm_bench_t *bench)
{
  dp_wfm_synth_state_t *obj = dp_wfm_synth_create (
      6 /* bits */, 1e6, 0.0, 100.0, 0, 1, sps, 7, 0, 0, 0.0);
  /* every bit one block reads: BENCH_N / sps symbols, `mod` bits each (0,
     the amplitude line, reads one) */
  const size_t n_pat
      = ((size_t)BENCH_N / (size_t)sps + 1u) * (size_t)(mod > 0 ? mod : 1);
  uint8_t *pat = malloc (n_pat);
  for (size_t i = 0, r = 0x5A5u; pat && i < n_pat; i++)
    {
      r      = r * 1103515245u + 12345u;
      pat[i] = (uint8_t)((r >> 16) & 1u);
    }
  if (!obj || !pat || dp_wfm_synth_set_bits (obj, pat, n_pat, mod) != 0)
    {
      printf ("  %-26s   (create failed)\n", name);
      if (obj)
        dp_wfm_synth_destroy (obj);
      free (pat);
      return;
    }
  free (pat);                             /* the synth keeps its own copy */
  dp_wfm_synth_steps (obj, out, BENCH_N); /* warm up */

  uint64_t t0, t1;
  double   times[ITERATIONS];
  for (int r = 0; r < ITERATIONS; r++)
    {
      dp_wfm_synth_reset (obj); /* the pattern from its first bit, untimed */
      t0 = jm_bench_now_ns ();
      dp_wfm_synth_steps (obj, out, BENCH_N);
      t1       = jm_bench_now_ns ();
      times[r] = jm_bench_elapsed_sec (t0, t1);
      if (dp_wfm_synth_data_ended (obj))
        {
          fprintf (stderr, "  %s: the pattern ran dry inside a block\n", name);
          exit (1);
        }
    }
  double mean = 0.0;
  for (int r = 0; r < ITERATIONS; r++)
    mean += times[r];
  mean /= ITERATIONS;
  double msas = (double)BENCH_N / mean / 1e6;
  printf ("  %-26s %8.1f MSa/s  (%.2f GSa/s)\n", name, msas, msas / 1000.0);
  jm_bench_add (bench, name, times, ITERATIONS, BENCH_N);

  dp_wfm_synth_destroy (obj);
}

int
main (void)
{
  float _Complex *out = malloc (BENCH_N * sizeof (float _Complex));
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
  bench_cfg_bits ("bits  amplitude sps=1", 1, 0, out, &bench);
  bench_cfg_bits ("bits  bpsk sps=8", 8, 1, out, &bench);
  bench_cfg_bits ("bits  qpsk sps=8", 8, 2, out, &bench);

  /* RRC pulse shaping — the polyphase resamp shaper (power-of-two sps). */
  bench_cfg_rrc ("bpsk  rrc sps=4", 3, 4, 7, 100.0, 1e5, out, &bench);
  bench_cfg_rrc ("bpsk  rrc sps=8", 3, 8, 7, 100.0, 1e5, out, &bench);
  bench_cfg_rrc ("qpsk  rrc sps=8", 4, 8, 7, 100.0, 1e5, out, &bench);
  bench_cfg_rrc ("bpsk  rrc sps=16", 3, 16, 7, 100.0, 1e5, out, &bench);

  jm_bench_write_json (&bench, "wfm_synth");
  free (out);
  return 0;
}
