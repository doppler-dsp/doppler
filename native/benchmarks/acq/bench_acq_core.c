/* bench_acq_core.c — the code-phase searcher end to end in C: one timed
 * dp_acq_push() per round, each pushing exactly one full non-coherent dwell
 * (n_noncoh * D epochs) of an injected signal in AWGN, through the real
 * auto-sizer, CFAR gate and peak report. Built via
 * dp_acq_create_continuous(), the continuous/async engine that window-tiles
 * a Doppler uncertainty into roll-FFT frequency hypotheses off one shared
 * forward FFT per epoch (acq_core.h, "Wideband window-tiling mode").
 *
 * What the rows span, and why only these (doppler#2386: a row exists only
 * if it reads differently from every kept sibling, and a sweep keeps its
 * two ends):
 *   native      one window, no Doppler search: the floor.
 *   op<R>_U<u>  the continuous async-DSSS operating range's four corners,
 *               as the pool as built set it and as its budget measured
 *               them (docs/design/async-dsss-receiver-measurements.md
 *               §12.1): chip rate 2 or 5 Mcps, uncertainty +/-5 or
 *               +/-50 kHz. The searcher's cost per sample scales with
 *               the tile count, so the 2 Mcps, +/-50 kHz corner is its
 *               worst case.
 *   _t<n>       the same push fanned across n threads: 1 and 8, the ends.
 *   _D<d>       the block-coherent depth
 *               (docs/design/async-dsss-receiver-measurements.md §12.8).
 *   _fc         the same with an RF carrier set (CARRIER_HZ), so every
 *               side tile runs its code-rate ramp at D > 1: the one map's
 *               speed gate measures it against main (#2752,
 *               docs/design/acq-surface.md §18.4). The injected code's
 *               chip clock is dilated by the tone's Doppler over the
 *               carrier, as a real emitter's is, so the row detects.
 *
 * The detection target is the API default, pd = 0.9, and the auto-sizer
 * picks n_noncoh from it; the row name records the n_noncoh it chose. A
 * stricter target only lengthens the dwell, not the cost per input sample,
 * so one target is enough. */
#include "../../src/corr2d/corr2d_map.h"
#include "doppler/acq/acq_core.h"
#include "doppler/acq/acq_private.h"
#include "doppler/dp_complex.h"
#include "jm_bench.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ITERATIONS 15
#define SF 1023            /* Gold-1023 code length. */
#define SPC 2              /* samples/chip -> code_bins = 2046.            */
#define CHIP_RATE 3.069e6  /* Hz: the no-Doppler floor's chip rate.       */
#define CN0_DBHZ 37.31     /* dB-Hz -- this waveform's real link budget.   */
#define SYMBOL_RATE 2700.0 /* bps -- async BPSK data clock.               */
#define PFA 1e-3
/* Frequency-window hypothesis (0 .. window_bins-1) the injected burst
   lands in. */
#define INJECT_WINDOW 5
#define INJECT_PHASE 777 /* code phase (samples) to inject the burst at. */
#define CARRIER_HZ 2.2e9 /* an S-band carrier, for the _fc rows. */

/* The API default (objects/acq.toml); see the file comment for why one. */
static const double PD_POINTS[] = { 0.9 };

typedef struct
{
  double      chip_rate;        /* Hz                                       */
  double      du;               /* doppler_uncertainty, Hz                  */
  const char *label;            /* bench name prefix                        */
  size_t      expect_bins;      /* window_bins the sizer must pick          */
  size_t      inject_window;    /* hypothesis to inject into (0 when 1 bin) */
  size_t      code_only_epochs; /* the depth's window; 1 = D of 1        */
  int         threads;          /* the fan; 1 = serial, 0 = the machine     */
  double      carrier_hz;       /* RF carrier, Hz; 0 = none (no ramp)       */
} acq_bench_cfg_t;

/* The tile count is the engine's own (acq_cover_window_bins:
 * 2*ceil((du - span)/(2*span)) + 1), and it is what the searcher's cost
 * scales with -- so the low chip rate, which has the narrowest tiles, is the
 * searcher's worst case per output sample. */
static const acq_bench_cfg_t CFGS[] = {
  { .chip_rate        = CHIP_RATE,
    .du               = 0.0,
    .label            = "native",
    .expect_bins      = 1,
    .inject_window    = 0,
    .code_only_epochs = 1,
    .threads          = 1 },
  { .chip_rate        = 5.0e6,
    .du               = 50000.0,
    .label            = "op5M_U50k",
    .expect_bins      = 21,
    .inject_window    = INJECT_WINDOW,
    .code_only_epochs = 1,
    .threads          = 1 },
  { .chip_rate        = 2.0e6,
    .du               = 50000.0,
    .label            = "op2M_U50k",
    .expect_bins      = 53,
    .inject_window    = INJECT_WINDOW,
    .code_only_epochs = 1,
    .threads          = 1 },
  { .chip_rate        = 5.0e6,
    .du               = 5000.0,
    .label            = "op5M_U5k",
    .expect_bins      = 3,
    .inject_window    = 1,
    .code_only_epochs = 1,
    .threads          = 1 },
  { .chip_rate        = 2.0e6,
    .du               = 5000.0,
    .label            = "op2M_U5k",
    .expect_bins      = 7,
    .inject_window    = 1,
    .code_only_epochs = 1,
    .threads          = 1 },
  /* docs/design/async-dsss-receiver-measurements.md §12.8: the per-tile cost
     with the block-coherent depth in it (D = 150 at 5 Mcps, 60 at 2: the
     window's 813 and 324 whole epochs under a 500 Hz/s rate bound give 154
     and 61, which the engine rounds down to a 7-smooth length), and the
     same push fanned across threads by the roll per thread (section 2.3) --
     the fraction of the tiles' cost that scales. Each fan is measured at its
     two ends; a D = 1 row at the same counts beside them is the fan on the
     epoch-by-epoch searcher alone. The published run is pinned to 4 cores
     and their SMT siblings, so 8 threads run two to a core (#2394). */
  { .chip_rate        = 5.0e6,
    .du               = 50000.0,
    .label            = "op5M_U50k_t8",
    .expect_bins      = 21,
    .inject_window    = INJECT_WINDOW,
    .code_only_epochs = 1,
    .threads          = 8 },
  { .chip_rate        = 5.0e6,
    .du               = 50000.0,
    .label            = "op5M_U50k_D150_t1",
    .expect_bins      = 21,
    .inject_window    = INJECT_WINDOW,
    .code_only_epochs = 813,
    .threads          = 1 },
  { .chip_rate        = 5.0e6,
    .du               = 50000.0,
    .label            = "op5M_U50k_D150_t8",
    .expect_bins      = 21,
    .inject_window    = INJECT_WINDOW,
    .code_only_epochs = 813,
    .threads          = 8 },
  { .chip_rate        = 2.0e6,
    .du               = 50000.0,
    .label            = "op2M_U50k_D60_t1",
    .expect_bins      = 53,
    .inject_window    = INJECT_WINDOW,
    .code_only_epochs = 324,
    .threads          = 1 },
  { .chip_rate        = 2.0e6,
    .du               = 50000.0,
    .label            = "op2M_U50k_D60_t4",
    .expect_bins      = 53,
    .inject_window    = INJECT_WINDOW,
    .code_only_epochs = 324,
    .threads          = 4 },
  { .chip_rate        = 5.0e6,
    .du               = 5000.0,
    .label            = "op5M_U5k_D150_t1",
    .expect_bins      = 3,
    .inject_window    = 1,
    .code_only_epochs = 813,
    .threads          = 1 },
  { .chip_rate        = 5.0e6,
    .du               = 5000.0,
    .label            = "op5M_U5k_D150_t4",
    .expect_bins      = 3,
    .inject_window    = 1,
    .code_only_epochs = 813,
    .threads          = 4 },
  /* The block-coherent rows with a carrier: the tile fan's code-rate ramp
     (#2752 §18.4), serial, at the three geometries the one map was
     measured on (docs/design/acq-surface-measurements.md §18.1). */
  { .chip_rate        = 5.0e6,
    .du               = 50000.0,
    .label            = "op5M_U50k_D150_t1_fc",
    .expect_bins      = 21,
    .inject_window    = INJECT_WINDOW,
    .code_only_epochs = 813,
    .threads          = 1,
    .carrier_hz       = CARRIER_HZ },
  { .chip_rate        = 2.0e6,
    .du               = 50000.0,
    .label            = "op2M_U50k_D60_t1_fc",
    .expect_bins      = 53,
    .inject_window    = INJECT_WINDOW,
    .code_only_epochs = 324,
    .threads          = 1,
    .carrier_hz       = CARRIER_HZ },
  { .chip_rate        = 5.0e6,
    .du               = 5000.0,
    .label            = "op5M_U5k_D150_t1_fc",
    .expect_bins      = 3,
    .inject_window    = 1,
    .code_only_epochs = 813,
    .threads          = 1,
    .carrier_hz       = CARRIER_HZ },
  /* Five tiles, either side of the one map's crossover
     (docs/design/acq-surface.md §18.1): without a carrier the rule picks
     slow first (it crosses between three tiles and five at D = 150), with
     one rows first (the fine grid moves the crossover past fifteen). So
     the speed gate holds a row on each side near it. */
  { .chip_rate        = 5.0e6,
    .du               = 10000.0,
    .label            = "op5M_U10k_D150_t1",
    .expect_bins      = 5,
    .inject_window    = 2,
    .code_only_epochs = 813,
    .threads          = 1 },
  { .chip_rate        = 5.0e6,
    .du               = 10000.0,
    .label            = "op5M_U10k_D150_t1_fc",
    .expect_bins      = 5,
    .inject_window    = 2,
    .code_only_epochs = 813,
    .threads          = 1,
    .carrier_hz       = CARRIER_HZ },
};

static uint32_t
_xorshift32 (uint32_t *s)
{
  *s ^= *s << 13;
  *s ^= *s >> 17;
  *s ^= *s << 5;
  return *s;
}

/* Unit-variance complex Gaussian (Box-Muller); same generator as
 * test_acq_core.c's cgauss -- E|z|^2 = 1. */
static float _Complex cgauss (uint32_t *st)
{
  uint32_t a   = _xorshift32 (st);
  uint32_t b   = _xorshift32 (st);
  double   u1  = ((double)a + 1.0) / 4294967297.0;
  double   u2  = ((double)b + 1.0) / 4294967297.0;
  double   mag = sqrt (-log (u1));
  double   th  = 6.283185307179586 * u2;
  return (float)(mag * cos (th)) + (float)(mag * sin (th)) * I;
}

int
main (void)
{
  jm_bench_t   _bench = { 0 };
  const double PI     = acos (-1.0);
  const size_t nx     = SF * SPC; /* code_bins = 2046. */

  printf ("=== acq search benchmark ===\n");
  printf ("sf=%d spc=%d code_bins=%zu cn0_dbhz=%.2f pd=%.2f\n\n", SF, SPC, nx,
          CN0_DBHZ, PD_POINTS[0]);

  /* Synthetic 1023-chip code (a real Gold code's exact chips don't matter
   * for a latency benchmark -- correctness of the roll-FFT math was already
   * cross-checked bit-exact in bench_freq_bank.py and _acq_wideband_check).
   */
  uint8_t  code[SF];
  uint32_t cseed = 7;
  for (size_t c = 0; c < SF; c++)
    code[c] = (uint8_t)(_xorshift32 (&cseed) & 1u);

  for (size_t g = 0; g < sizeof (CFGS) / sizeof (CFGS[0]); g++)
    {
      const acq_bench_cfg_t *cfg = &CFGS[g];
      /* The noise is scaled per chip rate: the same C/N0 is a different
         per-sample SNR when the sample rate changes with the chip rate. */
      const double fs      = cfg->chip_rate * SPC;
      const double amp_snr = sqrt (pow (10.0, CN0_DBHZ / 10.0) / fs);
      const float  sigma   = (float)(1.0 / amp_snr);
      printf ("--- %s: chip_rate=%.3g doppler_uncertainty=+/-%.0f Hz -> "
              "window_bins=%zu ---\n",
              cfg->label, cfg->chip_rate, cfg->du, cfg->expect_bins);

      for (size_t p = 0; p < sizeof (PD_POINTS) / sizeof (PD_POINTS[0]); p++)
        {
          const double pd_target = PD_POINTS[p];

          /* Let the real auto-sizer pick n_noncoh honestly, bounded only by
           * the internal safety-valve ceiling -- see the file doc comment
           * above. */
          dp_acq_state_t *a = dp_acq_create_continuous (
              code, SF, cfg->chip_rate * (double)SPC, cfg->chip_rate,
              SYMBOL_RATE, CN0_DBHZ, cfg->du, PFA, pd_target, 0,
              cfg->code_only_epochs, cfg->code_only_epochs > 1 ? 500.0 : 0.0);
          if (!a)
            {
              fprintf (stderr, "dp_acq_create_continuous failed at pd=%.3f\n",
                       pd_target);
              continue;
            }
          /* expect_bins is odd by construction: window sizing is against the
           * spacing bins are actually reported at (doppler_res_hz = 2*span),
           * and forced odd so coverage is symmetric and no ambiguous n/2
           * index exists -- see acq_cover_window_bins() / dp_fftfreq_index().
           */
          (void)dp_acq_set_threads (a, cfg->threads);
          if (cfg->carrier_hz > 0.0
              && dp_acq_set_carrier_freq_hz (a, cfg->carrier_hz) != DP_OK)
            {
              fprintf (stderr, "dp_acq_set_carrier_freq_hz refused %g Hz\n",
                       cfg->carrier_hz);
              dp_acq_destroy (a);
              return 1;
            }
          const size_t D = a->coherent_bins;
          if ((cfg->code_only_epochs == 1 && D != 1)
              || a->window_bins != cfg->expect_bins)
            {
              fprintf (stderr,
                       "unexpected grid at pd=%.3f: coherent_bins=%zu "
                       "window_bins=%zu\n",
                       pd_target, a->coherent_bins, a->window_bins);
              dp_acq_destroy (a);
              continue;
            }
          const size_t nc   = a->n_noncoh;
          const size_t n_in = a->n_noncoh * D * nx; /* a block per look */
          /* Same shared helper the engine itself uses, so the injected tone
             and the reported bin can never disagree about a row's sign. */
          const long signed_r
              = dp_fftfreq_index (cfg->inject_window, a->window_bins);
          const double f_norm = (double)signed_r / (double)nx;
          /* The code's chip clock rides the carrier's Doppler: it runs
             (1 + f / carrier) fast, f = f_norm * fs. */
          const double dil = cfg->carrier_hz > 0.0
                                 ? 1.0 + f_norm * fs / cfg->carrier_hz
                                 : 1.0;

          float _Complex *buf   = malloc (n_in * sizeof (float _Complex));
          uint32_t        nseed = 1234u + (uint32_t)nc;
          for (size_t k = 0; k < n_in; k++)
            {
              size_t epoch_k = k % nx;
              size_t src     = (epoch_k + nx - (INJECT_PHASE % nx)) % nx;
              if (dil != 1.0)
                {
                  const double t = (double)k * dil - (double)INJECT_PHASE;
                  src = (size_t)(t - (double)nx * floor (t / (double)nx));
                }
              uint8_t chip = code[(src / SPC) % SF];
              float   c    = (chip & 1u) ? -1.0f : 1.0f;
              double  ph   = 2.0 * PI * f_norm * (double)k;
              float _Complex tone
                  = c * (float _Complex) (cos (ph) + I * sin (ph));
              buf[k] = tone + sigma * cgauss (&nseed);
            }

          acq_result_t hits[4];
          size_t       nh = dp_acq_push (a, buf, n_in, hits, 4); /* warm-up */
          /* The lag column nearest the injected phase: on a padded lag
             grid a column is code_bins / lag_bins of a sample. */
          const size_t want_col
              = (size_t)lround ((double)INJECT_PHASE * (double)a->lag_bins
                                / (double)a->code_bins)
                % a->lag_bins;
          /* With a carrier the code drifts (dil - 1) * nx samples an
             epoch, and the ramp reads the block at its middle, so the peak
             may sit up to half the block's drift from the injected phase,
             either way: that many columns, plus one for the rounding. */
          const size_t drift
              = dil != 1.0
                    ? (size_t)ceil (0.5 * (double)(D - 1) * fabs (dil - 1.0)
                                    * (double)nx * (double)a->lag_bins
                                    / (double)a->code_bins)
                          + 1
                    : 0;
          size_t off_col = nh ? (hits[0].code_phase + a->lag_bins - want_col)
                                    % a->lag_bins
                              : 0;
          if (off_col > a->lag_bins / 2)
            off_col = a->lag_bins - off_col;
          int ok = (nh == 1 && hits[0].doppler_bin == cfg->inject_window * D
                    && off_col <= drift);
          /* The schedule the one delay-Doppler map runs this row in
             (docs/design/acq-surface.md §18.1), as the rule picks it. */
          const int sch = dp__dd_map_pick (
              a->map, a->window_bins > 1 ? a->window_bins : 1,
              acq_rate_count_at (a->rate_hyps, a->coherent_bins), 0);
          printf ("n_noncoh=%3zu D=%zu threads=%d pd_predicted=%.4f  "
                  "detect=%s  doppler_bin=%zu code_phase=%zu "
                  "cn0_dbhz_est=%.2f  schedule=%s\n",
                  nc, D, a->threads, a->pd_predicted, ok ? "yes" : "NO",
                  nh ? hits[0].doppler_bin : 0, nh ? hits[0].code_phase : 0,
                  nh ? hits[0].cn0_dbhz_est : 0.0f,
                  a->coherent_bins * a->interp <= 1 ? "rows only (D = 1)"
                  : sch == DP__DD_ROWS_FIRST        ? "rows first"
                                                    : "slow first");
          /* A row that misses its own emitter times a search that found
             nothing: refuse it rather than print NO and time it anyway. */
          if (!ok)
            {
              (void)fprintf (stderr,
                             "bench_acq: the injected emitter was not "
                             "detected on its cell (want col %zu)\n",
                             want_col);
              return 1;
            }

          double times[ITERATIONS];
          for (int r = 0; r < ITERATIONS; r++)
            {
              uint64_t t0, t1;
              t0       = jm_bench_now_ns ();
              nh       = dp_acq_push (a, buf, n_in, hits, 4);
              t1       = jm_bench_now_ns ();
              times[r] = jm_bench_elapsed_sec (t0, t1);
              if (nh != 1)
                fprintf (stderr, "  iter %d: unexpected nh=%zu\n", r, nh);
            }

          double sum = 0.0, mn = times[0], mx = times[0];
          for (int r = 0; r < ITERATIONS; r++)
            {
              sum += times[r];
              if (times[r] < mn)
                mn = times[r];
              if (times[r] > mx)
                mx = times[r];
            }
          double mean = sum / ITERATIONS;
          printf ("  latency: mean=%.2f ms  min=%.2f ms  max=%.2f ms  "
                  "(%zu epochs/dwell, %.3f ms/epoch, %.1f ns/sample min, "
                  "%.2fx real time on one core)\n\n",
                  mean * 1e3, mn * 1e3, mx * 1e3, a->n_noncoh * D,
                  mean * 1e3 / (double)(a->n_noncoh * D),
                  mn / (double)n_in * 1e9, mn / (double)n_in * fs);

          char name[JM_BENCH_NAME_LEN];
          snprintf (name, sizeof (name), "%s_nc%zu", cfg->label, nc);
          jm_bench_add_unit (&_bench, name, times, ITERATIONS, (int)n_in,
                             JM_BENCH_SAMPLE);

          free (buf);
          dp_acq_destroy (a);
        }
    }

  jm_bench_write_json (&_bench, "acq");
  return 0;
}
