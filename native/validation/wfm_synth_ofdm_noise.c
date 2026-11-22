/**
 * @file wfm_synth_ofdm_noise.c
 * @brief The noise of a wfmgen `ofdm` source against its contract: AWGN of
 *        variance 10^(-snr/10) per complex sample, referred to the full
 *        sampled band (snr_mode auto or fs) and to a unit-power frame, in
 *        the PPDU and in the gap alike (wfm_compose.h, `gap` and the
 *        renderer's unit-power contract in wfm_synth_core.h).
 *
 * Each cell renders the same source twice from one seed, once clean
 * (snr = WFM_SYNTH_SNR_CLEAN, no AWGN) and once at the cell's snr; their
 * difference is the noise alone, since the frame does not depend on the snr.
 * The difference is split into the PPDU's samples and the gap's, and each
 * part's mean power is read by the measure module's dp_tonemeas_time_stats():
 * its `rms` over the samples' interleaved I and Q floats, so the mean power
 * per complex sample is 2 rms^2 (the meter counts DC in, which is what a
 * variance about zero is).
 *
 * The prediction, per cell, written before the first run. The noise is
 * circular complex Gaussian with E|n|^2 = sigma^2 = 10^(-snr/10) (the synth's
 * per-component amplitude sqrt(1 / (2 * 10^(snr/10))), wfm_synth_core.c,
 * rounded in float: a relative error of a few 1e-8). |n|^2 / sigma^2 is
 * exponential with unit mean and unit variance, so the mean of N samples
 * estimates sigma^2 with relative standard error 1/sqrt(N). The difference
 * of the two renders adds a float rounding of the frame (|x| ~ 1, so ~6e-8),
 * which is nine orders under the smallest sigma^2 measured (1e-3 at 30 dB).
 * A cell passes when |measured / sigma^2 - 1| < 5 / sqrt(N): a false red
 * at 5 sigma is 6e-7 a cell. The sample counts:
 *
 *   --check   M = 2, snr 10 dB, 8 frames: the PPDU is 880 M samples (100
 *             octets at 36 Mb/s, six Data symbols), so N = 8 * 1760 = 14080
 *             in the PPDU and the same in the gap (the gap is one PPDU
 *             long), and the tolerance is 5 / sqrt(14080) = 4.2 %.
 *   full      M = 1, 2, 4, 8 and snr 0, 10, 20, 30 dB, 64 frames: N = 64 *
 *             880 M per part, from 56320 (2.1 %) at M = 1 to 450560 (0.74 %)
 *             at M = 8.
 *
 * What it must catch: a noise referred to anything but the full band. An
 * in-band reference (the 52 occupied subcarriers of 64 M) puts sigma^2 off by
 * 64 M / 52, 2.46 at M = 2; one referred to the 20 MHz channel, off by M; an
 * Es/N0 per subcarrier, off by 52 / 64 = -19 %. Each is far outside 4 %.
 *
 * Every cell also renders the same source with snr_mode auto and fs and
 * requires the two renders to be the same bytes: for ofdm, auto is fs.
 *
 * The rows print the frame's own mean power over the PPDU (clean, prefixes
 * and guard included, so not exactly 1: the unit-power convention is over
 * each field's useful samples, pinned in test_wfm_ofdm_bridge.c) and the SNR
 * that makes over the full band. That is information, not a check.
 *
 *   wfm_synth_ofdm_noise            the full sweep, printed
 *   wfm_synth_ofdm_noise --emit     the full sweep as CSV for validate.py
 *   wfm_synth_ofdm_noise --check    the cell ctest runs
 *   wfm_synth_ofdm_noise --check --emit   that cell as CSV
 */
#include "doppler/tonemeas/tonemeas_core.h"
#include "doppler/wfm/wfm_compose.h"
#include "doppler/wfm/wfm_defaults.h"
#include "doppler/wfm/wfm_frame.h"
#include "doppler/wfm_compose/wfm_compose_bridge.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "doppler/wlan/wlan_core.h"
#include "dp_rng_test.h"
#include "dp_test.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef float _Complex cf;

#define OCTETS 100u /* one PSDU: 36 Mb/s, 6 Data symbols */

typedef struct
{
  unsigned m;           /* oversampling: fs / 20 MHz */
  double   snr;         /* requested, dB */
  size_t   n;           /* noise samples per part (PPDU, gap) */
  double   ppdu_ratio;  /* measured / sigma^2 over the PPDU */
  double   gap_ratio;   /* measured / sigma^2 over the gap */
  double   tol;         /* 5 / sqrt(n) */
  double   frame_power; /* the clean frame's mean power over the PPDU */
  int      auto_is_fs;  /* 1: snr_mode auto and fs render the same bytes */
} cell_t;

/* Mean power per complex sample of x[0 .. n), by the measure module: rms over
   the 2n interleaved floats, squared, times 2. */
static double
mean_power (dp_tonemeas_state_t *meter, const cf *x, size_t n)
{
  const time_stats_t ts
      = dp_tonemeas_time_stats (meter, (const float *)x, 2u * n);
  return 2.0 * ts.rms * ts.rms;
}

static cf *
render (const wfm_source_t *s, double fs, size_t n)
{
  dp_wfm_synth_state_t *syn = dp_wfm_source_to_synth (s, fs);
  if (!syn)
    {
      fprintf (stderr, "refused: %s\n", dp_wfm_source_to_synth_error (s, fs));
      return NULL;
    }
  cf *x = malloc (n * sizeof *x);
  if (x)
    dp_wfm_synth_steps (syn, x, n);
  dp_wfm_synth_destroy (syn);
  return x;
}

static cell_t
measure (dp_tonemeas_state_t *meter, unsigned m, double snr, unsigned frames,
         uint32_t seed)
{
  cell_t c = { .m = m, .snr = snr };
  /* frames PSDUs of seeded octets, as bits, first bit most significant */
  const size_t n_bits = (size_t)frames * 8u * OCTETS;
  uint8_t     *bits   = malloc (n_bits);
  if (!bits)
    return c;
  uint32_t st = seed;
  for (size_t k = 0; k < n_bits; k++)
    bits[k] = (uint8_t)(dp_xs32 (&st) & 1u);

  const dp_wlan_rate_t r36 = { 4u, WLAN_CODE_3_4 };
  const size_t         ppdu
      = dp_wlan_ppdu_len (dp_wlan_rate_n_sym (&r36, OCTETS), 0u, m);
  const size_t frame = 2u * ppdu; /* the gap is one PPDU long */
  const size_t n     = (size_t)frames * frame;
  const double fs    = 20e6 * m;

  wfm_source_t s = WFM_SOURCE_DEFAULTS;
  s.type         = WFM_SYNTH_FRAMES;
  s.data = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .len = n_bits, .bits = bits };
  s.data_len       = 8u * OCTETS;
  s.wlan           = 36;
  s.scrambler_init = 93;
  s.window         = 0; /* stated: the default window is changing (#3190) */
  s.gap            = ppdu;
  s.seed           = seed;
  s.snr            = WFM_SYNTH_SNR_CLEAN;
  cf *clean        = render (&s, fs, n);
  s.snr            = snr;
  s.snr_mode       = 1; /* fs */
  cf *noisy        = render (&s, fs, n);
  s.snr_mode       = 0; /* auto */
  cf *noisy_auto   = render (&s, fs, n);
  cf *diff         = malloc (n * sizeof *diff);
  if (clean && noisy && noisy_auto && diff)
    {
      c.auto_is_fs = memcmp (noisy, noisy_auto, n * sizeof *noisy) == 0;
      /* the noise alone, gathered by part: PPDUs first, then gaps */
      const size_t half = (size_t)frames * ppdu;
      for (unsigned f = 0; f < frames; f++)
        for (size_t i = 0; i < ppdu; i++)
          {
            const size_t a = (size_t)f * frame + i, b = a + ppdu;
            diff[(size_t)f * ppdu + i]        = noisy[a] - clean[a];
            diff[half + (size_t)f * ppdu + i] = noisy[b] - clean[b];
          }
      const double sigma2 = pow (10.0, -snr / 10.0);
      c.n                 = half;
      c.ppdu_ratio        = mean_power (meter, diff, half) / sigma2;
      c.gap_ratio         = mean_power (meter, diff + half, half) / sigma2;
      c.tol               = 5.0 / sqrt ((double)half);
      double p            = 0;
      for (unsigned f = 0; f < frames; f++)
        p += mean_power (meter, clean + (size_t)f * frame, ppdu);
      c.frame_power = p / frames;
    }
  free (clean);
  free (noisy);
  free (noisy_auto);
  free (diff);
  free (bits);
  return c;
}

static void
print_cells (const cell_t *c, size_t n, int emit)
{
  if (emit)
    {
      printf ("# ofdm_noise\n");
      printf ("m,snr_db,n,ppdu_ratio,gap_ratio,tol,frame_power,"
              "snr_full_band_db,auto_is_fs\n");
      for (size_t i = 0; i < n; i++)
        printf ("%u,%.1f,%zu,%.6f,%.6f,%.6f,%.6f,%.3f,%d\n", c[i].m, c[i].snr,
                c[i].n, c[i].ppdu_ratio, c[i].gap_ratio, c[i].tol,
                c[i].frame_power,
                10.0 * log10 (c[i].frame_power / c[i].ppdu_ratio) + c[i].snr,
                c[i].auto_is_fs);
      return;
    }
  printf ("%3s %6s %8s %10s %10s %8s %11s %8s %5s\n", "M", "snr", "N",
          "ppdu/s2", "gap/s2", "tol", "frame pow", "SNR fs", "auto");
  for (size_t i = 0; i < n; i++)
    printf ("%3u %6.1f %8zu %10.4f %10.4f %8.4f %11.4f %8.2f %5s\n", c[i].m,
            c[i].snr, c[i].n, c[i].ppdu_ratio, c[i].gap_ratio, c[i].tol,
            c[i].frame_power,
            10.0 * log10 (c[i].frame_power / c[i].ppdu_ratio) + c[i].snr,
            c[i].auto_is_fs ? "=fs" : "DIFF");
}

static void
check_cell (const cell_t *c)
{
  DP_CHECK_MSG (c->n > 0, "the cell rendered");
  DP_CHECK_MSG (fabs (c->ppdu_ratio - 1.0) < c->tol,
                "PPDU noise variance within 5 / sqrt(N) of 10^(-snr/10)");
  DP_CHECK_MSG (fabs (c->gap_ratio - 1.0) < c->tol,
                "gap noise variance within 5 / sqrt(N) of 10^(-snr/10)");
  DP_CHECK_MSG (c->auto_is_fs, "snr_mode auto renders as fs");
}

int
main (int argc, char **argv)
{
  int check = 0, emit = 0;
  for (int i = 1; i < argc; i++)
    {
      check |= strcmp (argv[i], "--check") == 0;
      emit |= strcmp (argv[i], "--emit") == 0;
    }
  /* the meter reads only its full-scale reference for time_stats */
  dp_tonemeas_state_t *meter
      = dp_tonemeas_create (1024u, 1.0, 0u, 1.0, 0u, 0.0, 0u);
  DP_REQUIRE (meter != NULL);

  if (check)
    {
      cell_t c = measure (meter, 2u, 10.0, 8u, 2026u);
      print_cells (&c, 1, emit);
      check_cell (&c);
      /* control: the tolerance is tighter than the nearest wrong reference,
         Es/N0 per subcarrier at 52 / 64 */
      DP_CHECK_MSG (c.tol < 1.0 - 52.0 / 64.0,
                    "control: 5 sigma is inside the nearest wrong reference");
      dp_tonemeas_destroy (meter);
      if (emit)
        DP_TEST_EMIT_END ("wfm_synth_ofdm_noise --check");
      DP_TEST_END ("wfm_synth_ofdm_noise --check");
    }

  static const unsigned ms[]   = { 1, 2, 4, 8 };
  static const double   snrs[] = { 0.0, 10.0, 20.0, 30.0 };
  cell_t                cells[16];
  size_t                n_cells = 0;
  uint32_t              seed    = 1u;
  for (size_t i = 0; i < 4; i++)
    for (size_t j = 0; j < 4; j++)
      cells[n_cells++] = measure (meter, ms[i], snrs[j], 64u, seed++);
  print_cells (cells, n_cells, emit);
  for (size_t i = 0; i < n_cells; i++)
    check_cell (&cells[i]);
  dp_tonemeas_destroy (meter);
  if (emit)
    DP_TEST_EMIT_END ("wfm_synth_ofdm_noise");
  DP_TEST_END ("wfm_synth_ofdm_noise");
}
