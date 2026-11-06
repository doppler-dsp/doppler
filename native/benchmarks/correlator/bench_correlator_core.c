/* bench_correlator_core.c — single-row-reference fast path vs. the general
 * 2-D path, at a DSSS-acquisition-realistic grid.  A single-row reference
 * (the acq/detector2d shape: a code replica with no Doppler content of its
 * own) takes corr2d's fast path (ny independent length-nx 1-D FFTs); a
 * genuinely multi-row reference takes the general path (full (ny,nx) 2-D
 * FFT forward and inverse).  This is the durable regression gate for the
 * fast-path optimization (see docs/design/corr2d-interpolated-inverse.md).
 */
#include "doppler/correlator/correlator_core.h"
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
      dp_correlator_execute (dd, x, bn, maps, bn);
      dp_correlator_execute_all (dd, x, H * bn, maps, H * bn);
      for (int which = 0; which < 3; which++)
        {
          double best = 1e30;
          for (int r = 0; r < BREPS; r++)
            {
              const uint64_t t0 = jm_bench_now_ns ();
              if (which == 0)
                for (size_t h = 0; h < H; h++)
                  dp_correlator_execute (dd, x + h * bn, bn, maps + h * bn,
                                         bn);
              else if (which == 1)
                dp_correlator_execute_all (dd, x, H * bn, maps, H * bn);
              else
                dp_correlator_execute_power_max (dd, x, H * bn, pw, idx, bn);
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
  dp_correlator_bank_power_max (dd, x, n, 2, -2, 3, NULL, 0, pw, idx, n);
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
            dp_correlator_execute_power_max (dd, copies, H * n, pw, idx, n);
          else if (which == 1)
            dp_correlator_bank_power_max (dd, x, n, 4, -4, 7, NULL, 0, pw, idx,
                                          n);
          else if (which == 2)
            dp_correlator_bank_power_max (dd, x, n, 2, -2, 3, NULL, 0, pw, idx,
                                          n);
          else if (which == 4)
            dp_correlator_bank_power_max (dd, xr, n, 4, -4, 7, rates, R * ny,
                                          pw, idx, n);
          else
            recs = dp_correlator_bank_detect (dd, x, n, 2, 4, -2, 3, t, t,
                                              NULL, 0, rp, rg, ra, rr, rc, n);
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
    got = dp_correlator_execute (c, in, n, out, n);
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
            (void)dp_correlator_execute (acc, in, n, out, n);
          uint64_t t1                = jm_bench_now_ns ();
          t[s * N_KIND + CFG_ACC][r] = jm_bench_elapsed_sec (t0, t1);
          t0                         = jm_bench_now_ns ();
          for (int q = 0; q < REPS; q++)
            (void)dp_correlator_execute (dump, in, n, out, n);
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

  DP_BENCH_SETTLE (
      (void)dp_correlator_execute (corr[0], in, frame_n[0], out, frame_n[0]));

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
          (void)dp_correlator_execute (corr[l], in, n, out, n);

        t0 = jm_bench_now_ns ();
        (void)dp_correlator_execute (corr[l], in, n, out, n);
        t1                         = jm_bench_now_ns ();
        t[l * N_KIND + CFG_ACC][r] = jm_bench_elapsed_sec (t0, t1);

        t0 = jm_bench_now_ns ();
        (void)dp_correlator_execute (corr[l], in, n, out, n);
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
    dp_correlator_execute (obj, in, n, out, n); /* warmup */
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        dp_correlator_execute (obj, in, n, out, n);
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
    dp_correlator_execute (obj, in, n, out, n); /* warmup */
    for (int r = 0; r < ITERATIONS; r++)
      {
        t0 = jm_bench_now_ns ();
        dp_correlator_execute (obj, in, n, out, n);
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
        dp_correlator_execute (dd, x, dn, map, dno); /* warmup */
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
            dp_correlator_execute (dd, x, dn, map, dno);
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

  jm_bench_write_json (&_bench, "correlator");
  return 0;
}
