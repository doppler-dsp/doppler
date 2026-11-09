/* bench_correlator_core.c — single-row-reference fast path vs. the general
 * 2-D path, at a DSSS-acquisition-realistic grid.  A single-row reference
 * (the acq/detector2d shape: a code replica with no Doppler content of its
 * own) takes corr2d's fast path (ny independent length-nx 1-D FFTs); a
 * genuinely multi-row reference takes the general path (full (ny,nx) 2-D
 * FFT forward and inverse).  This is the durable regression gate for the
 * fast-path optimization (see docs/design/corr2d-interpolated-inverse.md).
 */
#include "../../src/correlator/correlator_map.h"
#include "doppler/correlator/correlator_core.h"
#include "doppler/correlator/correlator_private.h"
#include "doppler/dp_complex.h"
#include "dp_bench.h"
#include "dp_bench_pn.h"
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define ITERATIONS 50

static uint32_t
_xorshift32 (uint32_t *s)
{
  *s ^= *s << 13;
  *s ^= *s >> 17;
  *s ^= *s << 5;
  return *s;
}

static float
_rand_uniform (uint32_t *s)
{
  return ((float)(_xorshift32 (s) % 20001u) - 10000.0f) / 10000.0f;
}

/* Batches (#2683): H maps in one call against H execute() calls.  The
 * request's geometry: a 64 x 512 delay-Doppler map, H = 48 frames.  The
 * per-map work is the same either way, so what a batch can save is the
 * per-call overhead and, on a pool, the dispatches: whole maps per worker
 * against each map split across every worker.  Setup (create, stimulus,
 * buffers) is outside the timed region. */
static void
bench_batches (jm_bench_t *bench, uint32_t *seed)
{
  enum
  {
    BREPS = 10
  };
  const size_t    bny = 64, bnx = 512, H = 48, bn = bny * bnx;
  float _Complex *code = malloc (bnx * sizeof *code);
  float _Complex *x    = malloc (H * bn * sizeof *x);
  float _Complex *maps = malloc (H * bn * sizeof *maps);
  float          *pw   = malloc (bn * sizeof *pw);
  uint32_t       *idx  = malloc (bn * sizeof *idx);
  for (size_t j = 0; j < bnx; j++)
    code[j] = _rand_uniform (seed) + _rand_uniform (seed) * I;
  for (size_t k = 0; k < H * bn; k++)
    x[k] = _rand_uniform (seed) + _rand_uniform (seed) * I;
  const struct
  {
    int         nthreads;
    const char *name[3];
  } bc[] = {
    { 1,
      { "batch_64x512_h48_execute_loop_1t", "batch_64x512_h48_all_1t",
        "batch_64x512_h48_power_max_1t" } },
    { 0,
      { "batch_64x512_h48_execute_loop_all", "batch_64x512_h48_all_all",
        "batch_64x512_h48_power_max_all" } },
  };
  double bt[BREPS];
  for (size_t c = 0; c < sizeof bc / sizeof *bc; c++)
    {
      dp_correlator_state_t *dd = dp_correlator_create_delay_doppler (
          code, bnx, bny, 1, bc[c].nthreads, 0);
      /* warmup: one of each, which also makes the batch scratch */
      dp_correlator_execute (dd, x, bn, maps, bn, NULL);
      dp_correlator_execute_all (dd, x, H * bn, maps, H * bn, NULL);
      for (int which = 0; which < 3; which++)
        {
          double best = 1e30;
          for (int r = 0; r < BREPS; r++)
            {
              const uint64_t t0 = jm_bench_now_ns ();
              if (which == 0)
                for (size_t h = 0; h < H; h++)
                  dp_correlator_execute (dd, x + h * bn, bn, maps + h * bn, bn,
                                         NULL);
              else if (which == 1)
                dp_correlator_execute_all (dd, x, H * bn, maps, H * bn, NULL);
              else
                dp_correlator_execute_power_max (dd, x, H * bn, pw, idx, bn,
                                                 NULL);
              bt[r] = jm_bench_elapsed_sec (t0, jm_bench_now_ns ());
              best  = bt[r] < best ? bt[r] : best;
            }
          jm_bench_add_unit (bench, bc[c].name[which], bt, BREPS, H * bn,
                             JM_BENCH_SAMPLE);
          printf ("  %-36s %8.2f ms  (%d workers)\n", bc[c].name[which],
                  best * 1e3, dd->nworkers);
        }
      dp_correlator_destroy (dd);
    }
  free (code);
  free (x);
  free (maps);
  free (pw);
  free (idx);
}

/* The hypothesis bank (#2683 phase 3): one frame through 12 carriers (three
 * whole bins x L = 4), against the frame batch of 12 hand-corrected copies
 * that a caller would otherwise build, and the coarse search ruling 9
 * prefers: L = 2 alone, and bank_detect's L = 2 search with an L = 4 refine
 * on a frame holding one signal.  64 x 512, one thread; setup (create, the
 * copies, the stimulus) is outside the timed region.  And the same L = 4
 * bank with R = 12 Doppler-rate weightings, 144 hypotheses: the case where
 * a rate costs only a slow-time column, today, so the one delay-Doppler
 * map (#2752 step 2.3, #2831) is measured against it before it lands. */
static void
bench_bank (jm_bench_t *bench, uint32_t *seed)
{
  enum
  {
    KREPS = 10
  };
  const size_t    ny = 64, nx = 512, n = ny * nx, H = 12;
  float _Complex *code   = malloc (nx * sizeof *code);
  float _Complex *x      = malloc (n * sizeof *x);
  float _Complex *copies = malloc (H * n * sizeof *copies);
  float          *pw     = malloc (n * sizeof *pw);
  uint32_t       *idx    = malloc (n * sizeof *idx);
  float          *rp     = malloc (n * sizeof *rp);
  int32_t        *rg     = malloc (n * sizeof *rg);
  uint32_t       *ra = malloc (n * sizeof *ra), *rr = malloc (n * sizeof *rr);
  uint32_t       *rc = malloc (n * sizeof *rc);
  enum
  {
    R = 12
  };
  /* Rate r's per-period de-chirp, exp(-j pi a_r t^2), t in periods from
     the frame's middle. The cost depends on R alone, not on the rates: 12
     is corr2d-hypotheses.md §5's geometry. The span, a_r over +-0.01
     cycles a period^2, is chosen so that neighbouring rates differ by
     about 6 rad at the frame's ends, so the row's check below can tell
     them apart. */
  enum
  {
    RATE_IN = 8 /* the rate xr carries, so the search has an answer */
  };
  float _Complex *rates = malloc (R * ny * sizeof *rates);
  float _Complex *xr    = malloc (n * sizeof *xr);
  for (size_t r = 0; r < R; r++)
    for (size_t i = 0; i < ny; i++)
      {
        const double ar = 0.01 * (2.0 * (double)r / (R - 1) - 1.0);
        const double ti = (double)i - 0.5 * (double)(ny - 1);
        rates[r * ny + i]
            = cexpf (-I * (float)(3.14159265358979323846 * ar * ti * ti));
      }
  for (size_t j = 0; j < nx; j++)
    code[j] = (_xorshift32 (seed) & 1u) ? 1.0f : -1.0f;
  /* the code at lag 77 on a carrier a quarter bin off nominal, in noise */
  for (size_t m = 0; m < n; m++)
    {
      const double a = 2.0 * 3.14159265358979323846 * 0.25 * (double)m / nx;
      x[m]           = 0.1f * code[(m % nx + nx - 77) % nx]
                           * ((float)cos (a) + (float)sin (a) * I)
                       + _rand_uniform (seed) + _rand_uniform (seed) * I;
    }
  for (size_t h = 0; h < H; h++) /* carrier g = h - 4, on a 1/(4 nx) grid */
    for (size_t i = 0; i < ny; i++)
      for (size_t p = 0; p < nx; p++)
        {
          const double a = -2.0 * 3.14159265358979323846 * ((double)h - 4.0)
                           * (double)p / (4.0 * nx);
          copies[h * n + i * nx + p]
              = x[i * nx + p] * ((float)cos (a) + (float)sin (a) * I);
        }
  /* x carrying rate RATE_IN: what the rated row searches */
  for (size_t i = 0; i < ny; i++)
    for (size_t p = 0; p < nx; p++)
      xr[i * nx + p] = x[i * nx + p] * conjf (rates[RATE_IN * ny + i]);
  dp_correlator_state_t *dd
      = dp_correlator_create_delay_doppler (code, nx, ny, 1, 1, 0);
  /* a candidate threshold at half the coarse peak: one signal, one record */
  dp_correlator_bank_power_max (dd, x, n, 2, -2, 3, NULL, 0, pw, idx, n, NULL,
                                NULL);
  float top = 0.0f;
  for (size_t k = 0; k < n; k++)
    top = pw[k] > top ? pw[k] : top;
  const float        t = 0.5f * top;
  static const char *name[5]
      = { "bank_64x512_c12_corrected_batch_1t", "bank_64x512_c12_l4_1t",
          "bank_64x512_c6_l2_1t", "bank_64x512_detect_l2_l4_1t",
          "bank_64x512_c12_l4_r12_1t" };
  double bt[KREPS];
  double best[5];
  size_t recs = 0;
  for (int which = 0; which < 5; which++)
    {
      best[which] = 1e30;
      for (int r = 0; r < KREPS; r++)
        {
          const uint64_t t0 = jm_bench_now_ns ();
          if (which == 0)
            dp_correlator_execute_power_max (dd, copies, H * n, pw, idx, n,
                                             NULL);
          else if (which == 1)
            dp_correlator_bank_power_max (dd, x, n, 4, -4, 7, NULL, 0, pw, idx,
                                          n, NULL, NULL);
          else if (which == 2)
            dp_correlator_bank_power_max (dd, x, n, 2, -2, 3, NULL, 0, pw, idx,
                                          n, NULL, NULL);
          else if (which == 4)
            dp_correlator_bank_power_max (dd, xr, n, 4, -4, 7, rates, R * ny,
                                          pw, idx, n, NULL, NULL);
          else
            recs = dp_correlator_bank_detect (dd, x, n, 2, 4, -2, 3, t, t,
                                              NULL, 0, rp, rg, ra, rr, rc, n,
                                              NULL, NULL);
          bt[r]       = jm_bench_elapsed_sec (t0, jm_bench_now_ns ());
          best[which] = bt[r] < best[which] ? bt[r] : best[which];
        }
      if (which == 4)
        {
          /* The rows timed must have found the planted emitter: lag 77,
             carrier g = +1 (a quarter bin, L = 4: h's carrier index
             1 - (-4) = 5) and rate RATE_IN, so h = 5 R + RATE_IN. A map
             that is fast because it is wrong aborts the bench. */
          size_t k = 0;
          for (size_t m = 1; m < n; m++)
            k = pw[m] > pw[k] ? m : k;
          if (k % nx != 77 || idx[k] != 5 * R + RATE_IN)
            {
              (void)fprintf (stderr,
                             "bench_corr2d: the rated bank's peak is lag "
                             "%zu, hypothesis %u, not lag 77, hypothesis "
                             "%d\n",
                             k % nx, (unsigned)idx[k], (int)(5 * R + RATE_IN));
              exit (1);
            }
        }
      jm_bench_add_unit (bench, name[which], bt, KREPS, n, JM_BENCH_SAMPLE);
      printf ("  %-36s %8.2f ms\n", name[which], best[which] * 1e3);
    }
  printf ("  the bank is %.2fx the corrected batch; L = 2 is %.0f%% of L = 4;"
          "\n  detect 2 -> 4 refines %zu candidate(s) for %.2f ms over L = 2"
          "\n",
          best[0] / best[1], 100.0 * best[2] / best[1], recs,
          (best[3] - best[2]) * 1e3);
  printf ("  %d rates cost %.2fx the bank without them\n", (int)R,
          best[4] / best[1]);
  dp_correlator_destroy (dd);
  free (code), free (x), free (copies), free (pw), free (idx), free (rp);
  free (rg), free (ra), free (rr), free (rc), free (rates), free (xr);
}

/* ── The 1-D flavor's rows: what a dwell buys, and what the dump costs.
 *
 * `dp_correlator_execute` has two prices behind one signature. Every call runs
 * a forward FFT of the frame, multiplies pointwise by the stored reference
 * spectrum and adds that into a coherent accumulator. Only the
 * `dwell`-th call also runs the INVERSE transform and normalises, and
 * only that call writes an output; the rest return 0 and look, from the
 * outside, exactly the same.
 *
 * So a caller choosing `dwell` is amortising one inverse FFT over N
 * frames, and the useful figure is not the average -- it is the two
 * prices side by side, plus what the average becomes at each dwell. This
 * file measures the accumulate-only call and the dump call separately
 * (by driving the same correlator to the frame before its dump, then
 * timing one of each), and reports the per-frame average that implies
 * for several dwells.
 *
 * Deferring the inverse is only legal because the integration is
 * COHERENT -- a complex sum, so summing the cross-spectra and inverting
 * once equals inverting each and summing, by linearity. A non-coherent
 * (magnitude) integration could not do this and would pay the inverse
 * every frame; the ratio here is exactly what that choice would cost.
 *
 * Two frame lengths, because the accumulate and the dump scale
 * differently: the accumulate is one FFT plus an O(n) multiply-add, the
 * dump adds a second FFT and an O(n) scale, so the ratio between them
 * should be close to flat in n and any drift in it is the O(n) parts
 * moving against the O(n log n) ones.
 *
 * Three short lengths besides (#2752 step 3.1b): the PN periods 31, 63 and
 * 127, where per-call overhead is a large share of the work. They are the
 * baseline the one correlator's single-row flavor must meet (design page
 * docs/design/correlator.md, U1). Each correlates an m-sequence against
 * itself delayed by PLANT samples, and checks after its timing that the
 * map peaks at that lag, or the bench fails.
 */

#define ITERATIONS_1D 100
#define N_LEN 2
#define DWELL 8

static const size_t frame_n[N_LEN] = { 1024, 16384 };

/* Dwells a caller might actually pick, for the amortisation table. */
#define N_DWELL 4
static const size_t dwells[N_DWELL] = { 1, 4, 16, 64 };

enum
{
  CFG_ACC,
  CFG_DUMP,
  N_KIND
};

static const char *kind_name[N_KIND]
    = { "execute[accumulate]", "execute[dump]" };

#define N_CFG (N_LEN * N_KIND)

/* The short rows: register widths 5, 6 and 7, so periods 31, 63 and 127. */
#define N_SMALL 3
static const uint32_t small_bits[N_SMALL] = { 5, 6, 7 };
#define PLANT 5 /* the input's delay, so the map's peak lag */

/* One call at these lengths is a few hundred ns, near the clock's own
   granularity, so a round times REPS calls. The accumulate rows run on a
   correlator whose dwell is REPS + 1, so no dump lands in the batch; the
   dump rows on one whose dwell is 1, so every call is a dump. */
#define REPS 64

/* The lag at which a correlator's next dump peaks: runs it from reset to
   its dump. Returns n (an impossible lag) if no dump came. */
static size_t
dump_peak (dp_correlator_state_t *c, size_t dwell, const float _Complex *in,
           size_t n, float _Complex *out)
{
  dp_correlator_reset (c);
  size_t got = 0;
  for (size_t f = 0; f < dwell; f++)
    got = dp_correlator_execute (c, in, n, out, n, NULL);
  if (got != n)
    return n;
  size_t peak = 0;
  for (size_t k = 1; k < n; k++)
    if (cabsf (out[k]) > cabsf (out[peak]))
      peak = k;
  return peak;
}

/* The short rows: REPS calls a round, then checked. Each row's correlator
   runs once more to its dump, whose magnitude must peak at PLANT. Returns 0,
   or 1 when a row's map does not show what it was timed computing. */
static int
bench_small (jm_bench_t *bench)
{
  static double t[N_SMALL * N_KIND][ITERATIONS_1D];
  float _Complex ref[127], in[127], out[127];
  char name[64];
  for (int s = 0; s < N_SMALL; s++)
    {
      const size_t n = dp_bench_pn_nrz (small_bits[s], ref, 127);
      if (n == 0)
        return 1;
      for (size_t i = 0; i < n; i++)
        in[i] = ref[(i + n - PLANT) % n];
      dp_correlator_state_t *acc
          = dp_correlator_create_1d (ref, n, REPS + 1, 1, n);
      dp_correlator_state_t *dump = dp_correlator_create_1d (ref, n, 1, 1, n);
      if (!acc || !dump)
        return 1;
      for (int r = 0; r < ITERATIONS_1D; r++)
        {
          dp_correlator_reset (acc);
          uint64_t t0 = jm_bench_now_ns ();
          for (int q = 0; q < REPS; q++)
            (void)dp_correlator_execute (acc, in, n, out, n, NULL);
          uint64_t t1                = jm_bench_now_ns ();
          t[s * N_KIND + CFG_ACC][r] = jm_bench_elapsed_sec (t0, t1);
          t0                         = jm_bench_now_ns ();
          for (int q = 0; q < REPS; q++)
            (void)dp_correlator_execute (dump, in, n, out, n, NULL);
          t1                          = jm_bench_now_ns ();
          t[s * N_KIND + CFG_DUMP][r] = jm_bench_elapsed_sec (t0, t1);
        }
      const size_t pa = dump_peak (acc, REPS + 1, in, n, out);
      const size_t pd = dump_peak (dump, 1, in, n, out);
      dp_correlator_destroy (acc);
      dp_correlator_destroy (dump);
      if (pa != PLANT || pd != PLANT)
        {
          (void)fprintf (stderr,
                         "bench_corr: n=%zu peaks at lag %zu (accumulate) "
                         "and %zu (dump), not %d\n",
                         n, pa, pd, PLANT);
          return 1;
        }
      for (int k = 0; k < N_KIND; k++)
        {
          (void)snprintf (name, sizeof name, "%s[n=%zu]", kind_name[k], n);
          dp_bench_record (bench, name, t[s * N_KIND + k], ITERATIONS_1D,
                           REPS * n, "sample");
        }
    }
  return 0;
}

/* The 1-D flavor's rows, in this binary because jm bench runs only
   bench_<component>_core; main calls this first. They share the file,
   not a second translation unit, so jm_bench.h is compiled once.
   Returns 0, or 1 when an allocation or a plant check fails. */
static int
bench_correlator_1d (jm_bench_t *bench)
{
  uint64_t               t0, t1;
  static double          t[N_CFG][ITERATIONS_1D];
  dp_correlator_state_t *corr[N_LEN] = { 0 };
  const size_t           n_max       = frame_n[N_LEN - 1];
  float _Complex        *ref = NULL, *in = NULL, *out = NULL;
  char                   name[64];

  ref = malloc (n_max * sizeof *ref);
  in  = malloc (n_max * sizeof *in);
  out = malloc (n_max * sizeof *out);
  if (!ref || !in || !out)
    return 1;

  /* A chirp reference and an offset copy as input: representative of the
     acquisition case, and never a value the transform can shortcut. */
  for (size_t i = 0; i < n_max; i++)
    {
      const double p = 1e-4 * (double)i * (double)i;
      ref[i]         = (float _Complex) (cos (p) + sin (p) * I);
      in[i]          = (float _Complex) (cos (p + 0.7) + sin (p + 0.7) * I);
    }

  for (int l = 0; l < N_LEN; l++)
    {
      corr[l]
          = dp_correlator_create_1d (ref, frame_n[l], DWELL, 1, frame_n[l]);
      if (!corr[l])
        return 1;
    }

  printf ("=== corr (coherent cross-correlator, dwell = %d) ===\n", DWELL);
  printf ("%d rounds, min over rounds\n\n", ITERATIONS_1D);

  DP_BENCH_SETTLE ((void)dp_correlator_execute (corr[0], in, frame_n[0], out,
                                                frame_n[0], NULL));

  /* Rounds outside, (length, call kind) inside. The dump/accumulate ratio
     is the file's whole output, so both halves of it must see the same
     machine.
     Each timed pair starts from a fresh cycle: reset, then advance to one
     frame short of the dump so the next call is an accumulate and the one
     after it is the dump. The advance is untimed. */
  for (int r = 0; r < ITERATIONS_1D; r++)
    for (int l = 0; l < N_LEN; l++)
      {
        const size_t n = frame_n[l];

        dp_correlator_reset (corr[l]);
        for (size_t f = 0; f + 2 < DWELL; f++)
          (void)dp_correlator_execute (corr[l], in, n, out, n, NULL);

        t0 = jm_bench_now_ns ();
        (void)dp_correlator_execute (corr[l], in, n, out, n, NULL);
        t1                         = jm_bench_now_ns ();
        t[l * N_KIND + CFG_ACC][r] = jm_bench_elapsed_sec (t0, t1);

        t0 = jm_bench_now_ns ();
        (void)dp_correlator_execute (corr[l], in, n, out, n, NULL);
        t1                          = jm_bench_now_ns ();
        t[l * N_KIND + CFG_DUMP][r] = jm_bench_elapsed_sec (t0, t1);
      }

  for (int l = 0; l < N_LEN; l++)
    for (int k = 0; k < N_KIND; k++)
      {
        (void)snprintf (name, sizeof name, "%s[n=%zu]", kind_name[k],
                        frame_n[l]);
        dp_bench_record (bench, name, t[l * N_KIND + k], ITERATIONS_1D,
                         frame_n[l], "sample");
      }

  printf ("\n  what the deferred inverse buys, per frame:\n");
  printf ("    %-8s %10s", "n", "dump/acc");
  for (int d = 0; d < N_DWELL; d++)
    printf ("   dwell=%-4zu", dwells[d]);
  printf ("\n");
  for (int l = 0; l < N_LEN; l++)
    {
      const double acc = dp_bench_min (t[l * N_KIND + CFG_ACC], ITERATIONS_1D);
      const double dump
          = dp_bench_min (t[l * N_KIND + CFG_DUMP], ITERATIONS_1D);
      printf ("    %-8zu %9.2fx", frame_n[l], dump / acc);
      for (int d = 0; d < N_DWELL; d++)
        printf ("  %7.2f ns", (acc * (double)(dwells[d] - 1) + dump)
                                  / (double)dwells[d] / (double)frame_n[l]
                                  * 1e9);
      printf ("\n");
    }
  printf ("  The dwell columns are ns/sample averaged over one cycle. A\n"
          "  non-coherent integrator pays the dwell=1 column at every\n"
          "  dwell, because it cannot defer the inverse at all.\n");

  for (int l = 0; l < N_LEN; l++)
    dp_correlator_destroy (corr[l]);
  free (ref);
  free (in);
  free (out);
  return bench_small (bench) != 0;
}

/* ── The detector flavors' rows (#2752 step 3.3) ───────────────────────────
 *
 * They were bench_detector_core and bench_detector2d_core, one binary each;
 * the two detectors are now the correlator's detector flavors, and jm runs
 * only bench_<component>_core, so their rows live here, with their names
 * unchanged. `make bench-ab` pairs them with main's by alias through step 3
 * (docs/design/correlator.md section 7.1). Macros are prefixed DET1_ and
 * DET2_ so the three benches' constants cannot collide in one file.
 *
 * The 1-D rows: what the detector adds to the correlator, and whether the
 * caller's chunk size is free.
 *
 * `dp_correlator_push` accepts a chunk of ANY length. It writes into a
 * double-mapped ring, drains whole n-sample frames through the correlator
 * underneath, and on each int-dump computes peak-over-noise and decides
 * whether to emit. Nothing in that signature warns that a caller feeding
 * 64 samples at a time might be paying for the privilege -- and a caller
 * bridging a hardware block size to a frame size has no choice about it.
 *
 * So the first axis is chunk size at constant total work: the same 65536
 * samples pushed as one call, as frame-sized calls, and as chunks far
 * smaller than a frame. The correlator work is identical in all three;
 * only the ring bookkeeping and the call count change.
 *
 * The second axis is what a DETECTION costs. Threshold 0.0 fires on every
 * dump; a threshold nothing can reach fires on none. Both compute the same
 * statistic -- the peak search over n bins and the noise aggregate over
 * the noise band -- so the difference is only the emit, and a large gap
 * would mean the emit path is doing more than it looks like.
 *
 * The noise mode is the third thing worth knowing and is deliberately NOT
 * swept here: "median" sorts a scratch copy of the noise band and the other
 * three are a single pass, so it is a different algorithm rather than a
 * configuration of this one. Left for whoever needs it, named here so the
 * absence is a decision rather than an oversight.
 *
 * Three short frames besides (#2752 step 3.1b): the PN periods 31, 63 and
 * 127, one frame a push, where per-call overhead is a large share of the
 * work. They are the baseline the 1-D detector flavor must meet
 * (docs/design/correlator.md, U1). Each detects an m-sequence delayed by
 * DET1_PLANT samples, and checks after its timing that a push reports one
 * hit at that lag, or the bench fails.
 */

#define DET1_ITERATIONS 60
#define DET1_FRAME 1024
#define DET1_TOTAL 65536
#define DET1_MAX_RESULTS 256
#define DET1_DWELL 1

/* Chunk sizes: well under a frame, exactly a frame, and many frames. */
#define DET1_N_CFG 4
static const size_t det1_chunk[DET1_N_CFG]
    = { 64, DET1_FRAME, 16384, DET1_FRAME };
/* The last configuration repeats the frame-sized chunk with a threshold
   nothing reaches, so the pair differs only in whether it emits. */
static const float det1_thresh[DET1_N_CFG] = { 0.0f, 0.0f, 0.0f, 1e30f };
#define DET1_FIRING_FRAME_IDX 1
#define DET1_SILENT_FRAME_IDX 3

/* The short rows: register widths 5, 6 and 7, so periods 31, 63 and 127. */
#define DET1_PLANT 5   /* the stream's delay, so the hit's lag */
#define DET1_REPS 64   /* frames a round, one a push */
#define DET1_GATE 3.0f /* peak over noise: an m-sequence clears it by far */

/* The short rows: DET1_REPS one-frame pushes a round, then checked. A fresh
   push of one frame must report exactly one hit, at DET1_PLANT. Returns 0,
   or 1 when a row does not detect what it was timed detecting. */
static int
bench_detector_small (jm_bench_t *bench)
{
  static double t[N_SMALL][DET1_ITERATIONS];
  static float _Complex in[DET1_REPS * 127];
  float _Complex ref[127];
  dp_correlator_hit_t hit[2];
  char                name[72];
  for (int s = 0; s < N_SMALL; s++)
    {
      const size_t n = dp_bench_pn_nrz (small_bits[s], ref, 127);
      if (n == 0)
        return 1;
      for (size_t i = 0; i < DET1_REPS * n; i++)
        in[i] = ref[(i % n + n - DET1_PLANT) % n];
      dp_correlator_state_t *d = dp_correlator_create_detector_1d (
          ref, n, 1, 1, n - 1, DET_NOISE_MEAN, DET1_GATE, 1);
      if (!d)
        return 1;
      for (int r = 0; r < DET1_ITERATIONS; r++)
        {
          dp_correlator_reset (d);
          const uint64_t t0 = jm_bench_now_ns ();
          for (int q = 0; q < DET1_REPS; q++)
            (void)dp_correlator_push (d, in + (size_t)q * n, n, hit, 2);
          const uint64_t t1 = jm_bench_now_ns ();
          t[s][r]           = jm_bench_elapsed_sec (t0, t1);
        }
      dp_correlator_reset (d);
      const size_t nh = dp_correlator_push (d, in, n, hit, 2);
      dp_correlator_destroy (d);
      if (nh != 1 || hit[0].col != DET1_PLANT)
        {
          (void)fprintf (stderr,
                         "bench_detector: frame=%zu gave %zu hit(s), lag "
                         "%zu, not one at %d\n",
                         n, nh, nh ? hit[0].col : 0, DET1_PLANT);
          return 1;
        }
      (void)snprintf (name, sizeof name, "push[frame=%zu]", n);
      dp_bench_record (bench, name, t[s], DET1_ITERATIONS, DET1_REPS * n,
                       "sample");
    }
  return 0;
}

static int
bench_detector_1d (jm_bench_t *bench)
{
  uint64_t               t0, t1;
  static double          t[DET1_N_CFG][DET1_ITERATIONS];
  dp_correlator_state_t *det[DET1_N_CFG] = { 0 };
  float _Complex        *ref = NULL, *in = NULL;
  dp_correlator_hit_t   *res = NULL;
  char                   name[72];

  ref = malloc (DET1_FRAME * sizeof *ref);
  in  = malloc (DET1_TOTAL * sizeof *in);
  res = malloc (DET1_MAX_RESULTS * sizeof *res);
  if (!ref || !in || !res)
    return 1;

  /* A chirp reference and the same chirp in the stream: a real peak, so
     the peak search and the noise aggregate both see representative data
     rather than a flat correlation surface. */
  for (size_t i = 0; i < DET1_FRAME; i++)
    {
      const double p = 1e-4 * (double)i * (double)i;
      ref[i]         = (float _Complex) (cos (p) + sin (p) * I);
    }
  for (size_t i = 0; i < DET1_TOTAL; i++)
    {
      const double p
          = 1e-4 * (double)(i % DET1_FRAME) * (double)(i % DET1_FRAME);
      in[i] = (float _Complex) (cos (p) + sin (p) * I);
    }

  for (int c = 0; c < DET1_N_CFG; c++)
    {
      det[c] = dp_correlator_create_detector_1d (
          ref, DET1_FRAME, DET1_DWELL, 1, DET1_FRAME - 1, DET_NOISE_MEAN,
          det1_thresh[c], 1);
      if (!det[c])
        return 1;
    }

  printf ("\n=== detector (corr + ring + peak/noise gate) ===\n");
  printf ("frame = %d, %d samples per round, %d rounds, min over rounds\n\n",
          DET1_FRAME, DET1_TOTAL, DET1_ITERATIONS);

  DP_BENCH_SETTLE ((void)dp_correlator_push (det[0], in, DET1_FRAME, res,
                                             DET1_MAX_RESULTS));

  /* Rounds outside, chunk sizes inside: three of these rows are read as
     multiples of a fourth, so one thermal step must not land on one. */
  for (int r = 0; r < DET1_ITERATIONS; r++)
    for (int c = 0; c < DET1_N_CFG; c++)
      {
        dp_correlator_reset (det[c]);
        t0 = jm_bench_now_ns ();
        for (size_t off = 0; off < DET1_TOTAL; off += det1_chunk[c])
          (void)dp_correlator_push (det[c], in + off, det1_chunk[c], res,
                                    DET1_MAX_RESULTS);
        t1      = jm_bench_now_ns ();
        t[c][r] = jm_bench_elapsed_sec (t0, t1);
      }

  for (int c = 0; c < DET1_N_CFG; c++)
    {
      (void)snprintf (name, sizeof name, "push[chunk=%zu%s]", det1_chunk[c],
                      det1_thresh[c] > 0.0f ? ",no-detections" : "");
      dp_bench_record (bench, name, t[c], DET1_ITERATIONS, DET1_TOTAL,
                       "sample");
    }

  printf ("\n  chunk size at constant work (%d-sample chunks = 1.00x):\n",
          DET1_FRAME);
  for (int c = 0; c < DET1_N_CFG; c++)
    {
      if (c == DET1_SILENT_FRAME_IDX)
        continue;
      printf ("    %6zu samples per push   %.2fx   (%zu calls)\n",
              det1_chunk[c],
              dp_bench_min (t[c], DET1_ITERATIONS)
                  / dp_bench_min (t[DET1_FIRING_FRAME_IDX], DET1_ITERATIONS),
              (size_t)DET1_TOTAL / det1_chunk[c]);
    }
  printf ("\n  emitting a detection on every dump costs %.2fx over\n"
          "  emitting none, at the same chunk size and the same\n"
          "  statistic. Both compute peak-over-noise; only one writes.\n",
          dp_bench_min (t[DET1_FIRING_FRAME_IDX], DET1_ITERATIONS)
              / dp_bench_min (t[DET1_SILENT_FRAME_IDX], DET1_ITERATIONS));

  for (int c = 0; c < DET1_N_CFG; c++)
    dp_correlator_destroy (det[c]);
  free (ref);
  free (in);
  free (res);
  return bench_detector_small (bench);
}

/* The 2-D rows: the shape penalty, one layer up from the FFT.
 *
 * `bench_fft2d_core.c` shows that a separable 2-D transform is not priced
 * by its bin count: at a constant number of bins, an elongated array costs
 * well over a square one, because one of the two passes strides. These rows
 * ask whether that survives the detector wrapped around it -- the ring,
 * the peak search over the whole surface, and the noise aggregate over the
 * noise band are all O(n) and identical across shapes, so they dilute the
 * transform's shape penalty by a factor this measures rather than predicts.
 *
 * That matters because the caller's choice is real. A range-Doppler map is
 * "pulses by range gates"; how those multiply out is set by the waveform,
 * and a designer trading PRF against gate count is choosing this row
 * whether or not they know it.
 *
 * Three shapes at a constant 16384 bins, plus one repeat of the square
 * shape with a threshold nothing reaches -- so the pair differs only in
 * whether a detection is emitted. In 1-D that pair came out free; a 2-D
 * emit carries a row and a column instead of one lag, and this says
 * whether that changes anything.
 */

#define DET2_ITERATIONS 60
#define DET2_BINS 16384
#define DET2_FRAMES 4
#define DET2_MAX_RESULTS 64
#define DET2_DWELL 1

#define DET2_N_CFG 4
static const size_t det2_ny[DET2_N_CFG] = { 16, 128, 1024, 128 };
static const size_t det2_nx[DET2_N_CFG] = { 1024, 128, 16, 128 };
/* The last configuration repeats the square shape with a threshold nothing
   reaches, so the pair differs only in whether it emits. */
static const float det2_thresh[DET2_N_CFG] = { 0.0f, 0.0f, 0.0f, 1e30f };
#define DET2_SQUARE_IDX 1
#define DET2_SILENT_IDX 3

static int
bench_detector_2d (jm_bench_t *bench)
{
  uint64_t               t0, t1;
  static double          t[DET2_N_CFG][DET2_ITERATIONS];
  dp_correlator_state_t *det[DET2_N_CFG] = { 0 };
  float _Complex        *ref = NULL, *in = NULL;
  dp_correlator_hit_t   *res = NULL;
  char                   name[72];

  ref = malloc (DET2_BINS * sizeof *ref);
  in  = malloc ((size_t)DET2_FRAMES * DET2_BINS * sizeof *in);
  res = malloc (DET2_MAX_RESULTS * sizeof *res);
  if (!ref || !in || !res)
    return 1;

  /* A chirp reference and the same chirp in the stream: a real peak, so
     the surface search and the noise aggregate see representative data. */
  for (size_t i = 0; i < DET2_BINS; i++)
    {
      const double p = 1e-5 * (double)i * (double)i;
      ref[i]         = (float _Complex) (cos (p) + sin (p) * I);
    }
  for (size_t i = 0; i < (size_t)DET2_FRAMES * DET2_BINS; i++)
    {
      const double q
          = 1e-5 * (double)(i % DET2_BINS) * (double)(i % DET2_BINS);
      in[i] = (float _Complex) (cos (q) + sin (q) * I);
    }

  for (int c = 0; c < DET2_N_CFG; c++)
    {
      det[c] = dp_correlator_create_detector_2d (
          ref, det2_ny[c], det2_nx[c], DET2_DWELL, 1, DET2_BINS - 1,
          DET_NOISE_MEAN, det2_thresh[c], 1);
      if (!det[c])
        return 1;
    }

  printf ("\n=== detector2d (fft2d corr + ring + peak/noise gate) ===\n");
  printf ("%d bins per frame, %d frames per round, %d rounds, min over "
          "rounds\n\n",
          DET2_BINS, DET2_FRAMES, DET2_ITERATIONS);

  DP_BENCH_SETTLE ((void)dp_correlator_push (
      det[DET2_SQUARE_IDX], in, DET2_BINS, res, DET2_MAX_RESULTS));

  /* Rounds outside, shapes inside: three shapes of one bin count are read
     against each other, so drift must land on all of them. */
  for (int r = 0; r < DET2_ITERATIONS; r++)
    for (int c = 0; c < DET2_N_CFG; c++)
      {
        dp_correlator_reset (det[c]);
        t0 = jm_bench_now_ns ();
        for (int f = 0; f < DET2_FRAMES; f++)
          (void)dp_correlator_push (det[c], in + (size_t)f * DET2_BINS,
                                    DET2_BINS, res, DET2_MAX_RESULTS);
        t1      = jm_bench_now_ns ();
        t[c][r] = jm_bench_elapsed_sec (t0, t1);
      }

  for (int c = 0; c < DET2_N_CFG; c++)
    {
      (void)snprintf (name, sizeof name, "push[%zux%zu%s]", det2_ny[c],
                      det2_nx[c],
                      det2_thresh[c] > 0.0f ? ",no-detections" : "");
      dp_bench_record (bench, name, t[c], DET2_ITERATIONS,
                       (size_t)DET2_FRAMES * DET2_BINS, "sample");
    }

  printf ("\n  shape cost at a constant %d bins (%zux%zu = 1.00x):\n",
          DET2_BINS, det2_ny[DET2_SQUARE_IDX], det2_nx[DET2_SQUARE_IDX]);
  for (int c = 0; c < DET2_N_CFG; c++)
    {
      if (c == DET2_SILENT_IDX)
        continue;
      printf ("    %4zu x %-5zu  %.2fx\n", det2_ny[c], det2_nx[c],
              dp_bench_min (t[c], DET2_ITERATIONS)
                  / dp_bench_min (t[DET2_SQUARE_IDX], DET2_ITERATIONS));
    }
  printf ("  Compare this spread against bench_fft2d_core's. The detector\n"
          "  adds O(n) work that is identical across shapes, so a SMALLER\n"
          "  spread here than there is the wrapper diluting the\n"
          "  transform's penalty, not the penalty going away.\n");
  printf ("\n  emitting a detection on every dump costs %.2fx over\n"
          "  emitting none, same shape, same statistic.\n",
          dp_bench_min (t[DET2_SQUARE_IDX], DET2_ITERATIONS)
              / dp_bench_min (t[DET2_SILENT_IDX], DET2_ITERATIONS));

  for (int c = 0; c < DET2_N_CFG; c++)
    dp_correlator_destroy (det[c]);
  free (ref);
  free (in);
  free (res);
  return 0;
}

/* ── The search flavors' rows (#2752 step 3.4b) ──────────────────────────
 * They were bench_acq_core, which jm stopped running when acq became the
 * correlator's search flavors; bench-ab pairs them with main's through
 * ALIAS=bench_acq_core=bench_correlator_core, row for row.
 *
 * The code-phase searcher end to end in C: one timed
 * dp_correlator_push() per round, each pushing exactly one full non-coherent
 * dwell (n_noncoh * D epochs) of an injected signal in AWGN, through the real
 * auto-sizer, CFAR gate and peak report. Built via
 * dp_correlator_create_continuous(), the continuous/async engine that
 * window-tiles a Doppler uncertainty into roll-FFT frequency hypotheses off
 * one shared forward FFT per epoch (correlator_core.h, "Wideband window-tiling
 * mode").
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

#define SEARCH_ITERATIONS 15
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

/* The API default (objects/correlator.toml); see the file comment for why one.
 */
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

/* Unit-variance complex Gaussian (Box-Muller); same generator as
 * test_correlator_search.c's cgauss -- E|z|^2 = 1. */
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

static int
bench_search (jm_bench_t *bench)
{
  const double PI = acos (-1.0);
  const size_t nx = SF * SPC; /* code_bins = 2046. */

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
          dp_correlator_state_t *a = dp_correlator_create_continuous (
              code, SF, cfg->chip_rate * (double)SPC, cfg->chip_rate,
              SYMBOL_RATE, CN0_DBHZ, cfg->du, PFA, pd_target, 0,
              cfg->code_only_epochs, cfg->code_only_epochs > 1 ? 500.0 : 0.0);
          if (!a)
            {
              fprintf (stderr,
                       "dp_correlator_create_continuous failed at pd=%.3f\n",
                       pd_target);
              continue;
            }
          /* expect_bins is odd by construction: window sizing is against the
           * spacing bins are actually reported at (doppler_res_hz = 2*span),
           * and forced odd so coverage is symmetric and no ambiguous n/2
           * index exists -- see acq_cover_window_bins() / dp_fftfreq_index().
           */
          (void)dp_correlator_set_threads (a, cfg->threads);
          if (cfg->carrier_hz > 0.0
              && dp_correlator_set_carrier_freq_hz (a, cfg->carrier_hz)
                     != DP_OK)
            {
              fprintf (stderr,
                       "dp_correlator_set_carrier_freq_hz refused %g Hz\n",
                       cfg->carrier_hz);
              dp_correlator_destroy (a);
              return 1;
            }
          const size_t D = a->search->coherent_bins;
          if ((cfg->code_only_epochs == 1 && D != 1)
              || a->search->window_bins != cfg->expect_bins)
            {
              fprintf (stderr,
                       "unexpected grid at pd=%.3f: coherent_bins=%zu "
                       "window_bins=%zu\n",
                       pd_target, a->search->coherent_bins,
                       a->search->window_bins);
              dp_correlator_destroy (a);
              continue;
            }
          const size_t nc = a->search->n_noncoh;
          const size_t n_in
              = a->search->n_noncoh * D * nx; /* a block per look */
          /* Same shared helper the engine itself uses, so the injected tone
             and the reported bin can never disagree about a row's sign. */
          const long signed_r
              = dp_fftfreq_index (cfg->inject_window, a->search->window_bins);
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

          dp_correlator_hit_t hits[4];
          size_t nh = dp_correlator_push (a, buf, n_in, hits, 4); /* warm-up */
          /* The lag column nearest the injected phase: on a padded lag
             grid a column is code_bins / lag_bins of a sample. */
          const size_t want_col
              = (size_t)lround ((double)INJECT_PHASE
                                * (double)a->search->lag_bins
                                / (double)a->search->code_bins)
                % a->search->lag_bins;
          /* With a carrier the code drifts (dil - 1) * nx samples an
             epoch, and the ramp reads the block at its middle, so the peak
             may sit up to half the block's drift from the injected phase,
             either way: that many columns, plus one for the rounding. */
          const size_t drift
              = dil != 1.0
                    ? (size_t)ceil (0.5 * (double)(D - 1) * fabs (dil - 1.0)
                                    * (double)nx * (double)a->search->lag_bins
                                    / (double)a->search->code_bins)
                          + 1
                    : 0;
          size_t off_col = nh ? (hits[0].col + a->search->lag_bins - want_col)
                                    % a->search->lag_bins
                              : 0;
          if (off_col > a->search->lag_bins / 2)
            off_col = a->search->lag_bins - off_col;
          int ok = (nh == 1 && hits[0].row == cfg->inject_window * D
                    && off_col <= drift);
          /* The schedule the one delay-Doppler map runs this row in
             (docs/design/acq-surface.md §18.1), as the rule picks it. */
          const int sch = dp__dd_map_pick (
              a->dd_map,
              a->search->window_bins > 1 ? a->search->window_bins : 1,
              search_rate_count_at (a->search->rate_hyps,
                                    a->search->coherent_bins),
              0);
          printf ("n_noncoh=%3zu D=%zu threads=%d pd_predicted=%.4f  "
                  "detect=%s  doppler_bin=%zu code_phase=%zu "
                  "cn0_dbhz_est=%.2f  schedule=%s\n",
                  nc, D, a->search->threads, a->search->pd_predicted,
                  ok ? "yes" : "NO", nh ? hits[0].row : 0,
                  nh ? hits[0].col : 0, nh ? hits[0].cn0_dbhz_est : 0.0f,
                  a->search->coherent_bins * a->search->interp <= 1
                      ? "rows only (D = 1)"
                  : sch == DP__DD_ROWS_FIRST ? "rows first"
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

          double times[SEARCH_ITERATIONS];
          for (int r = 0; r < SEARCH_ITERATIONS; r++)
            {
              uint64_t t0, t1;
              t0       = jm_bench_now_ns ();
              nh       = dp_correlator_push (a, buf, n_in, hits, 4);
              t1       = jm_bench_now_ns ();
              times[r] = jm_bench_elapsed_sec (t0, t1);
              if (nh != 1)
                fprintf (stderr, "  iter %d: unexpected nh=%zu\n", r, nh);
            }

          double sum = 0.0, mn = times[0], mx = times[0];
          for (int r = 0; r < SEARCH_ITERATIONS; r++)
            {
              sum += times[r];
              if (times[r] < mn)
                mn = times[r];
              if (times[r] > mx)
                mx = times[r];
            }
          double mean = sum / SEARCH_ITERATIONS;
          printf ("  latency: mean=%.2f ms  min=%.2f ms  max=%.2f ms  "
                  "(%zu epochs/dwell, %.3f ms/epoch, %.1f ns/sample min, "
                  "%.2fx real time on one core)\n\n",
                  mean * 1e3, mn * 1e3, mx * 1e3, a->search->n_noncoh * D,
                  mean * 1e3 / (double)(a->search->n_noncoh * D),
                  mn / (double)n_in * 1e9, mn / (double)n_in * fs);

          char name[JM_BENCH_NAME_LEN];
          snprintf (name, sizeof (name), "%s_nc%zu", cfg->label, nc);
          jm_bench_add_unit (bench, name, times, SEARCH_ITERATIONS, (int)n_in,
                             JM_BENCH_SAMPLE);

          free (buf);
          dp_correlator_destroy (a);
        }
    }

  return 0;
}

/* The burst search's always-on cost, per sample of search: the rows
   bench_burst_acq_core recorded (c:burst_acq::push[...]) until burst_acq
   retired in #2752 step 4, carried here under their names
   (benchmarks/renames.toml maps c:burst_acq to c:correlator).
 *
 * A burst search is a DETECTOR, not a tracker: it runs over the input looking
 * for a preamble that may never arrive, so it pays its cost whether or not
 * there is a signal. That makes its per-sample number the floor of any
 * always-on receiver, and the one that decides how many channels a box can
 * watch at once.
 *
 * Spreading factor and samples-per-chip set the size of the search together,
 * through the code period in samples (sf * spc). The rows are the period's two
 * ends, 30 samples (sf 15, spc 2) and 252 (sf 63, spc 4) (doppler#2386).
 * Every timed push must consume its whole block: the engine stops taking
 * input when its result slots fill, and a short push would read fast. */
#define BURST_N 32768
#define BURST_ITERS 50
#define BURST_WARMUP_S 0.25
#define BURST_MAX_RESULTS 16

static int
bench_burst_push (jm_bench_t *bench)
{
  float _Complex      *x   = malloc (BURST_N * sizeof *x);
  dp_correlator_hit_t *res = malloc (BURST_MAX_RESULTS * sizeof *res);
  if (!x || !res)
    return 1;
  /* Unit-variance complex Gaussian noise (Box-Muller), the case a detector
     listens to between bursts. A structured +/-1 sequence is not: the CFAR
     reads it as a stream of detections, the result slots fill, and the push
     stops early (the consumption check below). */
  uint32_t st = 0x77C3u;
  for (int i = 0; i < BURST_N; i++)
    {
      const double u1  = ((double)_xorshift32 (&st) + 1.0) / 4294967297.0;
      const double u2  = ((double)_xorshift32 (&st) + 1.0) / 4294967297.0;
      const double mag = sqrt (-log (u1));
      const double th  = 6.283185307179586 * u2;
      x[i]             = (float)(mag * cos (th)) + (float)(mag * sin (th)) * I;
    }

  const size_t    sfs[2]  = { 15, 63 };
  const size_t    spcs[2] = { 2, 4 };
  static double   t_ps[2][BURST_ITERS];
  static uint8_t  code[64];
  volatile size_t sink = 0;
  for (int k = 0; k < 2; k++)
    {
      const size_t sf = sfs[k], spc = spcs[k];
      for (size_t i = 0; i < sf; i++)
        code[i] = (uint8_t)(((i * 2246822519u) >> 31) & 1u);
      /* The preamble is the code's samples: bin_to_nrz, held spc. */
      static float nrz[64];
      static float _Complex pre[64 * 4];
      (void)dp_bin_to_nrz (code, sf, nrz, 64);
      for (size_t i = 0; i < sf * spc; i++)
        pre[i] = nrz[i / spc];
      dp_correlator_state_t *a
          = dp_correlator_create_burst (pre, sf * spc, 8, 1.0e6 * (double)spc,
                                        65.0, 0.0, 1e-2, 0.9, 0, 0.0, 1);
      if (!a)
        {
          (void)fprintf (stderr, "burst push: create(sf=%zu, spc=%zu) NULL\n",
                         sf, spc);
          return 1;
        }
      uint64_t w0 = jm_bench_now_ns (), w1;
      do
        {
          dp_correlator_reset (a);
          sink += dp_correlator_push (a, x, BURST_N, res, BURST_MAX_RESULTS);
          w1 = jm_bench_now_ns ();
        }
      while (jm_bench_elapsed_sec (w0, w1) < BURST_WARMUP_S);
      for (int r = 0; r < BURST_ITERS; r++)
        {
          dp_correlator_reset (a);
          const uint64_t t0 = jm_bench_now_ns ();
          sink += dp_correlator_push (a, x, BURST_N, res, BURST_MAX_RESULTS);
          const uint64_t t1 = jm_bench_now_ns ();
          t_ps[k][r]        = jm_bench_elapsed_sec (t0, t1);
          if (dp_correlator_consumed (a) != BURST_N)
            {
              (void)fprintf (stderr,
                             "burst push(sf=%zu, spc=%zu) took %zu of %d "
                             "samples -- the row would credit samples it "
                             "never read\n",
                             sf, spc, dp_correlator_consumed (a), BURST_N);
              return 1;
            }
        }
      char name[64];
      (void)snprintf (name, sizeof name, "push[sf=%zu,spc=%zu]", sf, spc);
      jm_bench_add_unit (bench, name, t_ps[k], BURST_ITERS, BURST_N,
                         JM_BENCH_SAMPLE);
      dp_correlator_destroy (a);
    }
  (void)sink;
  free (x);
  free (res);
  return 0;
}

int
main (void)
{
  jm_bench_t _bench = { 0 };
  uint64_t   t0, t1;
  double     times[ITERATIONS];

  if (bench_correlator_1d (&_bench) != 0)
    return 1;

  /* ny=16, nx=2046 mirrors docs/design/acq-surface.md §14's worked
   * case (L=1023, spc=2 -> code_bins=2046; doppler_bins=16). */
  const size_t ny = 16, nx = 2046, n = ny * nx;
  uint32_t     seed = 42;

  printf ("=== corr2d benchmark ===\n");
  printf ("grid = %zu x %zu (%zu samples/frame), %d iterations\n\n", ny, nx, n,
          ITERATIONS);

  float _Complex *in  = malloc (n * sizeof *in);
  float _Complex *out = malloc (n * sizeof *out);
  for (size_t k = 0; k < n; k++)
    in[k] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;

  /* ── single-row reference: the acq/detector2d shape -- fast path ──────── */
  {
    float _Complex *ref = calloc (n, sizeof *ref);
    for (size_t j = 0; j < nx; j++)
      ref[j] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;

    dp_correlator_state_t *obj
        = dp_correlator_create_2d (ref, ny, nx, 1, 1, 0, 0, -1);
    dp_correlator_execute (obj, in, n, out, n, NULL); /* warmup */
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        dp_correlator_execute (obj, in, n, out, n, NULL);
        t1       = jm_bench_now_ns ();
        times[r] = jm_bench_elapsed_sec (t0, t1);
      }
    jm_bench_add_unit (&_bench, "single_row_fast_path", times, ITERATIONS, n,
                       JM_BENCH_SAMPLE);
    double sum = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      sum += times[r];
    printf ("  single-row (fast path)    %8.1f MSa/s  (fast_path=%d)\n",
            (double)n / (sum / ITERATIONS) / 1e6, obj->fast_path);

    free (ref);
    dp_correlator_destroy (obj);
  }

  /* ── genuinely multi-row reference -- general 2-D path ────────────────── */
  {
    float _Complex *ref = malloc (n * sizeof *ref);
    for (size_t k = 0; k < n; k++)
      ref[k] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;

    dp_correlator_state_t *obj
        = dp_correlator_create_2d (ref, ny, nx, 1, 1, 0, 0, -1);
    dp_correlator_execute (obj, in, n, out, n, NULL); /* warmup */
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        dp_correlator_execute (obj, in, n, out, n, NULL);
        t1       = jm_bench_now_ns ();
        times[r] = jm_bench_elapsed_sec (t0, t1);
      }
    jm_bench_add_unit (&_bench, "multi_row_general_path", times, ITERATIONS, n,
                       JM_BENCH_SAMPLE);
    double sum = 0.0;
    for (int r = 0; r < ITERATIONS; r++)
      sum += times[r];
    printf ("  multi-row (general path)  %8.1f MSa/s  (fast_path=%d)\n",
            (double)n / (sum / ITERATIONS) / 1e6, obj->fast_path);

    free (ref);
    dp_correlator_destroy (obj);
  }

  free (in);
  free (out);

  /* ── delay-Doppler flavor (#2666): raw stream in, centered map out ──────
   * The row correlations of the fast path above, then the slow-time FFT
   * down every delay column, at 1 thread and every online core (0). */
  {
    const struct
    {
      size_t      ny, nx;
      int         nthreads;
      size_t      ny_out; /* 0 = ny; 2 ny interpolates the Doppler axis */
      const char *name;
    } dd_cases[] = {
      { 32, 1024, 1, 0, "delay_doppler_32x1024_1t" },
      { 256, 4096, 1, 0, "delay_doppler_256x4096_1t" },
      { 256, 4096, 0, 0, "delay_doppler_256x4096_all" },
      /* Doppler interpolated twofold, an arbitrary factor (any ny_out > ny
         changes the count): the one map (#2752 step 2.3) takes slow time
         first and pays ny_out inverses where today pays ny. Its input
         carries a planted emitter, checked before timing. */
      { 32, 1024, 1, 64, "delay_doppler_32x1024_o64_1t" },
    };
    for (size_t c = 0; c < sizeof dd_cases / sizeof *dd_cases; c++)
      {
        const size_t dny = dd_cases[c].ny, dnx = dd_cases[c].nx;
        const size_t dn = dny * dnx;
        const size_t dno
            = (dd_cases[c].ny_out ? dd_cases[c].ny_out : dny) * dnx;
        float _Complex *code = malloc (dnx * sizeof *code);
        float _Complex *x    = malloc (dn * sizeof *x);
        float _Complex *map  = malloc (dno * sizeof *map);
        for (size_t j = 0; j < dnx; j++)
          code[j] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;
        for (size_t k = 0; k < dn; k++)
          x[k] = _rand_uniform (&seed) + _rand_uniform (&seed) * I;
        /* The interpolated case: the code at lag 333, turning 10/64 of a
           cycle a period, so its peak is Doppler bin 10 of the 64-row
           grid, centered at row 10 + 32 = 42. */
        if (dd_cases[c].ny_out)
          for (size_t i = 0; i < dny; i++)
            for (size_t p = 0; p < dnx; p++)
              x[i * dnx + p] += 4.0f * code[(p + dnx - 333) % dnx]
                                * cexpf (I
                                         * (float)(2.0 * 3.14159265358979323846
                                                   * 10.0 * (double)i / 64.0));
        dp_correlator_state_t *dd = dp_correlator_create_delay_doppler (
            code, dnx, dny, 1, dd_cases[c].nthreads, dd_cases[c].ny_out);
        dp_correlator_execute (dd, x, dn, map, dno, NULL); /* warmup */
        if (dd_cases[c].ny_out)
          {
            size_t k = 0;
            for (size_t m = 1; m < dno; m++)
              k = cabsf (map[m]) > cabsf (map[k]) ? m : k;
            if (k / dnx != 42 || k % dnx != 333)
              {
                (void)fprintf (stderr,
                               "bench_corr2d: %s peaks at row %zu, lag %zu, "
                               "not row 42, lag 333\n",
                               dd_cases[c].name, k / dnx, k % dnx);
                return 1;
              }
          }
        double sum = 0.0;
        for (int r = 0; r < ITERATIONS; r++)
          {
            t0 = jm_bench_now_ns ();
            dp_correlator_execute (dd, x, dn, map, dno, NULL);
            t1       = jm_bench_now_ns ();
            times[r] = jm_bench_elapsed_sec (t0, t1);
            sum += times[r];
          }
        jm_bench_add_unit (&_bench, dd_cases[c].name, times, ITERATIONS, dn,
                           JM_BENCH_SAMPLE);
        printf ("  %-28s %8.2f ms  (%d workers)\n", dd_cases[c].name,
                sum / ITERATIONS * 1e3, dd->nworkers);
        dp_correlator_destroy (dd);
        free (code);
        free (x);
        free (map);
      }
  }

  bench_batches (&_bench, &seed);
  bench_bank (&_bench, &seed);

  if (bench_detector_1d (&_bench) != 0 || bench_detector_2d (&_bench) != 0)
    return 1;

  if (bench_search (&_bench) != 0)
    return 1;
  if (bench_burst_push (&_bench) != 0)
    return 1;
  jm_bench_write_json (&_bench, "correlator");
  return 0;
}
