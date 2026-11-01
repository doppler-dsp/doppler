/**
 * @file async_dsss_near_far.c
 * @brief How far below a stronger emitter a weaker one is still taken, over
 *        the operating range: the weak one at the specification's floor,
 *        the strong one up to 20 dB above it.
 *
 * Rule 2 of the receiver's pick (async_dsss_receiver_core.c, adr_pick())
 * takes a second emitter only if its statistic clears what the stronger
 * emitters' sidelobes can read at its cell by the gate. This measures what
 * that leaves a caller, at the operating point of the certification
 * (async_dsss_receiver_common.h: Gold-1023 at 3.069 Mcps, two samples a
 * chip, 2,700 sym/s): the weak emitter W at the floor, Es/N0 5 dB (39.3
 * dB-Hz, docs/design/async-dsss-receiver.md §2.1), the strong one S clean
 * and `delta` dB above it, both carrying data, at independent random code
 * phases and carriers inside the native span. Every receiver is sized for
 * the floor.
 *
 * Per trial, three searches on one stream:
 *
 *   truth  a rank-0 receiver on W alone (with W's noise) and one on S alone
 *          give each emitter's code phase at the hand-off;
 *   rank 1 a rank-1 receiver on S + W: the second emitter rule 2 lets
 *          through. Taken when it seeds within a chip of W -- or of S,
 *          when W read the stronger and rank 0 took it, which a few dB
 *          apart noise and straddle decide (counted as `swapped`); a
 *          SIDELOBE when it seeds anywhere else -- the error rule 2 exists
 *          to prevent.
 *
 * and, per strong level, the same rank-1 search on S alone (W's noise, no
 * W): every seed there is a sidelobe or a noise false alarm.
 *
 * Two searches: one epoch deep over data (D = 1, the certification's own
 * receiver), and the specification frame's deep block (D = 90 inside 450
 * code-only symbols of 500, the receivers' slow-time search). Each row is a
 * share of trials with its Wilson 95% interval (dp_stats_test.h).
 *
 * Gated, in --check and the sweep alike: no sidelobe is ever taken, with
 * or without W, at any level; and in --check, at D = 1, W is taken in
 * every valid trial at 10 dB. The rest is measured, not claimed: the reach is
 * a finding for the design page's near-far table, and the deep block keeps
 * the code's worst sidelobe as a floor under its per-cell bound until a
 * strong emitter's data splatter is modelled (doppler#2730).
 *
 * Usage:
 *   validate_async_dsss_near_far           the sweep: 0..20 dB, both depths
 *   validate_async_dsss_near_far --check   the regression points: D = 1 at
 *                                          10 and 20 dB, 6 trials each
 *   ... --emit                             also a `# near_far` CSV block
 */
#include "async_dsss_receiver_common.h"
#include "dp_dsss_test.h"
#include "dp_stats_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The specification's floor, docs/design/async-dsss-receiver.md §2.1. */
#define FLOOR_ESN0_DB 5.0
#define CODE_ONLY 450u /* the deep frame: code-only symbols ...       */
#define FRAME 500u     /* ... of every FRAME                          */

typedef struct
{
  int    deep;   /* 0: D = 1 over data; 1: the D = 90 frame       */
  size_t n_sym;  /* symbols a trial: room for the floor's dwells  */
  size_t trials; /* per level                                     */
} depth_t;

/* A receiver sized for the floor, the certification's flavor, one epoch
   deep, or the frame's deep block (code_only_s and a Doppler rate). */
static dp_async_dsss_receiver_state_t *
near_far_rx (const uint8_t *code, int deep, size_t rank)
{
  dp_async_dsss_receiver_state_t *r = dp_async_dsss_receiver_create (
      code, SF, CHIP_RATE, SYM_RATE, FS, 2, cn0_of (FLOOR_ESN0_DB), 1e-3, 0.9,
      SPAN, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false,
      deep ? (double)CODE_ONLY / SYM_RATE : 0.0, deep ? 500.0 : 0.0);
  if (r && rank)
    (void)dp_async_dsss_receiver_set_rank (r, rank);
  return r;
}

/* Push whole epochs until the receiver leaves its search; 1 and its seed
   phase if it did, 0 if the stream ran out first. */
static int
seeded (dp_async_dsss_receiver_state_t *r, const float _Complex *x, size_t n,
        double *phase)
{
  static float _Complex out[4096];
  for (size_t off = 0; off + (size_t)TE <= n; off += (size_t)TE)
    {
      (void)dp_async_dsss_receiver_steps (r, x + off, (size_t)TE, out, 4096);
      if (r->state != ASYNC_DSSS_RX_SEARCHING)
        {
          *phase = r->seed_chip_phase;
          return 1;
        }
    }
  return 0;
}

/* A uniform draw in [0, 1) from the trial's stream. */
static double
u01 (uint32_t *st)
{
  return (double)dp_xs32 (st) / 4294967296.0;
}

static double
chip_dist (double a, double b)
{
  double d = fabs (a - b);
  d        = fmod (d, (double)SF);
  return d > SF / 2.0 ? SF - d : d;
}

typedef struct
{
  size_t trials, valid, taken, swapped, side, alone_side;
} tally_t;

/* One trial at `delta` dB: the truth searches, rank 1 on S + W, and rank 1
   on S alone. A trial whose truth searches did not both seed, or whose two
   emitters landed within 50 chips of each other, is not counted (`valid`). */
static int
trial (const uint8_t *code, const depth_t *dp, double delta, uint32_t seed,
       tally_t *t)
{
  uint32_t        st   = trial_rng (seed);
  const double    tsym = FS / SYM_RATE;
  const double    fw   = (u01 (&st) - 0.5) * 0.8 * SPAN; /* inside the span */
  const double    fs_  = (u01 (&st) - 0.5) * 0.8 * SPAN;
  const size_t    pw   = (size_t)(u01 (&st) * (double)TE);
  const size_t    ps   = (size_t)(u01 (&st) * (double)TE);
  float _Complex *w, *s, *nz;
  size_t          nw, ns, nn;
  double         *data;
  const size_t    win = dp->deep ? CODE_ONLY : 0, fr = dp->deep ? FRAME : 0;
  /* W carries the floor's noise; S is rendered clean. The noise alone is
     W's capture at the same seed less W: rendered as a second capture of
     no emitter would draw it differently, so take it as W's own. */
  dp_dsss_windowed_capture (code, SF, SPC, FS, tsym, fw,
                            cn0_of (FLOOR_ESN0_DB), dp->n_sym, pw, seed, win,
                            fr, &w, &nw, &data);
  free (data);
  dp_dsss_windowed_capture (code, SF, SPC, FS, tsym, fw, 300.0, dp->n_sym, pw,
                            seed, win, fr, &nz, &nn, &data);
  free (data);
  dp_dsss_windowed_capture (code, SF, SPC, FS, tsym, fs_, 300.0, dp->n_sym, ps,
                            seed + 7777u, win, fr, &s, &ns, &data);
  free (data);
  size_t n              = nw < ns ? nw : ns;
  n                     = n < nn ? n : nn;
  const float     g     = (float)pow (10.0, delta / 20.0);
  float _Complex *both  = dp_xmalloc (n * sizeof *both);
  float _Complex *alone = dp_xmalloc (n * sizeof *alone);
  for (size_t i = 0; i < n; i++)
    {
      const float _Complex noise = w[i] - nz[i]; /* W's noise alone */
      both[i]                    = w[i] + g * s[i];
      alone[i]                   = noise + g * s[i];
      s[i]                       = noise + g * s[i];
    }
  double                          ph_w, ph_s, ph_1;
  dp_async_dsss_receiver_state_t *rw = near_far_rx (code, dp->deep, 0);
  dp_async_dsss_receiver_state_t *rs = near_far_rx (code, dp->deep, 0);
  DP_REQUIRE (rw && rs);
  const int ok = seeded (rw, w, n, &ph_w) && seeded (rs, s, n, &ph_s)
                 && chip_dist (ph_w, ph_s) > 50.0;
  dp_async_dsss_receiver_destroy (rw);
  dp_async_dsss_receiver_destroy (rs);
  t->trials++;
  if (ok)
    {
      t->valid++;
      dp_async_dsss_receiver_state_t *r1 = near_far_rx (code, dp->deep, 1);
      DP_REQUIRE (r1);
      if (seeded (r1, both, n, &ph_1))
        {
          /* The second emitter found: W, or S when W read the stronger
             and rank 0 took it (at a few dB apart, noise and straddle
             decide the order). A seed on neither is a sidelobe. */
          if (chip_dist (ph_1, ph_w) <= 1.0)
            t->taken++;
          else if (chip_dist (ph_1, ph_s) <= 1.0)
            {
              t->taken++;
              t->swapped++;
            }
          else
            t->side++;
        }
      dp_async_dsss_receiver_destroy (r1);
      dp_async_dsss_receiver_state_t *ra = near_far_rx (code, dp->deep, 1);
      DP_REQUIRE (ra);
      t->alone_side += (size_t)seeded (ra, alone, n, &ph_1);
      dp_async_dsss_receiver_destroy (ra);
    }
  free (w);
  free (s);
  free (nz);
  free (both);
  free (alone);
  return 0;
}

int
main (int argc, char **argv)
{
  int check = 0, emit = 0;
  for (int a = 1; a < argc; a++)
    {
      check |= strcmp (argv[a], "--check") == 0;
      emit |= strcmp (argv[a], "--emit") == 0;
    }
  uint8_t code[SF];
  dp_dsss_gold_1023 (code);

  const depth_t depths[2]
      = { { 0, 1500, check ? 6 : 40 }, { 1, 2000, check ? 0 : 20 } };
  static const double sweep[] = { 0, 4, 8, 10, 12, 14, 16, 18, 20 };
  static const double spot[]  = { 10, 20 };
  const double       *levels  = check ? spot : sweep;
  const size_t        n_lv    = check ? 2 : sizeof sweep / sizeof *sweep;

  if (emit)
    printf ("# near_far\ndepth,delta_db,trials,valid,taken,taken_lo,"
            "taken_hi,swapped,sidelobe,alone_sidelobe\n");
  printf ("near-far at the floor (W at Es/N0 %.0f dB, %.1f dB-Hz), S "
          "delta dB above:\n",
          FLOOR_ESN0_DB, cn0_of (FLOOR_ESN0_DB));
  for (int d = 0; d < 2; d++)
    for (size_t l = 0; l < n_lv && depths[d].trials; l++)
      {
        tally_t t = { 0 };
        for (size_t k = 0; k < depths[d].trials; k++)
          DP_REQUIRE (trial (code, &depths[d], levels[l],
                             1000u * (uint32_t)l + 31u * (uint32_t)k, &t)
                      == 0);
        double lo = 0.0, hi = 1.0;
        if (t.valid)
          dp_wilson ((double)t.taken, (double)t.valid, &lo, &hi);
        printf ("  %-5s %4.0f dB  W taken %3zu/%-3zu (%.2f-%.2f, %zu "
                "swapped)  sidelobe %zu, alone %zu\n",
                depths[d].deep ? "D=90" : "D=1", levels[l], t.taken, t.valid,
                lo, hi, t.swapped, t.side, t.alone_side);
        if (emit)
          printf ("%s,%.0f,%zu,%zu,%zu,%.4f,%.4f,%zu,%zu,%zu\n",
                  depths[d].deep ? "deep" : "one", levels[l], t.trials,
                  t.valid, t.taken, lo, hi, t.swapped, t.side, t.alone_side);
        /* The gates. A sidelobe taken is the defect rule 2 exists to
           prevent, at any level and either depth. */
        DP_CHECK (t.side == 0);
        DP_CHECK (t.alone_side == 0);
        DP_CHECK (t.valid * 2 >= t.trials); /* the truth searches ran */
        /* Measured (the sweep, 2026-11-01): one epoch deep, W is taken in
           every valid trial up to 10 dB; past it the reach falls off (21%
           at 20 dB), so 20 dB is gated on its sidelobes alone. */
        if (check && !depths[d].deep && levels[l] <= 10.0)
          DP_CHECK (t.taken == t.valid);
      }
  DP_TEST_END ("validate_async_dsss_near_far");
}
