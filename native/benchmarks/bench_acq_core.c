/* bench_acq_core.c — full C end-to-end wideband D=1 search: real acq_push(),
 * real n_noncoh non-coherent accumulation, at the SPEC-realistic waveform
 * this story settled on (prototypes/async_despreader/SPEC.md): Rc = 3.069
 * Mcps Gold-1023 code (spc=2 -> code_bins=2046, native span = chip_rate/
 * (2*sf) = 1500 Hz exactly), +/-50 kHz Doppler uncertainty -> window_bins =
 * ceil(50000/1500) = 34 parallel roll-FFT frequency-window hypotheses per
 * epoch (see acq_core.h's "Wideband window-tiling mode" doc comment and
 * bench_freq_bank.py, the Python prototype this reuses), cn0_dbhz = 37.31
 * (this waveform's real link budget). Built via acq_create_continuous() --
 * this is exactly the continuous/async scenario that engine always
 * window-tiles for.
 *
 * Task #71: prototypes/async_despreader/bench_freq_bank.py only measured the
 * per-epoch cost of forming the 34-bin grid (a Python/numpy prototype); this
 * measures the real, full acquisition latency in C -- n_noncoh consecutive
 * epochs non-coherently accumulated before the CFAR gate fires -- via one
 * timed acq_push() call per iteration, each pushing exactly
 * n_noncoh*code_bins samples (one full non-coherent dwell) of a real
 * injected burst + AWGN.
 *
 * n_noncoh itself is picked by acq_create_continuous()'s real physics-driven
 * auto-sizer at three pd targets (0.9/0.99/0.999) rather than forced to
 * SPEC.md's earlier n_noncoh=96/128/192 sweep -- that sweep was a
 * standalone Python sizing sketch predating this wideband mode's C
 * implementation, and the REAL 34-bin Bonferroni-corrected model here turns
 * out considerably more optimistic (pd_predicted ~0.999 already by
 * n_noncoh~96-123 at this cn0, not ~0.917 at 96 / ~0.994 at 192 as
 * estimated there) -- see the memory note accompanying this benchmark. The
 * auto-sizer's internal safety-valve ceiling (ACQ_N_NONCOH_SAFETY_CEILING =
 * 256, replacing the old caller-supplied max_noncoh cap) comfortably covers
 * the n_noncoh~96-123 this waveform actually lands on. */
#include "acq/acq_core.h"
#include "jm_bench.h"
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ITERATIONS 15
#define SF 1023            /* Gold-1023 code length. */
#define SPC 2              /* samples/chip -> code_bins = 2046.            */
#define CHIP_RATE 3.069e6  /* Hz.                                          */
#define CN0_DBHZ 37.31     /* dB-Hz -- this waveform's real link budget.   */
#define SYMBOL_RATE 2700.0 /* bps -- async BPSK data clock.               */
#define DOPPLER_UNCERTAINTY 50000.0 /* Hz -- +/-50 kHz.                   */
#define PFA 1e-3
/* Frequency-window hypothesis (0 .. window_bins-1) the injected burst
   lands in. */
#define INJECT_WINDOW 5
#define INJECT_PHASE 777 /* code phase (samples) to inject the burst at. */

static const double PD_POINTS[] = { 0.9, 0.99, 0.999 };

/* Two search geometries, so the wideband tiling cost is reported against its
 * own baseline rather than in isolation.  "native" is the no-coarse-Doppler
 * case (doppler_uncertainty = 0 -> window_bins == 1, a single native-span
 * window, one forward+inverse FFT per epoch); "wideband" tiles +/-50 kHz into
 * 35 roll-FFT hypotheses off ONE shared forward FFT.  The ratio between them
 * is the real price of covering SPEC.md's uncertainty, which the wideband
 * number alone never showed. */
typedef struct
{
  double      du;            /* doppler_uncertainty, Hz                  */
  const char *label;         /* bench name prefix                        */
  size_t      expect_bins;   /* window_bins the sizer must pick          */
  size_t      inject_window; /* hypothesis to inject into (0 when 1 bin) */
} acq_bench_cfg_t;

static const acq_bench_cfg_t CFGS[] = {
  { 0.0, "native", 1, 0 },
  { DOPPLER_UNCERTAINTY, "wideband", 35, INJECT_WINDOW },
};

static double
elapsed_sec (struct timespec *t0, struct timespec *t1)
{
  return (double)(t1->tv_sec - t0->tv_sec)
         + (double)(t1->tv_nsec - t0->tv_nsec) * 1e-9;
}

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
static float complex
cgauss (uint32_t *st)
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

  printf ("=== acq wideband D=1 benchmark (SPEC.md waveform) ===\n");
  printf ("sf=%d spc=%d code_bins=%zu chip_rate=%.0f cn0_dbhz=%.2f "
          "doppler_uncertainty=+/-%.0f\n\n",
          SF, SPC, nx, CHIP_RATE, CN0_DBHZ, DOPPLER_UNCERTAINTY);

  /* Synthetic 1023-chip code (a real Gold code's exact chips don't matter
   * for a latency benchmark -- correctness of the roll-FFT math was already
   * cross-checked bit-exact in bench_freq_bank.py and _acq_wideband_check).
   */
  uint8_t  code[SF];
  uint32_t cseed = 7;
  for (size_t c = 0; c < SF; c++)
    code[c] = (uint8_t)(_xorshift32 (&cseed) & 1u);

  const double fs      = CHIP_RATE * SPC;
  const double amp_snr = sqrt (pow (10.0, CN0_DBHZ / 10.0) / fs);
  const float  sigma   = (float)(1.0 / amp_snr);

  for (size_t g = 0; g < sizeof (CFGS) / sizeof (CFGS[0]); g++)
    {
      const acq_bench_cfg_t *cfg = &CFGS[g];
      printf ("--- %s: doppler_uncertainty=+/-%.0f Hz -> window_bins=%zu "
              "---\n",
              cfg->label, cfg->du, cfg->expect_bins);

      for (size_t p = 0; p < sizeof (PD_POINTS) / sizeof (PD_POINTS[0]); p++)
        {
          const double pd_target = PD_POINTS[p];

          /* Let the real auto-sizer pick n_noncoh honestly, bounded only by
           * the internal safety-valve ceiling -- see the file doc comment
           * above. */
          acq_state_t *a
              = acq_create_continuous (code, SF, SPC, CHIP_RATE, SYMBOL_RATE,
                                       CN0_DBHZ, cfg->du, PFA, pd_target, 0);
          if (!a)
            {
              fprintf (stderr, "acq_create_continuous failed at pd=%.3f\n",
                       pd_target);
              continue;
            }
          /* The wideband case expects 35, not the 34 this once did: window
           * sizing is against the spacing bins are actually reported at
           * (doppler_res_hz = 2*span), and forced odd so coverage is symmetric
           * and no ambiguous n/2 index exists -- see _cover_window_bins() /
           * acq_bin_to_signed(). */
          if (a->coherent_bins != 1 || a->window_bins != cfg->expect_bins)
            {
              fprintf (stderr,
                       "unexpected grid at pd=%.3f: coherent_bins=%zu "
                       "window_bins=%zu\n",
                       pd_target, a->coherent_bins, a->window_bins);
              acq_destroy (a);
              continue;
            }
          const size_t nc = a->n_noncoh;

          const size_t n_in = a->n_noncoh * nx;
          /* Same shared helper the engine itself uses, so the injected tone
             and the reported bin can never disagree about a row's sign. */
          const long signed_r
              = acq_bin_to_signed (cfg->inject_window, a->window_bins);
          const double f_norm = (double)signed_r / (double)nx;

          float complex *buf   = malloc (n_in * sizeof (float complex));
          uint32_t       nseed = 1234u + (uint32_t)nc;
          for (size_t k = 0; k < n_in; k++)
            {
              size_t        epoch_k = k % nx;
              size_t        src  = (epoch_k + nx - (INJECT_PHASE % nx)) % nx;
              uint8_t       chip = code[(src / SPC) % SF];
              float         c    = (chip & 1u) ? -1.0f : 1.0f;
              double        ph   = 2.0 * PI * f_norm * (double)k;
              float complex tone
                  = c * (float complex) (cos (ph) + I * sin (ph));
              buf[k] = tone + sigma * cgauss (&nseed);
            }

          acq_result_t hits[4];
          size_t       nh = acq_push (a, buf, n_in, hits, 4); /* warm-up */
          int ok = (nh == 1 && hits[0].doppler_bin == cfg->inject_window
                    && hits[0].code_phase == INJECT_PHASE);
          printf ("n_noncoh=%3zu  pd_predicted=%.4f  detect=%s  "
                  "doppler_bin=%zu code_phase=%zu cn0_dbhz_est=%.2f\n",
                  nc, a->pd_predicted, ok ? "yes" : "NO",
                  nh ? hits[0].doppler_bin : 0, nh ? hits[0].code_phase : 0,
                  nh ? hits[0].cn0_dbhz_est : 0.0f);

          double times[ITERATIONS];
          for (int r = 0; r < ITERATIONS; r++)
            {
              struct timespec t0, t1;
              clock_gettime (CLOCK_MONOTONIC, &t0);
              nh = acq_push (a, buf, n_in, hits, 4);
              clock_gettime (CLOCK_MONOTONIC, &t1);
              times[r] = elapsed_sec (&t0, &t1);
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
                  "(%zu epochs/dwell, %.3f ms/epoch)\n\n",
                  mean * 1e3, mn * 1e3, mx * 1e3, a->n_noncoh,
                  mean * 1e3 / (double)a->n_noncoh);

          char name[JM_BENCH_NAME_LEN];
          snprintf (name, sizeof (name), "%s_nc%zu", cfg->label, nc);
          jm_bench_add (&_bench, name, times, ITERATIONS, (int)n_in);

          free (buf);
          acq_destroy (a);
        }
    }

  jm_bench_write_json (&_bench, "acq");
  return 0;
}
