/**
 * @file async_dsss_pool_ghosts.c
 * @brief The pool's ghosts at the 415.1-B-1 point: what the ghost rule
 *        releases, what it leaves, and what admitting k = 0 would cost.
 *
 * The pool's ghost rule (docs/design/async-dsss-receiver.md §8) releases a
 * receiver that tracks a stronger holder's Doppler-offset sidelobe: its
 * carrier still on the holder's, a whole NONZERO number k of lattice steps
 * off, its code lock low. A ghost on its parent's own carrier (k = 0) is
 * left: a true emitter sharing a stronger one's carrier looks the same.
 * The test of that choice is two populations, and a third says where k = 0
 * ghosts come from at all.
 *
 * Stimulus: the use-case scene, native/tests/dp_air_test.h, at 11 MSa/s,
 * 3,000 sym/s, the 275-in-1,000 window; C/N0 at the antenna. The pool is
 * designed at the use case's floor (4 dB Es/N0, 38.8 dB-Hz), its search
 * +/-1,500 Hz at 400 Hz/s through the window, as the specification sets
 * it. A receiver is put on a phase and Doppler by hand
 * (dp_async_dsss_receiver_seed() on the pool's receiver), and every push
 * completes the pool's group, so its control step runs at the end of
 * each.
 *
 * FORCED. A at PARENT_DBHZ; receiver 1 seeded once receiver 0 tracks A
 * with code lock, on A's sidelobe SIDE chips from A's phase (a Gold
 * sidelobe at the in-phase level, 63 of 1023) and on A's carrier plus k
 * lattice steps, k in -2..2, at the C/N0 the search would report for that
 * peak. Run FORCED_S after the seed with lost_confirm_s 2.0. Each ends
 * released by the ghost rule, freed by the loss release, or held; with
 * its code- and symbol-lock fractions while tracking.
 *
 * SECOND. A at 70 dB-Hz and a DISTINCT emitter B on A's carrier (+/-5 Hz),
 * at a drawn code offset, at B_DBHZ from the floor up: receiver 1 seeded
 * on B's own phase and Doppler once receiver 0 tracks A. Run until both
 * rings have been full for a window. Recorded: whether the rule released
 * B, and whether the rule with k = 0 admitted would have
 * (dp__async_dsss_pool_ghost_of_any_k(), the same tests on the same rings
 * -- async_dsss_pool_private.h), and B's code-lock fraction.
 *
 * ALONE. One emitter, a pool of four searching, ALONE_S; every tenure in
 * which a receiver holds a phase more than a chip from the emitter's is a
 * ghost the pool took on its own: its offset in chips and in lattice
 * steps, and how it ended.
 *
 * Asserted, the same in the sweep and --check: the rule never releases a
 * distinct emitter on its parent's carrier (SECOND, every level), and it
 * releases a k = 2 sidelobe ghost (FORCED, k = 2, every draw). Printed,
 * with Wilson 95% intervals: the forced outcomes by k, the counterfactual
 * release rate by B's level, and the pool's own ghosts. --check: 2.8 GHz,
 * FORCED k = 0 and 2 and SECOND at 38.8 and 50 dB-Hz, one draw each.
 *
 * Usage:  validate_async_dsss_pool_ghosts [--check] [--emit] [--draws N]
 *             [--shard i/n]
 */
#include "async_dsss_receiver_common.h"
#include "doppler/async_dsss_pool/async_dsss_pool_core.h"
#include "doppler/async_dsss_pool/async_dsss_pool_private.h"
#include "dp_air_test.h"
#include "dp_dsss_test.h"
#include "dp_s415_test.h"
#include "dp_stats_test.h"
#include "dp_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GH_SIDE 167u
#define GH_DRAWS 20u
#define GH_FORCED_S 10.0
#define GH_SECOND_S 4.0
#define GH_ALONE_S 4.0
#define GH_PARENT_DBHZ 60.0
#define GH_A2_DBHZ 70.0

static dp_async_dsss_pool_state_t *
gh_pool (const uint8_t *code, double fc, size_t n, double lost_s)
{
  return dp_s415_pool (code, fc, n, lost_s, 1);
}

static dp_air_emitter_t
gh_emitter (const uint8_t *code, double fc, double f0, double cn0,
            double lag_chips, uint32_t seed)
{
  return dp_s415_emitter (code, fc, f0, 0.0, cn0,
                          lag_chips / dp_s415_chip_rate (fc), seed);
}

static long
gh_autocorr (const uint8_t *code, size_t t)
{
  long r = 0;
  for (size_t i = 0; i < 1023; i++)
    r += (code[i] == code[(i + t) % 1023]) ? 1 : -1;
  return r;
}

/* One push: a whole group (the pool's carry is always empty here). */
static int
gh_push (dp_async_dsss_pool_state_t *p, dp_air_t *air, float complex *x)
{
  const size_t want = p->group - p->carry_len;
  DP_REQUIRE (dp_air_next (air, x, want));
  (void)dp_async_dsss_pool_push (p, x, want);
  DP_REQUIRE (p->carry_len == 0);
  return 0;
}

typedef struct
{
  int    outcome; /* 0 held, 1 released as a ghost, 2 freed as lost */
  double code_frac, sym_frac;
  int    any_k; /* the counterfactual took it while the rings were full */
} gh_t;

/* FORCED: receiver 1 on A's sidelobe at k lattice steps. */
static int
gh_forced (const uint8_t *code, double fc, int k, uint32_t seed, gh_t *o)
{
  const double f0 = 700.0;
  DP_REQUIRE_MSG (gh_autocorr (code, GH_SIDE) == 63,
                  "the forced sidelobe is an in-phase Gold sidelobe");
  dp_async_dsss_pool_state_t *p = gh_pool (code, fc, 2, 2.0);
  DP_REQUIRE (p != NULL);
  dp_air_emitter_t e
      = gh_emitter (code, fc, f0, GH_PARENT_DBHZ, 0.0, 100u + seed);
  dp_air_t       air;
  float complex *x = dp_xmalloc (p->group * sizeof *x);
  DP_REQUIRE (dp_air_init (&air, &e, 1, DP_S415_FS, GH_PARENT_DBHZ,
                           7919u + seed, p->group));
  const double level  = 20.0 * log10 (63.0 / 1023.0);
  double       t_seed = -1.0, t = 0.0;
  long         n = 0, code_up = 0, sym_up = 0;
  memset (o, 0, sizeof *o);
  for (;;)
    {
      DP_REQUIRE (gh_push (p, &air, x) == 0);
      t                                     = (double)air.n_out / DP_S415_FS;
      const async_dsss_receiver_status_t s0 = dp_async_dsss_pool_status (p, 0);
      const async_dsss_receiver_status_t s1 = dp_async_dsss_pool_status (p, 1);
      if (t_seed < 0.0)
        {
          if (t >= 0.5 && s0.state == ASYNC_DSSS_RX_TRACKING && s0.code_locked
              && s1.state == ASYNC_DSSS_RX_IDLE)
            {
              DP_REQUIRE (dp_async_dsss_receiver_seed (
                              p->rx[1], fmod (s0.chip_phase + GH_SIDE, 1023.0),
                              f0 + k * p->lattice_hz, GH_PARENT_DBHZ + level)
                          == DP_OK);
              t_seed = t;
            }
          DP_REQUIRE_MSG (t < 3.0, "receiver 0 tracks A with code lock");
          continue;
        }
      if (s1.state == ASYNC_DSSS_RX_TRACKING)
        {
          n++;
          code_up += s1.code_locked != 0;
          sym_up += s1.locked != 0;
          o->any_k |= dp__async_dsss_pool_ghost_of_any_k (p, 0, 1);
        }
      if (s1.state == ASYNC_DSSS_RX_IDLE || t - t_seed > GH_FORCED_S)
        break;
    }
  o->outcome   = p->released_ghost ? 1 : p->released_lost ? 2 : 0;
  o->code_frac = n ? (double)code_up / (double)n : NAN;
  o->sym_frac  = n ? (double)sym_up / (double)n : NAN;
  free (x);
  dp_air_free (&air);
  dp_async_dsss_pool_destroy (p);
  return 0;
}

/* SECOND: a distinct emitter B on A's carrier at `b_dbhz`. */
static int
gh_second (const uint8_t *code, double fc, double b_dbhz, uint32_t seed,
           gh_t *o)
{
  uint32_t     st   = trial_rng (seed);
  const double f0   = 700.0;
  const double df   = -5.0 + 10.0 * dp_uni (&st);
  const double lagc = 100.0 + 800.0 * dp_uni (&st); /* B's code offset */
  dp_async_dsss_pool_state_t *p = gh_pool (code, fc, 2, 0.0);
  DP_REQUIRE (p != NULL);
  dp_air_emitter_t em[2] = {
    gh_emitter (code, fc, f0, GH_A2_DBHZ, 0.0, 100u + seed),
    gh_emitter (code, fc, f0 + df, b_dbhz, lagc, 200u + seed),
  };
  dp_air_t       air;
  float complex *x = dp_xmalloc (p->group * sizeof *x);
  DP_REQUIRE (dp_air_init (&air, em, 2, DP_S415_FS, GH_A2_DBHZ, 7919u + seed,
                           p->group));
  double t_seed = -1.0, t = 0.0, t_full = -1.0;
  long   n = 0, code_up = 0;
  memset (o, 0, sizeof *o);
  for (;;)
    {
      DP_REQUIRE (gh_push (p, &air, x) == 0);
      t                                     = (double)air.n_out / DP_S415_FS;
      const async_dsss_receiver_status_t s0 = dp_async_dsss_pool_status (p, 0);
      const async_dsss_receiver_status_t s1 = dp_async_dsss_pool_status (p, 1);
      if (t_seed < 0.0)
        {
          if (t >= 0.5 && s0.state == ASYNC_DSSS_RX_TRACKING && s0.code_locked
              && s1.state == ASYNC_DSSS_RX_IDLE)
            {
              /* B's own phase at the pool's next sample, as a code NCO
                 reads it (half a sample before the continuous phase). */
              const dp_air_truth_t tb = dp_air_truth (&air, 1, air.n_out);
              DP_REQUIRE (!isnan (tb.code_chips));
              DP_REQUIRE (
                  dp_async_dsss_receiver_seed (
                      p->rx[1],
                      fmod (tb.code_chips - 0.5 * em[1].chip_rate / DP_S415_FS
                                + 1023.0,
                            1023.0),
                      tb.car_hz, b_dbhz)
                  == DP_OK);
              t_seed = t;
            }
          DP_REQUIRE_MSG (t < 3.0, "receiver 0 tracks A with code lock");
          continue;
        }
      if (s1.state == ASYNC_DSSS_RX_TRACKING)
        {
          n++;
          code_up += s1.code_locked != 0;
          o->any_k |= dp__async_dsss_pool_ghost_of_any_k (p, 0, 1);
          if (t_full < 0.0 && p->hist_len[0] == p->ghost_window
              && p->hist_len[1] == p->ghost_window)
            t_full = t;
        }
      if (s1.state == ASYNC_DSSS_RX_IDLE || (t_full >= 0.0 && t - t_full > 1.0)
          || t - t_seed > GH_SECOND_S)
        break;
    }
  o->outcome   = p->released_ghost ? 1 : p->released_lost ? 2 : 0;
  o->code_frac = n ? (double)code_up / (double)n : NAN;
  free (x);
  dp_air_free (&air);
  dp_async_dsss_pool_destroy (p);
  return 0;
}

/* ALONE: one emitter at the parent's level, a pool of four searching. A
   tenure is a receiver's run in tracking; one whose code phase is ever
   more than a chip from the emitter's is a ghost the pool took on its own.
   Counts them, and how each ended: by the release counters' change at the
   step it left tracking. */
typedef struct
{
  size_t tenures, ghosts, by_ghost, by_lost, by_dup, open;
} gh_alone_t;

static int
gh_alone (const uint8_t *code, double fc, uint32_t seed, gh_alone_t *o,
          int emit)
{
  uint32_t                    st = trial_rng (seed);
  dp_async_dsss_pool_state_t *p  = gh_pool (code, fc, 4, 2.0);
  DP_REQUIRE (p != NULL);
  dp_air_emitter_t e = gh_emitter (code, fc, -1200.0 + 2400.0 * dp_uni (&st),
                                   GH_PARENT_DBHZ, 0.0, 300u + seed);
  dp_air_t         air;
  float complex   *x = dp_xmalloc (p->group * sizeof *x);
  DP_REQUIRE (dp_air_init (&air, &e, 1, DP_S415_FS, GH_PARENT_DBHZ,
                           6007u + seed, p->group));
  int      in[4] = { 0 }, off[4] = { 0 };
  double   worst[4] = { 0 };
  uint64_t g0 = 0, l0 = 0, d0 = 0;
  memset (o, 0, sizeof *o);
  while ((double)air.n_out / DP_S415_FS < GH_ALONE_S)
    {
      DP_REQUIRE (gh_push (p, &air, x) == 0);
      const dp_air_truth_t t = dp_air_truth (&air, 0, air.n_out);
      for (size_t r = 0; r < 4; r++)
        {
          const async_dsss_receiver_status_t s
              = dp_async_dsss_pool_status (p, r);
          const int tracking = s.state == ASYNC_DSSS_RX_TRACKING;
          if (tracking && !isnan (t.code_chips))
            {
              double d
                  = fmod (s.chip_phase - t.code_chips + 1023.0 * 2.5, 1023.0)
                    - 511.5;
              d = fabs (d);
              if (!in[r])
                {
                  in[r]    = 1;
                  off[r]   = 0;
                  worst[r] = 0.0;
                  o->tenures++;
                }
              if (d > 1.0)
                off[r] = 1;
              if (d > worst[r])
                worst[r] = d;
            }
          else if (in[r] && !tracking)
            {
              in[r] = 0;
              if (off[r])
                {
                  o->ghosts++;
                  o->by_ghost += p->released_ghost > g0;
                  o->by_lost += p->released_lost > l0;
                  o->by_dup += p->released_duplicate > d0;
                  if (emit)
                    printf ("%.1f,%u,%zu,%.1f,%llu\n", fc / 1e9, seed, r,
                            worst[r], (unsigned long long)air.n_out);
                }
            }
        }
      g0 = p->released_ghost;
      l0 = p->released_lost;
      d0 = p->released_duplicate;
    }
  for (size_t r = 0; r < 4; r++)
    if (in[r] && off[r])
      {
        o->ghosts++;
        o->open++;
      }
  free (x);
  dp_air_free (&air);
  dp_async_dsss_pool_destroy (p);
  return 0;
}

static void
gh_rate (const char *what, size_t k, size_t n)
{
  double lo, hi;
  dp_wilson ((double)k, (double)n, &lo, &hi);
  printf ("      %-30s %3zu of %zu [%.2f, %.2f]\n", what, k, n, lo, hi);
}

int
main (int argc, char **argv)
{
  int      check = 0, emit = 0;
  unsigned draws = GH_DRAWS, shard = 0, shards = 1;
  for (int i = 1; i < argc; i++)
    {
      check |= strcmp (argv[i], "--check") == 0;
      emit |= strcmp (argv[i], "--emit") == 0;
      if (strcmp (argv[i], "--draws") == 0 && i + 1 < argc)
        draws = (unsigned)strtoul (argv[++i], NULL, 10);
      else if (strcmp (argv[i], "--shard") == 0 && i + 1 < argc)
        DP_REQUIRE (sscanf (argv[++i], "%u/%u", &shard, &shards) == 2
                    && shards > 0 && shard < shards);
    }
  if (check)
    draws = 1;
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  static const double fcs[2]    = { DP_S415_FC_LO, DP_S415_FC_HI };
  static const double b_lvls[5] = { 38.8, 42.0, 44.0, 46.0, 50.0 };

  if (emit)
    printf ("# forced\nfc_ghz,k,seed,outcome,code_frac,sym_frac,any_k\n");
  else
    printf ("ghosts: 415.1-B-1, 11 MSa/s, a pool designed at 38.8 dB-Hz; "
            "FORCED on a %.0f dB-Hz parent's %u-chip sidelobe, %.0f s\n",
            GH_PARENT_DBHZ, GH_SIDE, GH_FORCED_S);
  for (int f = check ? 1 : 0; f < 2; f++)
    for (int k = -2; k <= 2; k++)
      {
        if (check && k != 0 && k != 2)
          continue;
        size_t n = 0, rel = 0, lost = 0, held = 0;
        for (uint32_t s = 1; s <= draws; s++)
          {
            if ((s - 1) % shards != shard)
              continue;
            gh_t o;
            DP_REQUIRE (gh_forced (code, fcs[f], k, s, &o) == 0);
            n++;
            rel += o.outcome == 1;
            lost += o.outcome == 2;
            held += o.outcome == 0;
            if (k == 2)
              DP_CHECK_MSG (o.outcome == 1, "a k = 2 sidelobe ghost is "
                                            "released by the ghost rule");
            if (k == 0)
              DP_CHECK_MSG (o.outcome != 1, "the ghost rule never takes a "
                                            "k = 0 ghost");
            if (emit)
              printf ("%.1f,%d,%u,%d,%.3f,%.3f,%d\n", fcs[f] / 1e9, k, s,
                      o.outcome, o.code_frac, o.sym_frac, o.any_k);
          }
        if (!emit && n)
          {
            printf ("    %.1f GHz, k = %+d: %zu draws\n", fcs[f] / 1e9, k, n);
            gh_rate ("released by the ghost rule", rel, n);
            gh_rate ("freed by the loss release", lost, n);
            gh_rate ("held", held, n);
          }
      }

  if (emit)
    printf ("# second\nfc_ghz,b_dbhz,seed,outcome,code_frac,any_k\n");
  else
    printf ("  SECOND: a distinct emitter on a %.0f dB-Hz one's carrier\n",
            GH_A2_DBHZ);
  for (int f = check ? 1 : 0; f < 2; f++)
    for (int l = 0; l < 5; l++)
      {
        if (check && l != 0 && l != 4)
          continue;
        size_t n = 0, rel = 0, would = 0;
        for (uint32_t s = 1; s <= draws; s++)
          {
            if ((s - 1) % shards != shard)
              continue;
            gh_t o;
            DP_REQUIRE (gh_second (code, fcs[f], b_lvls[l], s, &o) == 0);
            n++;
            rel += o.outcome == 1;
            would += (size_t)o.any_k;
            DP_CHECK_MSG (o.outcome != 1, "the ghost rule never releases a "
                                          "distinct emitter on its "
                                          "parent's carrier");
            if (emit)
              printf ("%.1f,%.1f,%u,%d,%.3f,%d\n", fcs[f] / 1e9, b_lvls[l], s,
                      o.outcome, o.code_frac, o.any_k);
          }
        if (!emit && n)
          {
            printf ("    %.1f GHz, B at %.1f dB-Hz: %zu draws\n", fcs[f] / 1e9,
                    b_lvls[l], n);
            gh_rate ("released by the rule", rel, n);
            gh_rate ("released if k = 0 were admitted", would, n);
          }
      }
  if (emit)
    printf ("# alone\nfc_ghz,seed,receiver,worst_chips,ended_at\n");
  else
    printf ("  ALONE: one %.0f dB-Hz emitter, a pool of four, %.0f s\n",
            GH_PARENT_DBHZ, GH_ALONE_S);
  for (int f = check ? 1 : 0; f < 2; f++)
    {
      gh_alone_t a = { 0 };
      size_t     n = 0;
      for (uint32_t s = 1; s <= draws; s++)
        {
          if ((s - 1) % shards != shard)
            continue;
          gh_alone_t o;
          DP_REQUIRE (gh_alone (code, fcs[f], s, &o, emit) == 0);
          n++;
          a.tenures += o.tenures;
          a.ghosts += o.ghosts;
          a.by_ghost += o.by_ghost;
          a.by_lost += o.by_lost;
          a.by_dup += o.by_dup;
          a.open += o.open;
        }
      if (!emit && n)
        printf ("    %.1f GHz: %zu draws, %zu tenures, %zu off the emitter's "
                "phase: ended by the ghost rule %zu, the loss release %zu, "
                "the duplicate rule %zu, still held %zu\n",
                fcs[f] / 1e9, n, a.tenures, a.ghosts, a.by_ghost, a.by_lost,
                a.by_dup, a.open);
    }
  if (emit)
    DP_TEST_EMIT_END ("validate_async_dsss_pool_ghosts");
  DP_TEST_END ("validate_async_dsss_pool_ghosts");
}
