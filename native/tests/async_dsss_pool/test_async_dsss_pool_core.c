/**
 * @file test_async_dsss_pool_core.c
 * @brief AsyncDsssPool: the controller's rules, each pinned against its
 *        header claim (async_dsss_pool_core.h), and each test's sabotage
 *        named in its comment.
 */
#include "doppler/RateConverter/RateConverter_core.h"
#include "doppler/async_dsss_pool/async_dsss_pool_core.h"
#include "doppler/async_dsss_pool/async_dsss_pool_private.h"
#include "doppler/async_dsss_receiver/async_dsss_receiver_private.h"
#include "doppler/awgn/awgn_core.h"
#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include "dp_air_test.h"
#include "dp_chunk_inv.h"
#include "dp_dsss_test.h"
#include "dp_rng_test.h"
#include "dp_s415_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The design point of the receiver tests' ranked search: the specification's
   Gold-1023 (docs/design/async-dsss-receiver.md) at 3.069 Mcps, 2 samples a
   chip, 2700 symbols/s. */
#define POOL_CR 3.069e6
#define POOL_RS 2700.0
#define POOL_SPC 2
#define POOL_FS (POOL_CR * POOL_SPC)

/* The release on, short enough to fire inside the two-emitter stream: B
   leaves at CI_B_OFF, and the receiver holding it is released CI_CONFIRM_S
   later. A receiver reads its release clock once per call, so this is
   where a blocking-dependent feed shows (doppler#2548 review). */
#define CI_CONFIRM_S 0.02
#define CI_B_OFF (300000u)

/* One push the controller acts on: a group of code periods at 2 samples a
   chip (ASYNC_DSSS_POOL_PERIODS_PER_JOIN), the receivers' grid. */
#define POOL_GROUP ((size_t)2046 * ASYNC_DSSS_POOL_PERIODS_PER_JOIN)

static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

/* A pool on the Gold-1023 design point; the receiver tests' _slow_rx()
   numbers, n receivers, the release rule off unless asked. */
static dp_async_dsss_pool_state_t *
_pool_span (const uint8_t *code, size_t n, double lost_confirm_s, int threads,
            double span_hz)
{
  return dp_async_dsss_pool_create (
      code, 1023, n, POOL_CR, POOL_RS, POOL_CR * (double)POOL_SPC, 2, 47.0,
      1e-3, 0.9, span_hz, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0,
      lost_confirm_s, false, 0.0, 0.0, threads, true);
}

static dp_async_dsss_pool_state_t *
_pool (const uint8_t *code, size_t n, double lost_confirm_s, int threads)
{
  return _pool_span (code, n, lost_confirm_s, threads, 500.0);
}

/* Two emitters on one Gold code, ~400 chips apart: A at 200 Hz carries the
   noise (50 dB-Hz), B at -300 Hz sits 3 dB below it. The receiver tests'
   _two_emitters(), which their rank tests feed. Caller frees. */
static float _Complex *_two_emitters_off (const uint8_t *code, size_t b_off,
                                          size_t *n_out);

static float _Complex *
_two_emitters (const uint8_t *code, size_t *n_out)
{
  return _two_emitters_off (code, (size_t)-1, n_out);
}

/* As _two_emitters(), with B switched off from sample `b_off` on: the
   stream a release runs on. */
static float _Complex *
_two_emitters_off (const uint8_t *code, size_t b_off, size_t *n_out)
{
  const double    tsym = POOL_FS / POOL_RS;
  float _Complex *a, *b;
  size_t          na, nb;
  double         *da, *db;
  dp_dsss_capture (code, 1023, POOL_SPC, POOL_FS, tsym, 200.0, 50.0, 300, 1000,
                   11, &a, &na, &da);
  dp_dsss_capture (code, 1023, POOL_SPC, POOL_FS, tsym, -300.0, 120.0, 300,
                   200, 12, &b, &nb, &db);
  const size_t n = na < nb ? na : nb;
  for (size_t i = 0; i < n; i++)
    a[i] += i < b_off ? (float)M_SQRT1_2 * b[i] : 0.0f;
  free (b);
  free (da);
  free (db);
  *n_out = n;
  return a;
}

static double
_chip_dist (double a, double b, double l)
{
  double d = fabs (a - b);
  return d > l / 2 ? l - d : d;
}

/* create's domain: 1 to ASYNC_DSSS_POOL_MAX_RECEIVERS receivers, threads
   >= 0, and whatever a receiver refuses. Each receiver's engine runs on
   one thread. Sabotage: drop the receiver-count check -> 0 builds a pool
   of nothing; drop set_threads(1) -> the engine reads the core count. */
static int
_test_create_domain (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  DP_CHECK (_pool (code, 0, 0.0, 1) == NULL);
  DP_CHECK (_pool (code, ASYNC_DSSS_POOL_MAX_RECEIVERS + 1, 0.0, 1) == NULL);
  DP_CHECK (_pool (code, 2, 0.0, -1) == NULL);
  DP_CHECK (_pool (code, 2, -1.0, 1) == NULL); /* the receiver's refusal */
  DP_CHECK (dp_async_dsss_pool_create (
                NULL, 1023, 2, POOL_CR, POOL_RS, POOL_CR * (double)POOL_SPC, 2,
                47.0, 1e-3, 0.9, 500.0, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false,
                100000, 0.0, 0.0, false, 0.0, 0.0, 1, true)
            == NULL);
  dp_async_dsss_pool_state_t *p
      = _pool (code, ASYNC_DSSS_POOL_MAX_RECEIVERS, 0.0, 0);
  DP_REQUIRE (p != NULL);
  DP_CHECK (p->n_receivers == ASYNC_DSSS_POOL_MAX_RECEIVERS);
  dp_async_dsss_pool_destroy (p);
  /* A search over several Doppler tiles (+/-5 kHz, tiles 3 kHz apart),
     inside a pool. */
  p = dp_async_dsss_pool_create (
      code, 1023, 2, POOL_CR, POOL_RS, POOL_CR * (double)POOL_SPC, 2, 60.0,
      1e-3, 0.9, 5000.0, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0,
      false, 0.0, 0.0, 0, true);
  DP_REQUIRE (p != NULL && p->rx[0]->acq->search->window_bins > 1);
  for (size_t i = 0; i < p->n_receivers; i++)
    DP_CHECK (dp_async_dsss_receiver_get_threads (p->rx[i]) == 1);
  /* The search runs on the pool's own thread count, capped at one worker a
     tile (#2773, async-dsss-receiver.md §5): two asked for runs on two
     (asked explicitly, so this holds on a one-core box), one on one, and
     the online cores on no more than its tiles. Sabotage: the search
     serial again -> 1 at two, red. */
  const int tiles = (int)p->search->acq->search->window_bins;
  DP_REQUIRE (tiles >= 2);
  /* The search runs its per-epoch stages inline (#2775): the fan costs a
     pool 1.9-3.5x at D = 1 on WSL2. Sabotage: drop the pool's clear ->
     red. */
  DP_CHECK_MSG (p->search->acq->search->epoch_fan == 0,
                "the pool's search keeps its per-epoch stages inline");
  DP_REQUIRE (dp_async_dsss_pool_set_threads (p, 2) == 0);
  DP_CHECK (dp_async_dsss_receiver_get_threads (p->search) == 2);
  DP_REQUIRE (dp_async_dsss_pool_set_threads (p, 1) == 0);
  DP_CHECK (dp_async_dsss_receiver_get_threads (p->search) == 1);
  DP_REQUIRE (dp_async_dsss_pool_set_threads (p, 0) == 0);
  DP_CHECK (dp_async_dsss_receiver_get_threads (p->search) >= 1
            && dp_async_dsss_receiver_get_threads (p->search) <= tiles);
  /* set_state applies the rule on the READER, whatever count the writer's
     search resolved: a writer on 2 whose search was left on 3 (as one on
     another machine resolves its own cores) restores, on a pool asking for
     2, to 2. Sabotage: rebuild only when the pool's count moved -> the
     child blob's 3 survives, red. */
  if (tiles >= 3)
    {
      DP_REQUIRE (dp_async_dsss_pool_set_threads (p, 2) == 0);
      DP_REQUIRE (dp_async_dsss_receiver_set_threads (p->search, 3) == 0);
      DP_REQUIRE (dp_async_dsss_receiver_get_threads (p->search) == 3);
      const size_t   nb   = dp_async_dsss_pool_state_bytes (p);
      unsigned char *blob = malloc (nb);
      DP_REQUIRE (blob != NULL);
      dp_async_dsss_pool_get_state (p, blob);
      dp_async_dsss_pool_state_t *q = dp_async_dsss_pool_create (
          code, 1023, 2, POOL_CR, POOL_RS, POOL_CR * (double)POOL_SPC, 2, 60.0,
          1e-3, 0.9, 5000.0, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0,
          0.0, false, 0.0, 0.0, 2, true);
      DP_REQUIRE (q != NULL);
      DP_REQUIRE (dp_async_dsss_pool_set_state (q, blob) == DP_OK);
      DP_CHECK (dp_async_dsss_receiver_get_threads (q->search) == 2);
      DP_CHECK (q->search->acq->search->epoch_fan == 0);
      dp_async_dsss_pool_destroy (q);
      free (blob);
    }
  DP_CHECK (dp_async_dsss_pool_get_threads (p) >= 1);
  DP_CHECK (dp_async_dsss_pool_get_threads (p)
            <= (int)ASYNC_DSSS_POOL_MAX_RECEIVERS);
  DP_CHECK (dp_async_dsss_pool_get_coherent_bins (p)
            == dp_async_dsss_receiver_get_coherent_bins (p->rx[0]));
  dp_async_dsss_pool_destroy (p);
  dp_async_dsss_pool_destroy (NULL);
  return 0;
}

/* The point of the pool: three receivers on two emitters hold the two,
   one each, and the third stays idle; the one search is excluded from both
   holders; nothing is a duplicate. Sabotage: skip the search's exclusions
   -> its picks name A again and the third piles onto it; one pick a dwell
   instead of one per idle receiver -> still two holders, later (not this
   test's to catch: _test_search_runs_once). */
static int
_test_two_emitters_two_holders (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t                      n;
  float _Complex             *x = _two_emitters (code, &n);
  dp_async_dsss_pool_state_t *p = _pool (code, 3, 0.0, 1);
  DP_REQUIRE (p != NULL);
  /* Fed by hand, to count each receiver's symbols over the run. */
  size_t emitted[3] = { 0 };
  float _Complex buf[4096];
  const size_t blk = 1023 * POOL_SPC;
  for (size_t pos = 0; pos < n; pos += blk)
    {
      (void)dp_async_dsss_pool_push (p, x + pos,
                                     n - pos < blk ? n - pos : blk);
      DP_CHECK (dp_async_dsss_pool_symbols_max_out (p) <= 4096);
      for (size_t i = 0; i < 3; i++)
        emitted[i] += dp_async_dsss_pool_symbols (p, i, buf, 4096);
    }
  async_dsss_receiver_status_t st[3];
  size_t                       held = 0;
  for (size_t i = 0; i < 3; i++)
    {
      st[i] = dp_async_dsss_pool_status (p, i);
      held += st[i].state == ASYNC_DSSS_RX_REFINING
              || st[i].state == ASYNC_DSSS_RX_TRACKING;
    }
  DP_CHECK_MSG (held == 2, "two emitters, two holders");
  DP_CHECK (st[2].state == ASYNC_DSSS_RX_IDLE);
  DP_CHECK (_chip_dist (st[0].chip_phase, st[1].chip_phase, 1023.0) > 100.0);
  DP_CHECK (p->released_duplicate == 0);
  /* The one search is excluded from both holders; no receiver searches. */
  DP_CHECK (dp_async_dsss_receiver_get_n_exclusions (p->search) == 2);
  for (size_t i = 0; i < 3; i++)
    DP_CHECK (dp_correlator_position (p->rx[i]->acq) == 0);
  /* The holders track and emit; the searcher never emitted. */
  DP_CHECK (st[0].state == ASYNC_DSSS_RX_TRACKING
            && st[1].state == ASYNC_DSSS_RX_TRACKING);
  DP_CHECK (emitted[0] > 0 && emitted[1] > 0);
  DP_CHECK (emitted[2] == 0);
  dp_async_dsss_pool_destroy (p);
  free (x);
  return 0;
}

/* The controller's seeding and its levers, read at every control step
   (#3267, P19, P20). One push of a whole group is one control step. After
   each:
   - the receivers just seeded (idle before, holding now) are the lowest
     indices of those that were idle, and their seed C/N0 falls with the
     index: the picks go strongest first, into the idle receivers in index
     order;
   - the search's pick count is the receivers left idle (one when none
     is: the search is not fed then).
   The scene seeds both emitters in one step, so the order is exercised:
   A is 3 dB over B. A receiver that refines through a whole group has no
   symbols for that push (P39). Sabotage: seed the idle receivers from the
   highest
   index down -> red on the index rule; take the picks weakest first -> red
   on the C/N0 order; set the pick count to the holders -> red; let a
   refining receiver emit -> red on the refining row. */
static int
_test_seeds_go_strongest_first_in_index_order (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t                      n;
  float _Complex             *x = _two_emitters (code, &n);
  dp_async_dsss_pool_state_t *p = _pool (code, 3, 0.0, 1);
  DP_REQUIRE (p && x);
  async_dsss_receiver_status_t was[3], now[3];
  float _Complex buf[1];
  for (size_t i = 0; i < 3; i++)
    was[i] = dp_async_dsss_pool_status (p, i);
  size_t most_in_a_step = 0;
  int    lowest = 1, by_cn0 = 1, picks = 1, quiet = 1;
  size_t refined_through = 0;
  for (size_t pos = 0; pos + POOL_GROUP <= n; pos += POOL_GROUP)
    {
      (void)dp_async_dsss_pool_push (p, x + pos, POOL_GROUP);
      size_t seeded = 0, idle = 0, idle_before = 0;
      double last_cn0 = INFINITY;
      for (size_t i = 0; i < 3; i++)
        {
          now[i]              = dp_async_dsss_pool_status (p, i);
          const int was_idle  = was[i].state == ASYNC_DSSS_RX_IDLE;
          const int now_holds = now[i].state == ASYNC_DSSS_RX_REFINING
                                || now[i].state == ASYNC_DSSS_RX_TRACKING;
          idle += now[i].state == ASYNC_DSSS_RX_IDLE;
          if (was_idle && now_holds)
            {
              /* Every receiver idle before this one was seeded too. */
              lowest &= seeded == idle_before;
              by_cn0 &= now[i].cn0_dbhz_est <= last_cn0;
              last_cn0 = now[i].cn0_dbhz_est;
              seeded++;
            }
          idle_before += was_idle;
        }
      picks &= p->search->pick_want == (idle ? idle : 1);
      /* A receiver refining at both ends of the group refined through it:
         its symbols for this push are empty (#3267, P39). */
      for (size_t i = 0; i < 3; i++)
        if (was[i].state == ASYNC_DSSS_RX_REFINING
            && now[i].state == ASYNC_DSSS_RX_REFINING)
          {
            refined_through++;
            quiet &= dp_async_dsss_pool_symbols (p, i, buf, 1) == 0;
          }
      most_in_a_step = seeded > most_in_a_step ? seeded : most_in_a_step;
      memcpy (was, now, sizeof was);
    }
  DP_REQUIRE_MSG (most_in_a_step >= 2,
                  "the scene seeds two receivers in one step");
  DP_CHECK_MSG (lowest, "seeds go to the lowest-index idle receivers");
  DP_CHECK_MSG (by_cn0, "the stronger pick goes to the lower index");
  DP_CHECK_MSG (picks, "the search's pick count is the receivers idle");
  DP_REQUIRE_MSG (refined_through > 0,
                  "a receiver refined through a whole group");
  DP_CHECK_MSG (quiet, "a refining receiver's symbols are empty");
  dp_async_dsss_pool_destroy (p);
  free (x);
  return 0;
}

/* A period that is not a whole number of samples (#3063). The two emitters
   are taken to `ratio` times the design rate, so a code period is
   2046 * ratio samples. The live chain runs whole periods of tsamps samples
   and carries the remainder, so a holder's Dll stands up to a period behind
   the stream; its reported chip_phase must still be the phase at the
   stream's next sample, the one the search's picks are carried to, or the
   exclusions miss and the held emitters come back as seeds. So: a second
   receiver fed the same pushes as a search-only receiver with no
   exclusions, whose every pick is a held emitter, must land within the
   one-chip key of a holder after the push that found it; and over the run
   no duplicate is released and both emitters track. Sabotage: report the
   Dll's own phase in tracking (the defect) -> every pick sits ~50 chips
   (at 2.2 a chip) from its holder, and the pool releases duplicates. */
static int
_test_fractional_period_phase (double ratio)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t                    n;
  float _Complex           *x  = _two_emitters (code, &n);
  const size_t              ny = (size_t)ceil ((double)n * ratio) + 64;
  float _Complex           *y  = malloc (ny * sizeof *y);
  dp_RateConverter_state_t *rc = dp_RateConverter_create (ratio, 0);
  DP_REQUIRE (x && y && rc);
  const size_t                m  = dp_RateConverter_execute (rc, x, n, y, ny);
  const double                fs = POOL_FS * ratio;
  dp_async_dsss_pool_state_t *p  = dp_async_dsss_pool_create (
      code, 1023, 3, POOL_CR, POOL_RS, fs, 2, 47.0, 1e-3, 0.9, 500.0, 4, 8, 0,
      0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0, 1, true);
  dp_async_dsss_receiver_state_t *q = dp_async_dsss_receiver_create (
      code, 1023, POOL_CR, POOL_RS, fs, 2, 47.0, 1e-3, 0.9, 500.0, 4, 8, 0,
      0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0, true);
  DP_REQUIRE (p && q);
  (void)dp_async_dsss_receiver_set_search_only (q, 2);
  const size_t blk   = (size_t)ceil (1023.0 * POOL_SPC * ratio);
  size_t       picks = 0, matched = 0, q_fed = 0, handovers = 0;
  int          prev_state[3] = { -1, -1, -1 };
  double       prev_phase[3] = { 0 };
  size_t       prev_fed      = 0;
  double       step_worst    = 0.0;
  double       worst         = 0.0;
  for (size_t pos = 0; pos < m; pos += blk)
    {
      const size_t len = m - pos < blk ? m - pos : blk;
      (void)dp_async_dsss_pool_push (p, y + pos, len);
      /* The searcher gets what the pool's receivers got: the pool holds a
         partial period back (carry_len), and a holder's phase is at the
         sample after the ones it was fed. */
      const size_t fed = pos + len - p->carry_len;
      float _Complex none[1];
      (void)dp_async_dsss_receiver_steps (q, y + q_fed, fed - q_fed, none, 0);
      q_fed = fed;
      double ph[ASYNC_DSSS_RX_MAX_PEAKS], dop[ASYNC_DSSS_RX_MAX_PEAKS],
          cn0[ASYNC_DSSS_RX_MAX_PEAKS];
      const size_t k = dp_async_dsss_receiver_take_picks (
          q, ph, dop, cn0, ASYNC_DSSS_RX_MAX_PEAKS);
      async_dsss_receiver_status_t st[3];
      size_t                       tracking = 0;
      for (size_t i = 0; i < 3; i++)
        {
          st[i] = dp_async_dsss_pool_status (p, i);
          tracking += st[i].state == ASYNC_DSSS_RX_TRACKING;
        }
      /* No step at the hand-over: a receiver that was refining after the
         last push and tracks after this one reports the phase the refine
         did, carried over the samples fed since (no carrier set, so no
         dilation). */
      for (size_t i = 0; i < 3; i++)
        {
          if (prev_state[i] == ASYNC_DSSS_RX_REFINING
              && st[i].state == ASYNC_DSSS_RX_TRACKING)
            {
              const double want
                  = fmod (prev_phase[i]
                              + (double)(fed - prev_fed) / (POOL_SPC * ratio),
                          1023.0);
              step_worst = fmax (step_worst,
                                 _chip_dist (want, st[i].chip_phase, 1023.0));
              handovers++;
            }
          prev_state[i] = st[i].state;
          prev_phase[i] = st[i].chip_phase;
        }
      prev_fed = fed;
      if (tracking < 2)
        continue;
      for (size_t j = 0; j < k; j++)
        {
          double d = 1023.0;
          for (size_t i = 0; i < 3; i++)
            if (st[i].state == ASYNC_DSSS_RX_TRACKING)
              d = fmin (d, _chip_dist (ph[j], st[i].chip_phase, 1023.0));
          worst = fmax (worst, d);
          picks++;
          matched += d <= 1.0;
        }
    }
  DP_CHECK_MSG (handovers == 2, "both receivers handed over");
  DP_CHECK_MSG (step_worst < 1.0, "no phase step at the hand-over");
  DP_CHECK_MSG (picks > 0, "the searcher picked a held emitter");
  DP_CHECK_MSG (matched == picks, "every pick within a chip of its holder");
  if (matched != picks || step_worst >= 1.0)
    printf ("  ratio %.3f: %zu of %zu picks within a chip, worst %.2f; "
            "hand-over step %.2f chips\n",
            ratio, matched, picks, worst, step_worst);
  size_t held = 0;
  for (size_t i = 0; i < 3; i++)
    held += dp_async_dsss_pool_status (p, i).state == ASYNC_DSSS_RX_TRACKING;
  DP_CHECK_MSG (held == 2, "both emitters track");
  DP_CHECK (p->released_duplicate == 0);
  dp_async_dsss_receiver_destroy (q);
  dp_async_dsss_pool_destroy (p);
  dp_RateConverter_destroy (rc);
  free (y);
  free (x);
  return 0;
}

/* Today's peak zone (one native Doppler row by the template's mainlobe) on
   every searcher a pool owns: its receivers' and its controller's search. */
static int
_zone_today (dp_async_dsss_pool_state_t *p)
{
  for (size_t i = 0; i < p->n_receivers; i++)
    DP_REQUIRE (dp_correlator_set_peak_zone (p->rx[i]->acq, 0, 0.0, NULL)
                == DP_OK);
  DP_REQUIRE (dp_correlator_set_peak_zone (p->search->acq, 0, 0.0, NULL)
              == DP_OK);
  return 0;
}

/* The pool's outcome with the receivers' whole-Doppler peak zone is
   today's (#3060): the zone removes only peaks the pick drops (rule 1:
   within a chip of a stronger one's code phase, at any Doppler), so with
   two emitters, both listed either way, nothing the pick keeps moves --
   even on a deep block, where today's list fills on the first dwell with
   the emitters' own lags (their slow-time sidelobes). Two pools on one
   stream, one with today's zone, compared push by push over the whole run:
   every receiver's state, code phase and carrier, its symbols, and the
   released counts; and both emitters held. Rates: the design's 2 samples a
   chip and a fractional one (2.2); spans: one roll and several; depth: one
   epoch, and a deep block inside a code-only window. Where the list fills
   AND a weaker emitter lies past it, the zone is meant to change the
   outcome: _test_zone_lists_the_weaker. Sabotage: make the receiver's
   zone 450 chips wide (it masks B, 400 chips from A) -> runs diverge. */
static int
_zone_run (const uint8_t *code, double ratio, double span, int deep,
           int *compared)
{
  const double    tsym = POOL_FS / POOL_RS;
  float _Complex *a, *b;
  size_t          na, nb;
  double         *da, *db;
  const size_t    w = deep ? 450u : 0u, f = deep ? 500u : 0u;
  /* Both depths at 50 dB-Hz. The deep runs need it: there a deep coherent
     dwell lifts the emitters' own lags (their slow-time sidelobes) over the
     gate on enough rows to fill the list, which the run asserts as its
     premise. Measured (swiftgo, 0373a01d5): at 47 dB-Hz the list fills on
     none of the four deep runs, at 50 on all four, and the zone identity
     holds 8 of 8 at both. */
  const double cn0 = 50.0;
  dp_dsss_windowed_capture (code, 1023, POOL_SPC, POOL_FS, tsym, 200.0, cn0,
                            deep ? 600 : 300, 1000, 11, w, f, &a, &na, &da);
  dp_dsss_windowed_capture (code, 1023, POOL_SPC, POOL_FS, tsym, -300.0, 120.0,
                            deep ? 600 : 300, 200, 12, w, f, &b, &nb, &db);
  size_t n = na < nb ? na : nb;
  for (size_t i = 0; i < n; i++)
    a[i] += (float)M_SQRT1_2 * b[i];
  free (b);
  free (da);
  free (db);
  float _Complex *x = a;
  if (ratio != 1.0)
    {
      const size_t              cap = (size_t)ceil ((double)n * ratio) + 64;
      dp_RateConverter_state_t *rc  = dp_RateConverter_create (ratio, 0);
      x                             = malloc (cap * sizeof *x);
      DP_REQUIRE (rc && x);
      n = dp_RateConverter_execute (rc, a, n, x, cap);
      dp_RateConverter_destroy (rc);
      free (a);
    }
  const double fs = POOL_FS * ratio;
  const double co = deep ? 450.0 / POOL_RS : 0.0, dr = deep ? 500.0 : 0.0;
  dp_async_dsss_pool_state_t *pn = dp_async_dsss_pool_create (
      code, 1023, 3, POOL_CR, POOL_RS, fs, 2, 47.0, 1e-3, 0.9, span, 4, 8, 0,
      0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, co, dr, 1, true);
  dp_async_dsss_pool_state_t *po = dp_async_dsss_pool_create (
      code, 1023, 3, POOL_CR, POOL_RS, fs, 2, 47.0, 1e-3, 0.9, span, 4, 8, 0,
      0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, co, dr, 1, true);
  DP_REQUIRE (pn && po);
  DP_REQUIRE (_zone_today (po) == 0);
  const size_t blk  = (size_t)ceil (1023.0 * POOL_SPC * ratio);
  int          same = 1, filled = 0;
  size_t       pushes = 0;
  float _Complex sn[4096], so[4096];
  for (size_t pos = 0; pos < n && same; pos += blk)
    {
      const size_t take = n - pos < blk ? n - pos : blk;
      const size_t hn   = dp_async_dsss_pool_push (pn, x + pos, take);
      const size_t ho   = dp_async_dsss_pool_push (po, x + pos, take);
      pushes++;
      filled |= po->search->acq->n_peaks == po->search->acq->max_peaks;
      same &= hn == ho;
      for (size_t i = 0; i < 3; i++)
        {
          const async_dsss_receiver_status_t s1
              = dp_async_dsss_pool_status (pn, i);
          const async_dsss_receiver_status_t s2
              = dp_async_dsss_pool_status (po, i);
          same &= s1.state == s2.state && s1.chip_phase == s2.chip_phase
                  && s1.doppler_hz == s2.doppler_hz;
          const size_t k1 = dp_async_dsss_pool_symbols (pn, i, sn, 4096);
          const size_t k2 = dp_async_dsss_pool_symbols (po, i, so, 4096);
          same &= k1 == k2 && memcmp (sn, so, k1 * sizeof *sn) == 0;
        }
      same &= pn->released_duplicate == po->released_duplicate
              && pn->released_ghost == po->released_ghost
              && pn->released_lost == po->released_lost;
    }
  /* A run counts when both emitters were held by its end. */
  const int held
      = dp_async_dsss_pool_status (po, 0).state == ASYNC_DSSS_RX_TRACKING
        && dp_async_dsss_pool_status (po, 1).state == ASYNC_DSSS_RX_TRACKING;
  if (held)
    (*compared)++;
  DP_CHECK_MSG (same, "the whole-Doppler zone changed an outcome");
  /* On the deep block the identity holds where today's list fills. */
  if (deep)
    DP_CHECK_MSG (filled, "a deep run filled today's list");
  if (!same || !held)
    printf ("  zone identity: ratio %.2f span %.0f deep %d: %s after %zu "
            "pushes, %s\n",
            ratio, span, deep, same ? "identical" : "DIVERGED", pushes,
            held ? "both held" : "not both held when compared");
  dp_async_dsss_pool_destroy (pn);
  dp_async_dsss_pool_destroy (po);
  free (x);
  return 0;
}

static int
_test_zone_keeps_the_outcome (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const double ratios[2] = { 1.0, 1.1 };
  const double spans[2]  = { 500.0, 5000.0 };
  int          compared = 0, runs = 0;
  for (int r = 0; r < 2; r++)
    for (int sp = 0; sp < 2; sp++)
      for (int deep = 0; deep < 2; deep++)
        {
          runs++;
          (void)_zone_run (code, ratios[r], spans[sp], deep, &compared);
        }
  DP_CHECK_MSG (compared == runs, "every run held both emitters");
  printf ("  zone identity: %d of %d runs compared\n", compared, runs);
  return 0;
}

/* Whether a search-only receiver's last decided dwell lists a peak within
   `half` columns of lag `col` (circularly over `nx`). */
static int
_lists_lag (const dp_async_dsss_receiver_state_t *r, size_t col, size_t half)
{
  const size_t nx = r->acq->search->lag_bins;
  for (size_t k = 0; k < r->acq->n_peaks; k++)
    {
      const size_t c = r->acq->peaks[k].col;
      const size_t d = c > col ? c - col : col - c;
      if ((d < nx - d ? d : nx - d) <= half)
        return 1;
    }
  return 0;
}

/* The case the zone is for (#3067): a strong emitter S 20 dB above a weak
   one W, data throughout, on a deep block. S's data spreads it over many
   Doppler rows of its own lag, each its own peak under today's zone (one
   row), so its lag takes every slot of the list and W is never listed;
   with the whole-Doppler zone S is one peak and W is listed. The list, not
   the acquisition, is asserted: whether W is then taken is §7.2 rule 2's
   business; that the zone covers every row is read off the working mask.
   Sabotage, each red: the zone one native row (today's) -> W unlisted;
   one row short of the whole axis -> a row left unmasked. */
static int
_test_zone_lists_the_weaker (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const double    tsym = POOL_FS / POOL_RS;
  float _Complex *w, *sg;
  size_t          nw, ns;
  double         *dw, *ds;
  /* W carries the noise at 47 dB-Hz; S is rendered clean and set 20 dB
     above it, 400 chips and 500 Hz away. */
  dp_dsss_capture (code, 1023, POOL_SPC, POOL_FS, tsym, -300.0, 47.0, 200,
                   1000, 21, &w, &nw, &dw);
  dp_dsss_capture (code, 1023, POOL_SPC, POOL_FS, tsym, 200.0, 300.0, 200, 200,
                   22, &sg, &ns, &ds);
  const size_t    n    = nw < ns ? nw : ns;
  float _Complex *both = malloc (n * sizeof *both);
  DP_REQUIRE (both);
  for (size_t i = 0; i < n; i++)
    both[i] = w[i] + 10.0f * sg[i];
  /* Three search-only receivers on the deep block: on W alone (where W's
     lag is), and on S + W with today's zone and with the receiver's. */
  dp_async_dsss_receiver_state_t *r[3];
  for (int k = 0; k < 3; k++)
    {
      r[k] = dp_async_dsss_receiver_create (
          code, 1023, POOL_CR, POOL_RS, POOL_FS, 2, 47.0, 1e-3, 0.9, 5000.0, 4,
          8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false,
          450.0 / POOL_RS, 500.0, true);
      DP_REQUIRE (r[k] != NULL);
      (void)dp_async_dsss_receiver_set_search_only (r[k], 4);
    }
  DP_REQUIRE (dp_correlator_set_peak_zone (r[1]->acq, 0, 0.0, NULL) == DP_OK);
  DP_REQUIRE (r[0]->acq->search->coherent_bins > 1);
  size_t       w_col = 0, dwells = 0, old_full = 0, old_w = 0, new_w = 0;
  int          full_col = 1;
  const size_t blk      = 1023 * POOL_SPC;
  for (size_t pos = 0; pos + blk <= n; pos += blk)
    {
      uint64_t d0[3];
      for (int k = 0; k < 3; k++)
        {
          d0[k] = r[k]->acq->search->dwells;
          float _Complex none[1];
          (void)dp_async_dsss_receiver_steps (r[k], (k ? both : w) + pos, blk,
                                              none, 0);
          double ph[16], dp[16], cc[16];
          (void)dp_async_dsss_receiver_take_picks (r[k], ph, dp, cc, 16);
        }
      if (r[0]->acq->search->dwells == d0[0])
        continue;
      DP_REQUIRE (r[0]->acq->n_peaks > 0);
      w_col = r[0]->acq->peaks[0].col;
      dwells++;
      old_full += r[1]->acq->n_peaks == r[1]->acq->max_peaks;
      old_w += _lists_lag (r[1], w_col, 2);
      new_w += _lists_lag (r[2], w_col, 2);
      /* The zone covers every Doppler bin of every roll: the first listed
         peak's lag is masked in every surface row. */
      const size_t nx   = r[2]->acq->search->lag_bins;
      const size_t rows = r[2]->acq->search->n_surf / nx;
      const size_t c0   = r[2]->acq->peaks[0].col;
      for (size_t row = 0; row < rows; row++)
        full_col &= r[2]->acq->peak_mask[row * nx + c0] != 0;
    }
  DP_CHECK_MSG (full_col, "the zone covers every Doppler row");
  DP_CHECK_MSG (dwells >= 2, "the run decided dwells to compare");
  DP_CHECK_MSG (old_full == dwells, "today's list fills on S's lag");
  DP_CHECK_MSG (old_w == 0, "today's list never lists W");
  DP_CHECK_MSG (new_w == dwells, "the whole-Doppler zone lists W");
  if (old_w || new_w != dwells || old_full != dwells)
    printf ("  zone lists the weaker: %zu dwells; old full %zu, old W %zu, "
            "new W %zu\n",
            dwells, old_full, old_w, new_w);
  for (int k = 0; k < 3; k++)
    dp_async_dsss_receiver_destroy (r[k]);
  free (both);
  free (w);
  free (sg);
  free (dw);
  free (ds);
  return 0;
}

/* The first decided dwell that picks both of a same-lag pair, today's zone
   (`first[0]`) and the receiver's (`first[1]`); B starts `b_pre` samples
   in (A at 1000). -1 when a run never picks both. */
static int
_pair_first_two (const uint8_t *code, size_t b_pre, double half_df,
                 int first[2])
{
  const double    tsym = POOL_FS / POOL_RS, fc = 2.5e9;
  const size_t    n_sym = 1000;
  float _Complex *a0, *b0;
  size_t          na, nb;
  double         *da, *db;
  /* Same delay (at b_pre = 1000), no residual: each channel puts its own
     Doppler on. A carries the noise at 50 dB-Hz; B is clean, 3 dB below.
     Code-only windows (450 of 500 symbols) for the deep block. */
  dp_dsss_windowed_capture (code, 1023, POOL_SPC, POOL_FS, tsym, 0.0, 50.0,
                            n_sym, 1000, 31, 450, 500, &a0, &na, &da);
  dp_dsss_windowed_capture (code, 1023, POOL_SPC, POOL_FS, tsym, 0.0, 300.0,
                            n_sym, b_pre, 32, 450, 500, &b0, &nb, &db);
  const size_t    n = (na < nb ? na : nb) - 8;
  float _Complex *x = calloc (n + 64, sizeof *x);
  float _Complex *y = malloc ((n + 64) * sizeof *y);
  DP_REQUIRE (x && y);
  const double    ppm[2] = { half_df / fc * 1e6, -half_df / fc * 1e6 };
  float _Complex *src[2] = { a0, b0 };
  for (int k = 0; k < 2; k++)
    {
      dp_doppler_channel_state_t *ch
          = dp_doppler_channel_create (POOL_FS, fc, ppm[k], 0.0);
      DP_REQUIRE (ch);
      size_t got = 0;
      for (size_t o = 0; o < n && got < n; o += 32768)
        {
          const size_t m = n - o < 32768 ? n - o : 32768;
          got += dp_doppler_channel_execute (ch, src[k] + o, m, y + got,
                                             n + 64 - got);
        }
      for (size_t i = 0; i < n && i < got; i++)
        x[i] += (k ? (float)M_SQRT1_2 : 1.0f) * y[i];
      dp_doppler_channel_destroy (ch);
    }
  /* Two search-only receivers, today's zone and the receiver's, ranks 0
     and 1, no exclusions. */
  dp_async_dsss_receiver_state_t *r[2];
  for (int k = 0; k < 2; k++)
    {
      r[k] = dp_async_dsss_receiver_create (
          code, 1023, POOL_CR, POOL_RS, POOL_FS, 2, 47.0, 1e-3, 0.9,
          half_df + 5e3, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, fc, 0.0,
          false, 450.0 / POOL_RS, 500.0, true);
      DP_REQUIRE (r[k] != NULL);
      (void)dp_async_dsss_receiver_set_search_only (r[k], 2);
    }
  DP_REQUIRE (dp_correlator_set_peak_zone (r[0]->acq, 0, 0.0, NULL) == DP_OK);
  DP_REQUIRE (r[0]->acq->search->coherent_bins == 90);
  int          dwell[2] = { 0, 0 };
  const size_t blk      = 1023 * POOL_SPC;
  first[0] = first[1] = -1;
  /* Stop once the answer is known: both picked by both, or the zone's
     search already more than a dwell past today's first. */
  for (size_t pos = 0; pos + blk <= n; pos += blk)
    for (int k = 0; k < 2; k++)
      {
        if (first[0] > 0 && (first[1] > 0 || dwell[1] > first[0] + 1))
          break;
        const uint64_t d0 = r[k]->acq->search->dwells;
        float _Complex none[1];
        (void)dp_async_dsss_receiver_steps (r[k], x + pos, blk, none, 0);
        double       ph[16], dp[16], cc[16];
        const size_t np
            = dp_async_dsss_receiver_take_picks (r[k], ph, dp, cc, 16);
        if (r[k]->acq->search->dwells == d0)
          continue;
        dwell[k]++;
        if (first[k] < 0 && np >= 2)
          first[k] = dwell[k];
      }
  for (int k = 0; k < 2; k++)
    dp_async_dsss_receiver_destroy (r[k]);
  free (x);
  free (y);
  free (a0);
  free (b0);
  free (da);
  free (db);
  return 0;
}

/* The one place the zone may delay a pick where the list did not fill: a
   carrier set. The zone is a lag at the middle of the total dwell; the
   pick's key is a code phase carried to the dwell's end at the hit's own
   Doppler, so two emitters at one lag and +/- 30 kHz apart at 2.5 GHz on a
   deep block (D = 90, 1023 chips) sit (60e3 / 2.5e9) * 0.5 * 90 * 1023 =
   1.1 chips apart in the key: today the search can pick both in one total
   dwell; the whole-Doppler zone lists one, and the other waits for a later
   dwell, by when their code clocks (24 ppm apart) have drifted apart. Each
   emitter is dilated by its own DopplerChannel so the stimulus is the
   physics the key models.

   The contract is the bound: over start offsets on both sides of the
   crossing (where B at 993-996 samples puts the pair on one lag in the
   second dwell's middle), at +/- 30 kHz and at +/- 2 kHz, the second
   emitter is picked, and no more than one dwell later than today. The
   receiver's zone, half the key wide, sits inside the key, so no wait was
   measured at all; a zone as wide as the key waited two dwells at
   +/- 2 kHz. Sabotage, red: a zone that never releases a lag (the working
   mask kept from dwell to dwell) -> the second is never picked. */
static int
_test_zone_delays_a_same_lag_pair (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const size_t offs[4] = { 990, 993, 996, 1004 };
  int          worst = 0, all = 1;
  for (int o = 0; o < 4; o++)
    {
      int first[2];
      DP_REQUIRE (_pair_first_two (code, offs[o], 30e3, first) == 0);
      all &= first[0] > 0 && first[1] > 0;
      const int late = first[1] - first[0];
      worst          = late > worst ? late : worst;
      if (first[1] < 0 || late > 1)
        printf ("  same-lag pair, B at %zu: both picked first at dwell %d "
                "today, %d with the zone\n",
                offs[o], first[0], first[1]);
    }
  DP_CHECK_MSG (all, "the pair is picked, today and with the zone");
  DP_CHECK_MSG (worst <= 1, "the zone delays the second by one dwell at most");
  /* A small Doppler difference (+/- 2 kHz): the key-against-lag offset is
     tiny and the pair drifts 0.07 chip a dwell, so a zone wider than the
     key would hold the second back for many dwells; within the key it is
     still one at most, wherever today picks both. */
  const size_t b_at[3]  = { 999, 1002, 1005 };
  int          compared = 0;
  for (int o = 0; o < 3; o++)
    {
      int first[2];
      DP_REQUIRE (_pair_first_two (code, b_at[o], 2e3, first) == 0);
      /* Where today picks both inside the run, the zone picks both and at
         most a dwell later; a run where today never does says nothing, so
         most runs must be ones where it does. */
      const int ok
          = first[0] > 0 ? first[1] > 0 && first[1] <= first[0] + 1 : 1;
      compared += first[0] > 0;

      if (!ok)
        printf ("  same-lag pair +/- 2 kHz, B at %zu: both first at dwell %d "
                "today, %d with the zone\n",
                b_at[o], first[0], first[1]);
      DP_CHECK_MSG (ok, "+/- 2 kHz: one dwell later at most");
    }
  DP_CHECK_MSG (compared >= 2, "+/- 2 kHz: today picks both in most runs");
  return 0;
}

/* The key's hole is not widened by the zone: two emitters whose code
   phases are 1.5 chips apart (outside the one-chip key) at different
   Dopplers are both held. Within a chip they are one signal to the pick
   and the exclusions, today as with the zone (async-dsss-receiver.md §7.2
   rules 1 and 3): 2 of 1023 code phases per held emitter. The receiver's
   zone is half the key (1 column at 2.002 a chip), so a neighbour 1.5
   chips away is well outside it. Sabotage: a zone of 2 chips (4 columns)
   -> B is masked behind A and never held. (At 3 columns B's peak lands
   just past the zone; the carrier bound's +/- 2 kHz set is what catches a
   zone that wide.) */
static int
_test_zone_keeps_a_neighbour_past_the_key (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const double    tsym = POOL_FS / POOL_RS;
  float _Complex *a, *b;
  size_t          na, nb;
  double         *da, *db;
  /* 3 samples at 2 a chip: 1.5 chips apart. */
  /* Pure code (a code-only window over every symbol): no data transition
     ever weakens A below B in an epoch, so A lists first every dwell. */
  dp_dsss_windowed_capture (code, 1023, POOL_SPC, POOL_FS, tsym, 200.0, 50.0,
                            300, 1000, 41, 300, 300, &a, &na, &da);
  dp_dsss_windowed_capture (code, 1023, POOL_SPC, POOL_FS, tsym, -300.0, 120.0,
                            300, 1003, 42, 300, 300, &b, &nb, &db);
  const size_t n = na < nb ? na : nb;
  for (size_t i = 0; i < n; i++)
    a[i] += 0.5f * b[i]; /* 6 dB below A: A lists first every dwell */
  dp_async_dsss_pool_state_t *p = _pool (code, 3, 0.0, 1);
  DP_REQUIRE (p != NULL);
  const size_t blk = 1023 * POOL_SPC;
  for (size_t pos = 0; pos < n; pos += blk)
    (void)dp_async_dsss_pool_push (p, a + pos, n - pos < blk ? n - pos : blk);
  async_dsss_receiver_status_t st[3];
  size_t                       held = 0;
  for (size_t i = 0; i < 3; i++)
    {
      st[i] = dp_async_dsss_pool_status (p, i);
      held += st[i].state == ASYNC_DSSS_RX_TRACKING;
    }
  DP_CHECK_MSG (held == 2, "both emitters 1.5 chips apart are held");
  DP_CHECK (_chip_dist (st[0].chip_phase, st[1].chip_phase, 1023.0) > 1.0);
  DP_CHECK (p->released_duplicate == 0);
  dp_async_dsss_pool_destroy (p);
  free (a);
  free (b);
  free (da);
  free (db);
  return 0;
}

/* A lone strong emitter seeds nothing but itself under the zone: with its
   own lag masked in every row, its code cross-correlation sidelobes reach
   the list where its own lag used to fill it, and §7.2 rule 2 must still
   refuse them -- on one epoch (where rule 2 has no floor) and on a deep
   block with data inside it. S at 20 and 30 dB over the design level,
   three receivers: one holds S, and no other is ever seeded (no ghost or
   duplicate released, the other two idle every push); with a real weak
   emitter W beside S, W is held where the deep search can take it (12 dB
   down) and nothing beyond S and W is ever seeded. */
static int
_test_zone_seeds_no_ghost (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const double tsym = POOL_FS / POOL_RS;
  /* lvl 0, 1: S alone at +20, +30 dB. lvl 2: W at the design level and
     S 12 dB over it, where the deep search takes both (§7.2's table): S
     and W held. lvl 3: S 20 dB over W, past that table's limit (#3067):
     S held, W's state not asserted. Nothing else is ever busy. */
  for (int deep = 0; deep < 2; deep++)
    for (int lvl = 0; lvl < 4; lvl++)
      {
        const double    gain   = lvl == 1 ? 30.0 : lvl == 2 ? 12.0 : 20.0;
        const int       with_w = lvl >= 2;
        float _Complex *nz, *sg;
        size_t          nn, ns;
        double         *dn, *ds;
        /* The design's noise (a 47 dB-Hz capture less its clean copy) plus
           S scaled `gain` dB over it, S at 200 Hz, data throughout. */
        dp_dsss_capture (code, 1023, POOL_SPC, POOL_FS, tsym, 200.0, 47.0,
                         deep ? 600 : 300, 1000, 51, &nz, &nn, &dn);
        dp_dsss_capture (code, 1023, POOL_SPC, POOL_FS, tsym, 200.0, 300.0,
                         deep ? 600 : 300, 1000, 51, &sg, &ns, &ds);
        size_t          n   = nn < ns ? nn : ns;
        const float     g   = (float)pow (10.0, gain / 20.0);
        float _Complex *wc  = NULL;
        double         *dwc = NULL;
        size_t          nwc = n;
        if (with_w) /* W, clean, at unit amplitude: the design level */
          {
            dp_dsss_capture (code, 1023, POOL_SPC, POOL_FS, tsym, -300.0,
                             300.0, deep ? 600 : 300, 200, 52, &wc, &nwc,
                             &dwc);
            n = n < nwc ? n : nwc;
          }
        for (size_t i = 0; i < n; i++)
          nz[i] = (nz[i] - sg[i]) + g * sg[i] + (with_w ? wc[i] : 0.0f);
        dp_async_dsss_pool_state_t *p = dp_async_dsss_pool_create (
            code, 1023, 3, POOL_CR, POOL_RS, POOL_FS, 2, 47.0, 1e-3, 0.9,
            5000.0, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0,
            false, deep ? 450.0 / POOL_RS : 0.0, deep ? 500.0 : 0.0, 1, true);
        DP_REQUIRE (p != NULL);
        const size_t blk    = 1023 * POOL_SPC;
        int          others = 0;
        for (size_t pos = 0; pos < n; pos += blk)
          {
            (void)dp_async_dsss_pool_push (p, nz + pos,
                                           n - pos < blk ? n - pos : blk);
            int busy = 0;
            for (size_t i = 0; i < 3; i++)
              busy += dp_async_dsss_pool_status (p, i).state
                      != ASYNC_DSSS_RX_IDLE;
            others |= busy > (with_w ? 2 : 1);
          }
        int held = 0;
        for (size_t i = 0; i < 3; i++)
          held += dp_async_dsss_pool_status (p, i).state
                  == ASYNC_DSSS_RX_TRACKING;
        if (lvl == 2)
          DP_CHECK_MSG (held == 2, "S and W 12 dB apart are both held");
        else
          DP_CHECK_MSG (held >= 1 && held <= (with_w ? 2 : 1),
                        "the strong emitter is held");
        DP_CHECK_MSG (!others && p->released_ghost == 0
                          && p->released_duplicate == 0,
                      "nothing but the strong emitter is ever seeded");
        if (held < 1 || (lvl == 2 && held != 2) || others || p->released_ghost
            || p->released_duplicate)
          printf ("  no ghost: deep %d +%.0f dB: held %d, another busy %d, "
                  "ghosts %llu, duplicates %llu\n",
                  deep, gain, held, others,
                  (unsigned long long)p->released_ghost,
                  (unsigned long long)p->released_duplicate);
        dp_async_dsss_pool_destroy (p);
        free (nz);
        free (sg);
        free (dn);
        free (ds);
        free (wc);
        free (dwc);
      }
  return 0;
}

/* One search (docs/design/async-dsss-receiver.md §5): two receivers on
   two emitters hold both, and no receiver ever runs a search of its own --
   every receiver's engine stays at 0 -- while the controller's one search
   runs only while a receiver is idle: once both hold it is not fed again,
   so its period count and its engine stop where they were. Sabotage, each
   red: feed the search every period (its count keeps rising); leave the
   receivers unpooled (their engines move). */
static int
_test_search_runs_once (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t                      n;
  float _Complex             *x = _two_emitters (code, &n);
  dp_async_dsss_pool_state_t *p = _pool (code, 2, 0.0, 1);
  DP_REQUIRE (p != NULL);
  const size_t blk     = 1023 * POOL_SPC;
  size_t       both_at = 0, periods = 0;
  uint64_t     count_then = 0, engine_then = 0;
  for (size_t pos = 0; pos + blk <= n; pos += blk)
    {
      (void)dp_async_dsss_pool_push (p, x + pos, blk);
      periods++;
      const int both
          = dp_async_dsss_pool_status (p, 0).state == ASYNC_DSSS_RX_TRACKING
            && dp_async_dsss_pool_status (p, 1).state
                   == ASYNC_DSSS_RX_TRACKING;
      if (both && !both_at)
        {
          both_at     = periods;
          count_then  = p->search_periods;
          engine_then = dp_correlator_position (p->search->acq);
        }
    }
  DP_REQUIRE_MSG (both_at > 0, "both emitters held");
  /* A long run past it: at least two controller steps (it runs once a
     group of ASYNC_DSSS_POOL_PERIODS_PER_JOIN periods), so the search
     would have been fed again at least once more if the rule did not stop
     it. */
  DP_CHECK (periods >= both_at + 2 * (size_t)ASYNC_DSSS_POOL_PERIODS_PER_JOIN);
  for (size_t i = 0; i < 2; i++)
    DP_CHECK (dp_correlator_position (p->rx[i]->acq) == 0);
  DP_CHECK_MSG (p->search_periods == count_then,
                "the search ran with every receiver holding");
  DP_CHECK (dp_correlator_position (p->search->acq) == engine_then);
  DP_CHECK (!p->searching);
  dp_async_dsss_pool_destroy (p);
  free (x);
  return 0;
}

/* Duplicates: two holders within a chip hold one signal, and the keeper
   is tracking over refining, then the longer in its state, then the
   higher C/N0, then the lower index; the other is reset to idle, and the
   search is excluded from the keeper's phase. The holders here are seeded by
   hand through the borrow, so each rung is set up exactly. Sabotage: drop the
   duplicate pass -> both keep refining; reverse any rung -> the wrong
   receiver is reset. */
static int
_test_duplicates_keep_the_first (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  static float _Complex z[POOL_GROUP];
  /* rung: 0 = longer in its state, 1 = higher C/N0, 2 = lower index. */
  for (int rung = 0; rung < 3; rung++)
    {
      dp_async_dsss_pool_state_t *p = _pool (code, 3, 0.0, 1);
      DP_REQUIRE (p != NULL);
      dp_async_dsss_receiver_state_t *r0 = dp_async_dsss_pool_receiver (p, 0);
      dp_async_dsss_receiver_state_t *r1 = dp_async_dsss_pool_receiver (p, 1);
      DP_REQUIRE (r0 && r1 && !dp_async_dsss_pool_receiver (p, 3));
      size_t expect_keeper;
      if (rung == 0)
        {
          /* r1 first, so r1 is in refining longer despite the higher
             index and the lower C/N0. */
          DP_REQUIRE (dp_async_dsss_receiver_seed (r1, 100.0, 0.0, 45.0)
                      == DP_OK);
          (void)dp_async_dsss_pool_push (p, z, POOL_GROUP);
          DP_REQUIRE (dp_async_dsss_pool_status (p, 1).state
                      == ASYNC_DSSS_RX_REFINING);
          DP_REQUIRE (dp_async_dsss_receiver_seed (r0, 100.5, 0.0, 60.0)
                      == DP_OK);
          expect_keeper = 1;
        }
      else
        {
          DP_REQUIRE (dp_async_dsss_receiver_seed (r0, 100.0, 0.0, 45.0)
                      == DP_OK);
          DP_REQUIRE (dp_async_dsss_receiver_seed (r1, 100.3, 0.0,
                                                   rung == 1 ? 50.0 : 45.0)
                      == DP_OK);
          expect_keeper = rung == 1 ? 1 : 0;
        }
      const uint64_t before = p->released_duplicate;
      (void)dp_async_dsss_pool_push (p, z, POOL_GROUP);
      DP_CHECK (p->released_duplicate == before + 1);
      const size_t loser = 1 - expect_keeper;
      DP_CHECK_MSG (dp_async_dsss_pool_status (p, expect_keeper).state
                        == ASYNC_DSSS_RX_REFINING,
                    "the keeper is still refining");
      DP_CHECK_MSG (dp_async_dsss_pool_status (p, loser).state
                        == ASYNC_DSSS_RX_IDLE,
                    "the loser is reset");
      /* The search now excludes the keeper's phase, the one holder. */
      DP_CHECK (dp_async_dsss_receiver_get_n_exclusions (p->search) == 1);
      dp_async_dsss_pool_destroy (p);
    }
  return 0;
}

/* Two holders more than a chip apart are two signals: neither is reset.
   The zone is circular, so 0.4 and 1022.8 are 0.6 chips apart and ARE one.
   Sabotage: compare without the wrap -> the across-zero pair survives;
   widen the zone -> the 1.5-chip pair is reset. */
static int
_test_duplicate_zone_is_one_chip_circular (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  static float _Complex z[POOL_GROUP];
  static const struct
  {
    double a, b;
    int    dup;
  } rows[] = { { 100.0, 101.5, 0 }, { 0.4, 1022.8, 1 }, { 500.0, 500.9, 1 } };
  for (size_t k = 0; k < sizeof rows / sizeof rows[0]; k++)
    {
      dp_async_dsss_pool_state_t *p = _pool (code, 2, 0.0, 1);
      DP_REQUIRE (p != NULL);
      DP_REQUIRE (dp_async_dsss_receiver_seed (p->rx[0], rows[k].a, 0.0, 45.0)
                  == DP_OK);
      DP_REQUIRE (dp_async_dsss_receiver_seed (p->rx[1], rows[k].b, 0.0, 45.0)
                  == DP_OK);
      /* status() advances a refining receiver's phase by the samples
         since its seed, alike for both, so their distance holds. */
      const size_t held = dp_async_dsss_pool_push (p, z, POOL_GROUP);
      DP_CHECK_MSG (held == (rows[k].dup ? 1u : 2u),
                    rows[k].dup ? "one signal held twice" : "two signals");
      DP_CHECK (p->released_duplicate == (uint64_t)rows[k].dup);
      dp_async_dsss_pool_destroy (p);
    }
  return 0;
}

/* The lost are released: a receiver whose release rule fired (the emitter
   switched off) is reset by the controller and searches again. CODE7's
   fixture, as the receiver's own switch-off test. Sabotage: skip the lost
   pass -> the receiver stays lost and the count stays 0. */
static int
_test_lost_is_released (void)
{
  const size_t    sf = 7, spc = 4;
  const double    fs = 1.0e6 * (double)spc, sym_rate = 35714.29;
  const double    cn0 = 70.0, confirm_s = 0.02;
  const size_t    pre = sf * spc * 5 + 3, tail_n = (size_t)(0.25 * fs);
  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, fs / sym_rate, 0.0, cn0, 400, pre, 7,
                   &x, &n, &data);
  float _Complex *off = calloc (tail_n, sizeof *off);
  dp_dsss_add_awgn (off, tail_n, fs, cn0, 99u); /* the shipped noise, #689 */
  dp_async_dsss_pool_state_t *p = dp_async_dsss_pool_create (
      CODE7, 7, 1, 1.0e6, sym_rate, 4.0 * 1.0e6, 2, cn0, 1e-2, 0.9, 500.0, 4,
      8, 0, 0.5, 4, 14.0, 32, 8, false, 100000, 0.0, confirm_s, false, 0.0,
      0.0, 1, true);
  DP_REQUIRE (p != NULL);
  DP_REQUIRE (dp_async_dsss_receiver_seed (p->rx[0], 0.0, 0.0, cn0) == DP_OK);
  const size_t te = sf * spc;
  for (size_t pos = pre; pos < n; pos += te)
    (void)dp_async_dsss_pool_push (p, x + pos, n - pos < te ? n - pos : te);
  DP_REQUIRE (dp_async_dsss_pool_status (p, 0).state
              == ASYNC_DSSS_RX_TRACKING);
  int searched_again = 0;
  for (size_t pos = 0; pos < tail_n && !searched_again; pos += 1024)
    {
      (void)dp_async_dsss_pool_push (
          p, off + pos, tail_n - pos < 1024 ? tail_n - pos : 1024);
      /* The controller runs right after every whole group, and status()
         is as of the last one, so lost is never what it reads. */
      DP_CHECK (dp_async_dsss_pool_status (p, 0).state != ASYNC_DSSS_RX_LOST);
      searched_again = p->released_lost > 0;
    }
  DP_CHECK_MSG (searched_again, "the lost receiver was released");
  DP_CHECK (p->released_lost == 1);
  DP_CHECK (dp_async_dsss_pool_status (p, 0).state != ASYNC_DSSS_RX_TRACKING);
  dp_async_dsss_pool_destroy (p);
  free (off);
  free (x);
  free (data);
  return 0;
}

/* The search stops while every receiver holds and a fresh one starts on
   the period one is free, reset at its first period (#3267, P3); a
   receiver the controller resets keeps reporting under the same names
   (P42). Two receivers on the two-emitter stream, B leaving at CI_B_OFF,
   pushed one group (one control step) at a time, telemetry attached:
   - while both hold, the search is not fed: search_periods stands still;
   - in the group after B's holder is released, the search is fed again,
     a group's periods, and its correlator stands where a fresh search
     does after one group: its position counts samples since its reset
     (dp_correlator_position()), so a search resumed from where it
     stopped reads its earlier count plus a group;
   - the released receiver is still attached to the same context, and the
     registry holds the same probes under the same names.
   Sabotage: drop feed_group's reset of a restarted search -> red on the
   position; feed the search while all hold -> red on search_periods;
   detach a receiver in its reset -> red on the names. */
static int
_test_search_restarts_and_names_survive (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t                      n;
  float _Complex             *x     = _two_emitters_off (code, CI_B_OFF, &n);
  dp_async_dsss_pool_state_t *p     = _pool (code, 2, CI_CONFIRM_S, 1);
  dp_async_dsss_pool_state_t *fresh = _pool (code, 2, CI_CONFIRM_S, 1);
  dp_tlm_t                   *t     = dp_tlm_create (1 << 12);
  DP_REQUIRE (p && fresh && x && t);
  DP_REQUIRE (dp_async_dsss_pool_set_telemetry (p, t, "p", 1) == DP_OK);
  const uint32_t n_probes = t->n_probes;
  /* A fresh search after one group: where a restarted one must stand. */
  (void)dp_async_dsss_pool_push (fresh, x, POOL_GROUP);
  const uint64_t fresh_pos  = dp_correlator_position (fresh->search->acq);
  int            held_still = 1, restarted = 0, both_held = 0;
  uint64_t       stop_pos = 0;
  for (size_t pos = 0; pos + 2 * POOL_GROUP <= n && !restarted;
       pos += POOL_GROUP)
    {
      const uint64_t periods  = p->search_periods;
      const uint64_t released = p->released_lost;
      const int      all_held = !p->searching && p->search_periods > 0;
      (void)dp_async_dsss_pool_push (p, x + pos, POOL_GROUP);
      if (all_held && p->released_lost == released && !p->searching)
        held_still &= p->search_periods == periods;
      if (!p->searching && !both_held)
        {
          both_held = 1;
          stop_pos  = dp_correlator_position (p->search->acq);
        }
      if (p->released_lost > released)
        {
          /* The next group restarts the search. */
          const uint64_t before = p->search_periods;
          (void)dp_async_dsss_pool_push (p, x + pos + POOL_GROUP, POOL_GROUP);
          DP_CHECK (p->searching);
          DP_CHECK (p->search_periods
                    == before + ASYNC_DSSS_POOL_PERIODS_PER_JOIN);
          DP_REQUIRE_MSG (stop_pos > 0,
                          "the search was fed before it stopped");
          DP_CHECK_MSG (dp_correlator_position (p->search->acq) == fresh_pos,
                        "a restarted search is reset at its first period");
          restarted = 1;
        }
    }
  DP_REQUIRE_MSG (both_held && restarted,
                  "both held, then B's holder was released");
  DP_CHECK_MSG (held_still,
                "the search is not fed while every receiver holds");
  DP_CHECK (p->rx[0]->tlm == t && p->rx[1]->tlm == t);
  DP_CHECK_MSG (t->n_probes == n_probes, "no probe registered anew");
  int found = 0;
  for (uint32_t k = 0; k < t->n_probes; k++)
    found += strcmp (t->probes[k].name, "p.r0.state") == 0
             || strcmp (t->probes[k].name, "p.r1.state") == 0;
  DP_CHECK_MSG (found == 2, "a reset receiver reports under its names");
  dp_async_dsss_pool_destroy (p);
  dp_async_dsss_pool_destroy (fresh);
  dp_tlm_destroy (t);
  free (x);
  return 0;
}

/* With `reacquire`, a receiver whose emitter leaves releases itself: it
   goes back to idle with the controller's lost count at 0, where without
   it the controller releases it (#3267, P11). Two pools on the stream
   where B leaves, the same arguments but `reacquire`; status() reads lost
   in neither (P21), so what tells the two apart is who released the
   receiver. Sabotage: let a reacquire receiver enter lost (its release
   clock's reacquire branch) -> the controller releases it and the count
   reads 1. */
static int
_test_reacquire_releases_itself (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t          n;
  float _Complex *x = _two_emitters_off (code, CI_B_OFF, &n);
  DP_REQUIRE (x != NULL);
  for (int reacquire = 0; reacquire < 2; reacquire++)
    {
      dp_async_dsss_pool_state_t *p = dp_async_dsss_pool_create (
          code, 1023, 2, POOL_CR, POOL_RS, POOL_CR * (double)POOL_SPC, 2, 47.0,
          1e-3, 0.9, 500.0, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0,
          CI_CONFIRM_S, reacquire, 0.0, 0.0, 1, true);
      DP_REQUIRE (p != NULL);
      int    tracked[2] = { 0, 0 }, idle_rx = -1;
      double seed_cn0[2] = { NAN, NAN };
      size_t lost_read   = 0;
      for (size_t pos = 0; pos + POOL_GROUP <= n; pos += POOL_GROUP)
        {
          (void)dp_async_dsss_pool_push (p, x + pos, POOL_GROUP);
          for (size_t i = 0; i < 2; i++)
            {
              const async_dsss_receiver_status_t s
                  = dp_async_dsss_pool_status (p, i);
              lost_read += s.state == ASYNC_DSSS_RX_LOST;
              if (tracked[i] && s.state == ASYNC_DSSS_RX_IDLE && idle_rx < 0)
                idle_rx = (int)i;
              if (s.state == ASYNC_DSSS_RX_TRACKING && !tracked[i])
                {
                  tracked[i]  = 1;
                  seed_cn0[i] = s.cn0_dbhz_est;
                }
            }
        }
      DP_REQUIRE_MSG (idle_rx >= 0 && tracked[0] && tracked[1],
                      "both held, then a holder went back to idle");
      /* B, the one that leaves, is 3 dB under A: its holder has the lower
         seed C/N0, and it is that one that goes idle. */
      DP_CHECK_MSG (seed_cn0[idle_rx] < seed_cn0[1 - idle_rx],
                    "the holder that goes idle is B's");
      /* Released by neither of the controller's other rules. */
      DP_CHECK (p->released_duplicate == 0 && p->released_ghost == 0);
      DP_CHECK (lost_read == 0);
      if (reacquire)
        DP_CHECK_MSG (p->released_lost == 0,
                      "a reacquire receiver releases itself");
      else
        DP_CHECK_MSG (p->released_lost >= 1,
                      "without reacquire the controller releases it");
      dp_async_dsss_pool_destroy (p);
    }
  free (x);
  return 0;
}

/* A lock retuned through the borrow (dp_async_dsss_pool_receiver()) lasts
   only as long as the chain it reached (#3267, P44). Two receivers on the
   two emitters, one group a push:
   - set on receiver 1 while it is idle, it never reaches the chain built
     at its hand-over: that chain has the derived depth;
   - set on receiver 0 while it tracks, its Dll takes it and keeps it over
     the groups that follow;
   - after receiver 0 is reset and seeded again on its own emitter, the
     chain built at that hand-over has the derived depth again.
   The retuned depth is one no derivation gives at this point (a prime
   past the derived one). Sabotage: keep a retuned depth on the receiver
   and apply it at every chain build -> red on the idle and reseed rows. */
static int
_test_borrowed_lock_lasts_one_chain (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t                      n;
  float _Complex             *x = _two_emitters (code, &n);
  dp_async_dsss_pool_state_t *p = _pool (code, 2, 0.0, 1);
  DP_REQUIRE (p && x);
  dp_async_dsss_receiver_state_t *r0 = dp_async_dsss_pool_receiver (p, 0);
  dp_async_dsss_receiver_state_t *r1 = dp_async_dsss_pool_receiver (p, 1);
  DP_REQUIRE (r0 && r1 && dp_async_dsss_pool_receiver (p, 2) == NULL);
  /* The derived depth, from a pool no retune has touched, run first. */
  size_t derived = 0;
  {
    dp_async_dsss_pool_state_t *c = _pool (code, 2, 0.0, 1);
    DP_REQUIRE (c != NULL);
    for (size_t at = 0; at + POOL_GROUP <= n && !derived; at += POOL_GROUP)
      {
        (void)dp_async_dsss_pool_push (c, x + at, POOL_GROUP);
        if (c->rx[0]->state == ASYNC_DSSS_RX_TRACKING)
          derived = c->rx[0]->dll->n_looks;
      }
    dp_async_dsss_pool_destroy (c);
  }
  const size_t odd = 37; /* no derivation at this point gives a prime */
  DP_REQUIRE_MSG (derived && derived != odd,
                  "the retuned depth is not the derived one");
  dp_async_dsss_receiver_configure_lock_raw (r1, 0.4, 0.2, odd, 0.1, 5, 3);
  size_t pos = 0;
  for (; pos + POOL_GROUP <= n; pos += POOL_GROUP)
    {
      (void)dp_async_dsss_pool_push (p, x + pos, POOL_GROUP);
      if (r0->state == ASYNC_DSSS_RX_TRACKING
          && r1->state == ASYNC_DSSS_RX_TRACKING)
        break;
    }
  DP_REQUIRE_MSG (r0->state == ASYNC_DSSS_RX_TRACKING
                      && r1->state == ASYNC_DSSS_RX_TRACKING,
                  "both receivers track");
  DP_CHECK_MSG (r1->dll->n_looks == derived,
                "a lock set on an idle receiver never reaches its chain");
  /* Set while tracking: taken, and kept. */
  dp_async_dsss_receiver_configure_lock_raw (r0, 0.4, 0.2, odd, 0.1, 5, 3);
  DP_CHECK (r0->dll->n_looks == odd);
  for (size_t k = 0; k < 4 && pos + 2 * POOL_GROUP <= n; k++)
    {
      pos += POOL_GROUP;
      (void)dp_async_dsss_pool_push (p, x + pos, POOL_GROUP);
    }
  DP_CHECK_MSG (r0->state == ASYNC_DSSS_RX_TRACKING && r0->dll->n_looks == odd,
                "a lock set while tracking lasts while it tracks");
  /* A fresh seed on the same emitter: the next hand-over's chain is the
     derived one. */
  const async_dsss_receiver_status_t s0 = dp_async_dsss_pool_status (p, 0);
  dp_async_dsss_receiver_reset (r0);
  DP_REQUIRE (dp_async_dsss_receiver_seed (r0, s0.chip_phase, s0.doppler_hz,
                                           s0.cn0_dbhz_est)
              == DP_OK);
  for (pos += POOL_GROUP; pos + POOL_GROUP <= n; pos += POOL_GROUP)
    {
      (void)dp_async_dsss_pool_push (p, x + pos, POOL_GROUP);
      if (r0->state == ASYNC_DSSS_RX_TRACKING)
        break;
    }
  DP_REQUIRE_MSG (r0->state == ASYNC_DSSS_RX_TRACKING,
                  "receiver 0 tracks again");
  DP_CHECK_MSG (r0->dll->n_looks == derived,
                "the next hand-over builds the derived lock");
  dp_async_dsss_pool_destroy (p);
  free (x);
  return 0;
}

/* The controller's latency (#3267, P9): a release comes at most a group's
   periods less one after the period that called for it. The receivers are
   fed only when a group completes, and the controller runs at the end of
   that same step, so the claim is that the step that ends the group a
   receiver entered lost in releases it: after every push the receiver's
   OWN state (not status(), which P21 already holds) never reads lost, and
   it is released. The lost-release scene of _test_lost_is_released, a
   group a push. Sabotage: release a lost receiver only once it has been
   lost a whole group -> it is seen lost after a push, red here and
   nowhere else. */
static int
_test_release_latency (void)
{
  const size_t    sf = 7, spc = 4, te = sf * spc;
  const double    fs = 1.0e6 * (double)spc, sym_rate = 35714.29;
  const double    cn0 = 70.0, confirm_s = 0.02;
  const size_t    pre = sf * spc * 5 + 3, tail_n = (size_t)(0.25 * fs);
  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, fs / sym_rate, 0.0, cn0, 400, pre, 7,
                   &x, &n, &data);
  float _Complex *off = calloc (tail_n, sizeof *off);
  dp_dsss_add_awgn (off, tail_n, fs, cn0, 99u);
  dp_async_dsss_pool_state_t *p = dp_async_dsss_pool_create (
      CODE7, 7, 1, 1.0e6, sym_rate, 4.0 * 1.0e6, 2, cn0, 1e-2, 0.9, 500.0, 4,
      8, 0, 0.5, 4, 14.0, 32, 8, false, 100000, 0.0, confirm_s, false, 0.0,
      0.0, 1, true);
  DP_REQUIRE (p && off);
  DP_REQUIRE (dp_async_dsss_receiver_seed (p->rx[0], 0.0, 0.0, cn0) == DP_OK);
  for (size_t pos = pre; pos + te <= n; pos += te)
    (void)dp_async_dsss_pool_push (p, x + pos, te);
  DP_REQUIRE (p->rx[0]->state == ASYNC_DSSS_RX_TRACKING);
  const size_t g         = p->group;
  int          seen_lost = 0;
  for (size_t k = 0; (k + 1) * g <= tail_n && !p->released_lost; k++)
    {
      (void)dp_async_dsss_pool_push (p, off + k * g, g);
      seen_lost |= p->rx[0]->state == ASYNC_DSSS_RX_LOST;
    }
  DP_REQUIRE_MSG (p->released_lost == 1, "the lost receiver was released");
  DP_CHECK_MSG (!seen_lost, "the step that ends the group it entered lost "
                            "in releases it");
  dp_async_dsss_pool_destroy (p);
  free (off);
  free (x);
  free (data);
  return 0;
}

/* The thread count changes nothing but the cost: one worker and three
   give the same state, byte for byte, and the same symbols. The search
   spans +-5 kHz, several Doppler tiles, so on three workers it fans its
   tiles too (#2776) and that is compared as well. Sabotage: share one
   output row across receivers -> the symbols differ; run the controller
   inside the workers -> the order, and so the bytes, vary. */
static int
_test_threads_change_nothing (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t                      n;
  float _Complex             *x = _two_emitters (code, &n);
  dp_async_dsss_pool_state_t *a = _pool_span (code, 3, 0.0, 1, 5000.0);
  dp_async_dsss_pool_state_t *b = _pool_span (code, 3, 0.0, 3, 5000.0);
  DP_REQUIRE (a && b);
  DP_REQUIRE (b->search->acq->search->window_bins > 1); /* a fan to compare */
  DP_REQUIRE (dp_async_dsss_pool_get_threads (a) == 1);
  /* At most the receivers, and at most the box: 3 on a box of 3 or more. */
  DP_CHECK (dp_async_dsss_pool_get_threads (b) <= 3);
  int          same_syms = 1;
  const size_t blk       = 1023 * POOL_SPC;
  float _Complex sa[4096], sb[4096];
  for (size_t pos = 0; pos < n; pos += blk)
    {
      const size_t take = n - pos < blk ? n - pos : blk;
      DP_CHECK (dp_async_dsss_pool_push (a, x + pos, take)
                == dp_async_dsss_pool_push (b, x + pos, take));
      for (size_t i = 0; i < 3; i++)
        {
          const size_t na = dp_async_dsss_pool_symbols (a, i, sa, 4096);
          const size_t nb = dp_async_dsss_pool_symbols (b, i, sb, 4096);
          same_syms &= na == nb && memcmp (sa, sb, na * sizeof *sa) == 0;
        }
    }
  DP_CHECK (same_syms);
  const size_t   bytes = dp_async_dsss_pool_state_bytes (a);
  unsigned char *ba = malloc (bytes), *bb = malloc (bytes);
  DP_REQUIRE (bytes == dp_async_dsss_pool_state_bytes (b));
  dp_async_dsss_pool_get_state (a, ba);
  dp_async_dsss_pool_get_state (b, bb);
  /* The blob carries the thread count asked for (a mutator's value), so
     compare with it made equal. */
  DP_REQUIRE (dp_async_dsss_pool_set_threads (b, 1) == DP_OK);
  dp_async_dsss_pool_get_state (b, bb);
  DP_CHECK_MSG (memcmp (ba, bb, bytes) == 0, "1 and 3 workers, same bytes");
  free (ba);
  free (bb);
  dp_async_dsss_pool_destroy (a);
  dp_async_dsss_pool_destroy (b);
  free (x);
  return 0;
}

/* The state blob resumes the pool bit for bit mid-stream, carries the
   thread count, and is refused by a pool of another size or under a
   clobbered envelope. Sabotage: leave a receiver out of the blob -> the
   resumed pool diverges; skip the receiver-count check -> the 2-pool
   reads the 3-pool's blob. */
static int
_test_state_round_trip (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t          n;
  float _Complex *x = _two_emitters_off (code, CI_B_OFF, &n);
  /* Mid-period, so the blob carries a part-filled carry, with the release
     on and B leaving after the split. */
  const size_t half = 250000 + 777;
  DP_REQUIRE (half % (1023 * POOL_SPC * ASYNC_DSSS_POOL_PERIODS_PER_JOIN) != 0
              && half < CI_B_OFF);
  dp_async_dsss_pool_state_t *a = _pool (code, 3, CI_CONFIRM_S, 2);
  dp_async_dsss_pool_state_t *b = _pool (code, 3, CI_CONFIRM_S, 1);
  dp_async_dsss_pool_state_t *c = _pool (code, 2, CI_CONFIRM_S, 1);
  DP_REQUIRE (a && b && c);
  (void)dp_async_dsss_pool_push (a, x, half);
  DP_REQUIRE (a->carry_len
              == half % (1023 * POOL_SPC * ASYNC_DSSS_POOL_PERIODS_PER_JOIN));
  const size_t   bytes = dp_async_dsss_pool_state_bytes (a);
  unsigned char *blob  = malloc (bytes);
  dp_async_dsss_pool_get_state (a, blob);
  DP_CHECK (dp_async_dsss_pool_set_state (b, blob) == DP_OK);
  DP_CHECK (b->threads == 2 && b->carry_len == a->carry_len);
  DP_CHECK (b->samples == a->samples && b->samples == half);
  DP_CHECK (dp_async_dsss_pool_symbols_max_out (b) == 0);
  DP_CHECK (dp_async_dsss_pool_set_state (c, blob) == DP_ERR_INVALID);
  /* The envelope's size alone refuses c; a same-size blob naming another
     receiver count is refused by the count. */
  {
    unsigned char *forged = malloc (bytes);
    memcpy (forged, blob, bytes);
    const uint64_t two = 2;
    memcpy (forged + sizeof (dp_state_hdr_t), &two, sizeof two);
    DP_CHECK (dp_async_dsss_pool_set_state (b, forged) == DP_ERR_INVALID);
    free (forged);
  }
  /* The rest in blocks of 3001, both, every symbol compared. */
  int same = 1;
  float _Complex sa[8192], sb[8192];
  for (size_t pos = half; pos < n; pos += 3001)
    {
      const size_t take = n - pos < 3001 ? n - pos : 3001;
      (void)dp_async_dsss_pool_push (a, x + pos, take);
      (void)dp_async_dsss_pool_push (b, x + pos, take);
      for (size_t i = 0; i < 3; i++)
        {
          const size_t na = dp_async_dsss_pool_symbols (a, i, sa, 8192);
          const size_t nb = dp_async_dsss_pool_symbols (b, i, sb, 8192);
          same &= na == nb && memcmp (sa, sb, na * sizeof *sa) == 0;
        }
    }
  DP_CHECK_MSG (same, "the resumed pool emits what the original does");
  DP_CHECK_MSG (a->released_lost >= 1, "a release after the split");
  unsigned char *ea = malloc (bytes), *eb = malloc (bytes);
  dp_async_dsss_pool_get_state (a, ea);
  dp_async_dsss_pool_get_state (b, eb);
  DP_CHECK_MSG (memcmp (ea, eb, bytes) == 0, "resumed bit for bit");
  free (blob);
  free (ea);
  free (eb);
  dp_async_dsss_pool_destroy (a);
  dp_async_dsss_pool_destroy (b);
  dp_async_dsss_pool_destroy (c);
  free (x);
  return 0;
}

/* A refused restore is whole or nothing: a blob whose THIRD receiver is
   corrupt leaves the first two (and the pool's own fields) as they were,
   though they decode before the refusal. Sabotage: drop the snapshot
   rollback -> receivers 0 and 1 carry the blob's state. */
static int
_test_refused_restore_changes_nothing (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t                      n;
  float _Complex             *x   = _two_emitters (code, &n);
  dp_async_dsss_pool_state_t *src = _pool (code, 3, 0.0, 1);
  dp_async_dsss_pool_state_t *dst = _pool (code, 3, 0.0, 1);
  DP_REQUIRE (src && dst);
  (void)dp_async_dsss_pool_push (src, x, n / 2); /* src: holders, a clock */
  (void)dp_async_dsss_pool_push (dst, x, 4096);  /* dst: somewhere else   */
  const size_t   bytes = dp_async_dsss_pool_state_bytes (src);
  unsigned char *blob = malloc (bytes), *before = malloc (bytes),
                *after = malloc (bytes);
  dp_async_dsss_pool_get_state (src, blob);
  dp_async_dsss_pool_get_state (dst, before);
  /* Clobber receiver 2's envelope magic: its sub-blob follows the pool's
     header, extra, carry, and receivers 0 and 1. */
  const size_t cells = src->n_receivers * src->ghost_window;
  const size_t rx2   = sizeof (dp_state_hdr_t) + sizeof (uint64_t) * 10
                       + src->group * sizeof (float _Complex)
                       + src->n_receivers * sizeof (uint64_t)
                       + cells * (sizeof (double) + 1)
                       + 2 * dp_async_dsss_receiver_state_bytes (src->rx[0]);
  blob[rx2] ^= 0xFF;
  DP_CHECK (dp_async_dsss_pool_set_state (dst, blob) == DP_ERR_INVALID);
  dp_async_dsss_pool_get_state (dst, after);
  DP_CHECK_MSG (memcmp (before, after, bytes) == 0,
                "a refused restore left the pool as it was");
  blob[rx2] ^= 0xFF; /* and the blob itself restores */
  DP_CHECK (dp_async_dsss_pool_set_state (dst, blob) == DP_OK);
  free (blob);
  free (before);
  free (after);
  dp_async_dsss_pool_destroy (src);
  dp_async_dsss_pool_destroy (dst);
  free (x);
  return 0;
}

/* Telemetry is one context across every receiver, "<prefix>.r<i>", and
   one producer: refused unless push() runs on one worker, and set_threads()
   and set_state() then refuse more. A refusal leaves the previous
   attachment. A receiver past the pool reads as no receiver. Sabotage:
   drop the worker check -> a 2-worker pool attaches; drop set_threads'
   check -> an attached pool goes parallel. */
static int
_test_telemetry_one_producer (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  dp_async_dsss_pool_state_t *p = _pool (code, 2, 0.0, 2);
  dp_tlm_t *t0 = dp_tlm_create (1 << 12), *t1 = dp_tlm_create (1 << 12);
  DP_REQUIRE (p && t0 && t1);
  DP_CHECK (dp_async_dsss_pool_set_telemetry (p, t0, "p", 1)
            == DP_ERR_INVALID);
  DP_CHECK (p->tlm == NULL && p->rx[0]->tlm == NULL);
  DP_REQUIRE (dp_async_dsss_pool_set_threads (p, 1) == DP_OK);
  DP_CHECK (dp_async_dsss_pool_set_telemetry (p, t0, "p", 0)
            == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_pool_set_telemetry (p, t0, "p", 1) == DP_OK);
  DP_CHECK (p->rx[0]->tlm == t0 && p->rx[1]->tlm == t0);
  int found = 0;
  for (uint32_t k = 0; k < t0->n_probes; k++)
    found += strcmp (t0->probes[k].name, "p.r0.state") == 0
             || strcmp (t0->probes[k].name, "p.r1.state") == 0;
  DP_CHECK (found == 2);
  /* Parallel while attached: refused, unchanged. */
  DP_CHECK (dp_async_dsss_pool_set_threads (p, 2) == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_pool_get_threads (p) == 1);
  /* A prefix too long for "<p>.r<i>" is refused and keeps t0. */
  DP_CHECK (dp_async_dsss_pool_set_telemetry (
                p, t1, "abcdefghijklmnopqrstuvwxyz0123", 1)
            == DP_ERR_INVALID);
  DP_CHECK (p->tlm == t0 && p->rx[1]->tlm == t0);
  /* Detach frees the pool to go parallel. */
  DP_CHECK (dp_async_dsss_pool_set_telemetry (p, NULL, NULL, 1) == DP_OK);
  DP_CHECK (p->rx[0]->tlm == NULL && p->rx[1]->tlm == NULL);
  DP_CHECK (dp_async_dsss_pool_set_threads (p, 2) == DP_OK);
  DP_CHECK (dp_async_dsss_pool_status (p, 2).state
            == ASYNC_DSSS_POOL_NO_RECEIVER);
  float _Complex buf[4];
  DP_CHECK (dp_async_dsss_pool_symbols (p, 2, buf, 4) == 0);
  dp_async_dsss_pool_destroy (p);
  dp_tlm_destroy (t0);
  dp_tlm_destroy (t1);
  return 0;
}

/* A pool reset with a part-filled carry gives a blob set_state() takes:
   get_state() writes the whole carry and set_state() refuses a nonzero
   tail past carry_len, so a reset that left the old samples behind would
   give a blob no pool restores, one that is not a function of the state.
   Sabotage: drop the carry's memset from reset() -> red. */
static int
_test_reset_clears_the_carry (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t                      n;
  float _Complex             *x = _two_emitters (code, &n);
  dp_async_dsss_pool_state_t *p = _pool (code, 3, 0.0, 1);
  dp_async_dsss_pool_state_t *q = _pool (code, 3, 0.0, 1);
  DP_REQUIRE (p && q && x && n > POOL_GROUP);
  (void)dp_async_dsss_pool_push (p, x, POOL_GROUP / 2 + 17);
  DP_REQUIRE (p->carry_len == POOL_GROUP / 2 + 17);
  dp_async_dsss_pool_reset (p);
  const size_t   bytes = dp_async_dsss_pool_state_bytes (p);
  unsigned char *a     = malloc (bytes);
  DP_REQUIRE (a && bytes == dp_async_dsss_pool_state_bytes (q));
  dp_async_dsss_pool_get_state (p, a);
  DP_CHECK (dp_async_dsss_pool_set_state (q, a) == DP_OK);
  free (a);
  dp_async_dsss_pool_destroy (p);
  dp_async_dsss_pool_destroy (q);
  free (x);
  return 0;
}

/* reset() hands back every receiver idle and the search with no
   exclusions, and the clock and counts at 0; set_threads refuses a
   negative count unchanged. Sabotage: leave the search's exclusions -> a
   reset pool still skips the old phases. */
static int
_test_reset_and_threads (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  static float _Complex z[POOL_GROUP];
  dp_async_dsss_pool_state_t *p = _pool (code, 3, 0.0, 1);
  DP_REQUIRE (p != NULL);
  DP_REQUIRE (dp_async_dsss_receiver_seed (p->rx[0], 10.0, 0.0, 45.0)
              == DP_OK);
  DP_REQUIRE (dp_async_dsss_receiver_seed (p->rx[1], 10.2, 0.0, 45.0)
              == DP_OK);
  (void)dp_async_dsss_pool_push (p, z, POOL_GROUP);
  DP_REQUIRE (p->released_duplicate == 1 && p->samples == POOL_GROUP);
  DP_REQUIRE (dp_async_dsss_receiver_get_n_exclusions (p->search) == 1);
  dp_async_dsss_pool_reset (p);
  DP_CHECK (p->samples == 0 && p->released_duplicate == 0
            && p->released_lost == 0);
  for (size_t i = 0; i < 3; i++)
    {
      DP_CHECK (dp_async_dsss_pool_status (p, i).state == ASYNC_DSSS_RX_IDLE);
      DP_CHECK (dp_async_dsss_receiver_get_n_exclusions (p->rx[i]) == 0);
      DP_CHECK (dp_async_dsss_receiver_get_rank (p->rx[i]) == 0);
    }
  DP_CHECK (dp_async_dsss_receiver_get_n_exclusions (p->search) == 0);
  DP_CHECK (!p->searching && p->search_periods == 0);
  DP_CHECK (dp_async_dsss_pool_set_threads (p, -1) == DP_ERR_INVALID);
  DP_CHECK (p->threads == 1);
  DP_CHECK (dp_async_dsss_pool_set_threads (p, 2) == DP_OK);
  DP_CHECK (dp_async_dsss_pool_get_threads (p) == 2);
  dp_async_dsss_pool_destroy (p);
  return 0;
}

static void _ring (dp_async_dsss_pool_state_t *p, size_t r, double f0,
                   double slope, double jit, size_t locked);

/* The header's reset, restore and read-back edges (#3267, P31, P33, P37,
   P38, P39):
   - reset() zeroes the ghost history and its release count, not only the
     clock and the duplicate and loss counts;
   - set_state() refuses a carry of a whole group or more, and leaves the
     pool as it was;
   - status() past the pool is a zeroed record, not only its state;
   - symbols_max_out() is 0 before the first push and after reset(), and
     is the largest of the receivers' counts;
   - a push replaces the symbols: the pool holds what its last push
     emitted, which a copy restored from the state before that push
     emits on the same samples (set_state clears them, and the resume is
     bit-for-bit); reset() clears them.
   Sabotage, one per claim: drop reset's released_ghost = 0, or its
   hist_len memset -> red; drop set_state's carry check -> the forged blob
   restores; give status' none record a nonzero field -> red; let push
   append to n_sym -> the original holds more than the copy. */
static int
_test_reset_and_restore_edges (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t                      n;
  float _Complex             *x = _two_emitters (code, &n);
  dp_async_dsss_pool_state_t *p = _pool (code, 3, 0.0, 1);
  dp_async_dsss_pool_state_t *q = _pool (code, 3, 0.0, 1);
  DP_REQUIRE (p && q && x && n > 4 * POOL_GROUP);
  DP_CHECK_MSG (dp_async_dsss_pool_symbols_max_out (p) == 0,
                "no symbols before the first push");

  /* status() past the pool: every field but the state is zero. Field by
     field, not memcmp: the record has padding after `state`, whose bytes
     are unspecified. */
  {
    const async_dsss_receiver_status_t z = dp_async_dsss_pool_status (p, 3);
    DP_CHECK_MSG (z.state == ASYNC_DSSS_POOL_NO_RECEIVER && z.doppler_hz == 0.0
                      && z.chip_phase == 0.0 && z.code_rate == 0.0
                      && z.cn0_dbhz_est == 0.0 && z.code_locked == 0
                      && z.locked == 0 && z.lock_metric == 0.0
                      && z.lock_threshold == 0.0 && z.car_last_error == 0.0
                      && z.mpsk_last_error == 0.0 && z.state_samples == 0
                      && z.both_down_samples == 0 && z.refused_hits == 0
                      && z.carrier_lock_n_noncoh == 0,
                  "status past the pool is a zeroed record");
  }

  /* A push replaces the symbols: q resumes from p's state before the
     last push and emits on the same samples what p holds after it. */
  const size_t first = n / 2 - (n / 2) % POOL_GROUP;
  (void)dp_async_dsss_pool_push (p, x, first);
  const size_t   bytes = dp_async_dsss_pool_state_bytes (p);
  unsigned char *blob  = malloc (bytes);
  DP_REQUIRE (blob != NULL);
  dp_async_dsss_pool_get_state (p, blob);
  DP_REQUIRE (dp_async_dsss_pool_set_state (q, blob) == DP_OK);
  (void)dp_async_dsss_pool_push (p, x + first, POOL_GROUP);
  (void)dp_async_dsss_pool_push (q, x + first, POOL_GROUP);
  size_t most = 0;
  for (size_t i = 0; i < 3; i++)
    {
      const size_t np = p->n_sym[i], nq = q->n_sym[i];
      if (np != nq)
        DP_TEST_ERR ("receiver %zu: %zu held against %zu emitted\n", i, np,
                     nq);
      DP_CHECK_MSG (np == nq, "a push replaces the symbols");
      most = np > most ? np : most;
    }
  DP_REQUIRE_MSG (most > 0, "the scene emits symbols in the push");
  DP_CHECK (dp_async_dsss_pool_symbols_max_out (p) == most);
  /* Largest of: one receiver's count raised past the others reads back. */
  p->n_sym[2] = most + 1;
  DP_CHECK (dp_async_dsss_pool_symbols_max_out (p) == most + 1);
  p->n_sym[2] = q->n_sym[2];

  /* set_state() refuses a carry of a group or more, unchanged. The carry
     length is the sixth field of the pool's extra, after the header. */
  {
    unsigned char *forged = malloc (bytes), *before = malloc (bytes),
                  *after = malloc (bytes);
    DP_REQUIRE (forged && before && after);
    dp_async_dsss_pool_get_state (p, forged);
    dp_async_dsss_pool_get_state (q, before);
    const uint64_t group = (uint64_t)p->group;
    memcpy (forged + sizeof (dp_state_hdr_t) + 5 * sizeof (uint64_t), &group,
            sizeof group);
    DP_CHECK_MSG (dp_async_dsss_pool_set_state (q, forged) == DP_ERR_INVALID,
                  "a carry of a whole group is refused");
    dp_async_dsss_pool_get_state (q, after);
    DP_CHECK (memcmp (before, after, bytes) == 0);
    free (forged);
    free (before);
    free (after);
  }

  /* reset() zeroes the ghost history and its count. */
  _ring (p, 0, 100.0, 0.0, 0.0, 0);
  _ring (p, 1, 1600.0, 0.0, 0.0, 0);
  p->hist_pos       = 7;
  p->released_ghost = 3;
  dp_async_dsss_pool_reset (p);
  DP_CHECK (p->released_ghost == 0 && p->hist_pos == 0);
  for (size_t i = 0; i < 3; i++)
    {
      DP_CHECK_MSG (p->hist_len[i] == 0, "every ring emptied");
      DP_CHECK (dp_async_dsss_pool_symbols (p, i, x, 1) == 0);
    }
  int zero = 1;
  for (size_t c = 0; c < 3 * p->ghost_window; c++)
    zero &= p->car_hist[c] == 0.0 && p->cl_hist[c] == 0;
  DP_CHECK_MSG (zero, "every ring cell zeroed");
  DP_CHECK (dp_async_dsss_pool_symbols_max_out (p) == 0);
  free (blob);
  dp_async_dsss_pool_destroy (p);
  dp_async_dsss_pool_destroy (q);
  free (x);
  return 0;
}

/* Chunk invariance (dp_chunk_inv.h): the controller acts at group
   positions of the stream (ASYNC_DSSS_POOL_PERIODS_PER_JOIN code periods,
   on the grid counted from create), so how the stream is cut into pushes
   changes nothing. One spec per holder, each comparing that receiver's
   symbols, on the two-emitter stream through the hand-offs, the duplicate
   checks and tracking (the third receiver searches throughout and emits none:
   its share is the holders' exclusions, which the symbols reflect). Sabotage:
   run the controller once per push instead of on the grid -> the partitions
   disagree. */
typedef struct
{
  const uint8_t *code;
  size_t         receiver;
} ci_pool_arg_t;

static void *
ci_pool_create (void *arg)
{
  const ci_pool_arg_t *a = arg;
  return _pool (a->code, 3, CI_CONFIRM_S, 1);
}

static void
ci_pool_destroy (void *obj)
{
  dp_async_dsss_pool_destroy (obj);
}

static ci_pool_arg_t *ci_pool_cur;

static size_t
ci_pool_push (void *obj, const void *in, size_t n, void *out, size_t cap)
{
  (void)dp_async_dsss_pool_push (obj, in, n);
  return dp_async_dsss_pool_symbols (obj, ci_pool_cur->receiver, out, cap);
}

static int
_test_chunk_invariance (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t          n;
  float _Complex *x = _two_emitters_off (code, CI_B_OFF, &n);
  for (size_t r = 0; r < 2; r++)
    {
      ci_pool_arg_t arg = { code, r };
      ci_pool_cur       = &arg;
      static const char *names[2]
          = { "AsyncDsssPool rx0", "AsyncDsssPool rx1" };
      dp_ci_spec_t spec = {
        .name     = names[r],
        .create   = ci_pool_create,
        .destroy  = ci_pool_destroy,
        .process  = ci_pool_push,
        .arg      = &arg,
        .in_size  = sizeof (float _Complex),
        .out_size = sizeof (float _Complex),
        .out_cap  = n / 64,
        .frame_n  = 1023 * POOL_SPC,
      };
      DP_CHECK (dp_chunk_invariance (&spec, x, n) == 0);
    }
  /* And the run reached what it is there to cover: both emitters held,
     then B's holder released after B left. */
  dp_async_dsss_pool_state_t *p = _pool (code, 3, CI_CONFIRM_S, 1);
  DP_REQUIRE (p != NULL);
  DP_CHECK (dp_async_dsss_pool_push (p, x, CI_B_OFF) == 2);
  (void)dp_async_dsss_pool_push (p, x + CI_B_OFF, n - CI_B_OFF);
  DP_CHECK_MSG (p->released_lost >= 1, "the release fired in the stream");
  /* The shared round trip, on a pool that has released. */
  dp_async_dsss_pool_state_t *q = _pool (code, 3, CI_CONFIRM_S, 1);
  DP_REQUIRE (q != NULL);
  DP_STATE_ROUNDTRIP_TEST (dp_async_dsss_pool, p, q);
  dp_async_dsss_pool_destroy (p);
  dp_async_dsss_pool_destroy (q);
  free (x);
  return 0;
}

/* Write a full window into receiver r's ring: carrier f0 + slope*b plus a
   +/-jit square wave (a standard deviation of exactly jit over an even
   window), and code lock on the first `locked` steps. */
static void
_ring (dp_async_dsss_pool_state_t *p, size_t r, double f0, double slope,
       double jit, size_t locked)
{
  const size_t w = p->ghost_window;
  for (size_t b = 0; b < w; b++)
    {
      p->car_hist[r * w + b] = f0 + slope * (double)b + (b & 1 ? jit : -jit);
      p->cl_hist[r * w + b]  = b < locked;
    }
  p->hist_len[r] = w;
}

/* The ghost test, each clause at its edge, on rings written by hand (the
   test reads nothing else). The window is 0.2 s of code periods (600 at
   3 kHz), the lattice chip_rate / (2 code_len) (1500 Hz). Receiver 0 is
   the parent, a ramp of 300 Hz/s (0.1 Hz a period) every row shares, so
   only the delta matters. Sabotage, one per clause: drop k != 0 -> the
   co-located row fires; drop the eps test -> the off-lattice row fires;
   drop the stillness test -> the jittery row fires; drop the code-lock
   test -> the locked row fires; drop the full-window check -> the short
   row fires. */
static int
_test_ghost_test_edges (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  dp_async_dsss_pool_state_t *p = _pool (code, 3, 0.0, 1);
  DP_REQUIRE (p != NULL);
  const size_t w = p->ghost_window;
  DP_REQUIRE (w == 600 && fabs (p->lattice_hz - 1500.0) < 1e-9);
  const double eps = ASYNC_DSSS_POOL_GHOST_EPS_HZ;
  const double sd  = ASYNC_DSSS_POOL_GHOST_STILL_HZ;
  /* The most locked steps a ghost may hold: strictly below the fraction. */
  const size_t lk = (size_t)ceil (ASYNC_DSSS_POOL_GHOST_LOCK_FRAC * w) - 1;
  /* delta = k lattice steps plus eps_frac of eps; jitter jit_frac of
     the stillness limit; lock_extra more locked steps than a ghost may
     hold. */
  static const struct
  {
    int         k;
    double      eps_frac, jit_frac;
    int         lock_extra, short_ring, ghost;
    const char *what;
  } rows[] = {
    { 1, 0.3, 0.5, 0, 0, 1, "on the lattice, still, unlocked" },
    { -2, 0.3, 0.5, 0, 0, 1, "k = -2" },
    { 0, 0.3, 0.5, 0, 0, 0, "k = 0: co-located, the duplicate rule's" },
    { 1, 0.99, 0.5, 0, 0, 1, "inside eps" },
    { 1, 1.01, 0.5, 0, 0, 0, "outside eps" },
    { 1, -1.01, 0.5, 0, 0, 0, "outside eps, below" },
    { 1, 0.0, 0.99, 0, 0, 1, "inside the stillness limit" },
    { 1, 0.0, 1.01, 0, 0, 0, "outside the stillness limit" },
    { 1, 0.0, 0.5, 1, 0, 0, "code lock at the fraction" },
    { 1, 0.0, 0.5, 0, 1, 0, "a window not yet full" },
  };
  for (size_t k = 0; k < sizeof rows / sizeof rows[0]; k++)
    {
      const double delta = rows[k].k * p->lattice_hz + rows[k].eps_frac * eps;
      _ring (p, 0, 200.0, 0.1, 0.0, w);
      _ring (p, 1, 200.0 + delta, 0.1, rows[k].jit_frac * sd,
             lk + (size_t)rows[k].lock_extra);
      if (rows[k].short_ring)
        p->hist_len[1] = w - 1;
      DP_CHECK_MSG (dp_async_dsss_pool_ghost_of (p, 0, 1) == rows[k].ghost,
                    rows[k].what);
    }
  /* The parent is not the ghost's ghost (it holds code lock), an index
     past the pool or equal to the other is no ghost. */
  _ring (p, 1, 1700.0, 0.1, 0.0, 0);
  DP_CHECK (dp_async_dsss_pool_ghost_of (p, 0, 1) == 1);
  DP_CHECK (dp_async_dsss_pool_ghost_of (p, 1, 0) == 0);
  DP_CHECK (dp_async_dsss_pool_ghost_of (p, 0, 3) == 0);
  DP_CHECK (dp_async_dsss_pool_ghost_of (p, 1, 1) == 0);
  dp_async_dsss_pool_destroy (p);
  return 0;
}

/* A pool for the ghost scenes: two receivers searching +/-5 kHz (several
   3 kHz tiles, so a sidelobe a tile off is in range) at design 47 dB-Hz,
   never lost, so only the controller releases. */
static dp_async_dsss_pool_state_t *
_ghost_pool (const uint8_t *code)
{
  return dp_async_dsss_pool_create (
      code, 1023, 2, POOL_CR, POOL_RS, POOL_CR * (double)POOL_SPC, 2, 47.0,
      1e-3, 0.9, 5000.0, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0,
      false, 0.0, 0.0, 1, true);
}

/* The ghost scenes at the 415.1-B-1 point (async-dsss-receiver.md §8): 11
   MSa/s in at 2.8 GHz, 4.0913 Mcps, so 2.69 samples a chip -- never a whole
   number -- 3000 sym/s, the 275-symbol code-only window of every
   1000-symbol frame, rendered by the shipped generator at two samples a
   chip, the shipped RateConverter to 11 MSa/s, the shipped DopplerChannel
   and AWGN. A alone at 60 dB-Hz on a static carrier; a pool of two at the
   spec configuration (design 4 dB Es/N0, the code-only window and 400 Hz/s
   bounding the search), its loss release at `lost_s` (0: never). Once
   receiver 0 tracks A with code lock, receiver 1 is seeded on A's code
   sidelobe G415_SIDE chips away, `k` lattice steps off A's carrier, with
   the C/N0 the search would report for that peak (A's times the sidelobe's
   level): the seed the search makes when noise lifts a sidelobe past its
   gate. Pushed until receiver 1 is released or `tmax_s` after the seed. */
#define G415_PUSH 4096u
#define G415_FC DP_CCSDS_415_FC_HI
typedef struct
{
  dp_async_dsss_pool_state_t *p;
  double                      df_end;  /* receiver 1's carrier minus 0's */
  int                         tracked; /* receiver 1 reached tracking     */
  long n, code_up, sym_up, both_down;  /* its tracking steps and flags   */
} _g415_t;

/* The sidelobe the ghost scenes seed on: 167 chips from the code's peak,
   the offset the pool's own search took on A's sidelobe at 70 dB-Hz at this
   point (async-dsss-receiver.md §8). Its circular autocorrelation, of the
   peak's 1023: a Gold code's sidelobes take two magnitudes, 63 and 65,
   about -24 dB, and 167 is at the in-phase one. */
#define G415_SIDE 167u
static long
_autocorr (const uint8_t *code, size_t t)
{
  long r = 0;
  for (size_t i = 0; i < 1023; i++)
    r += (code[i] == code[(i + t) % 1023]) ? 1 : -1;
  return r;
}

static dp_async_dsss_pool_state_t *
_g415_pool (const uint8_t *code, double lost_s, bool refine)
{
  return dp_s415_pool_refine (code, G415_FC, 2, lost_s, 1, refine);
}

/* The ghost's seed C/N0 is A's plus ghost_db; NAN seeds it at the
   sidelobe's own level, as the search would (every scene but the order
   test's, which seeds it stronger than A). */
static int
_g415_scene_seeded (const uint8_t *code, uint32_t seed, double cn0, int k,
                    double lost_s, double tmax_s, bool refine, double ghost_db,
                    _g415_t *out)
{
  const double f0 = 700.0;
  const long   r  = _autocorr (code, G415_SIDE);
  DP_REQUIRE_MSG (r == 63, "167 chips is a peak-level in-phase sidelobe");
  const size_t                side  = G415_SIDE;
  const double                level = 20.0 * log10 ((double)r / 1023.0);
  dp_async_dsss_pool_state_t *p     = _g415_pool (code, lost_s, refine);
  DP_REQUIRE (p != NULL);
  /* A on the air at the 415.1 point (dp_air_test.h): the shipped synth,
     the front end's fixed-ratio converter, a Doppler channel 700 Hz off,
     and the shipped noise at A's own C/N0. */
  dp_air_emitter_t e
      = dp_s415_emitter (code, G415_FC, f0, 0.0, cn0, 0.0, 100u + seed);
  /* Each push completes the pool's current group (group minus its carry),
     so the controller runs at the end of every push and a release is seen
     before any later period records into the rings: hist_clear is the only
     thing that can have emptied one. */
  const size_t    push = p->group;
  dp_air_t        air;
  float _Complex *ns = malloc (push * sizeof *ns);
  DP_REQUIRE (
      ns != NULL
      && dp_air_init (&air, &e, 1, DP_CCSDS_415_FS, cn0, 7919u + seed, push));
  const double lattice = p->lattice_hz;
  size_t       pos     = 0;
  double       t_seed  = -1.0;
  out->tracked         = 0;
  out->df_end          = NAN;
  out->n = out->code_up = out->sym_up = out->both_down = 0;
  for (;;)
    {
      const size_t want = push - p->carry_len;
      DP_REQUIRE (dp_air_next (&air, ns, want));
      (void)dp_async_dsss_pool_push (p, ns, want);
      DP_REQUIRE (p->carry_len == 0);
      pos += want;
      const double                       t  = (double)pos / DP_CCSDS_415_FS;
      const async_dsss_receiver_status_t s0 = dp_async_dsss_pool_status (p, 0);
      const async_dsss_receiver_status_t s1 = dp_async_dsss_pool_status (p, 1);
      if (t_seed < 0.0)
        {
          if (t >= 0.5 && s0.state == ASYNC_DSSS_RX_TRACKING && s0.code_locked
              && s1.state == ASYNC_DSSS_RX_IDLE)
            {
              DP_REQUIRE (dp_async_dsss_receiver_seed (
                              p->rx[1],
                              fmod (s0.chip_phase + (double)side, 1023.0),
                              f0 + k * lattice,
                              cn0 + (isnan (ghost_db) ? level : ghost_db))
                          == DP_OK);
              t_seed = t;
            }
          DP_REQUIRE_MSG (t < 3.0, "receiver 0 tracks A with code lock");
          continue;
        }
      if (s1.state == ASYNC_DSSS_RX_TRACKING)
        {
          out->tracked = 1;
          out->df_end  = s1.doppler_hz - s0.doppler_hz;
          out->n++;
          out->code_up += s1.code_locked != 0;
          out->sym_up += s1.locked != 0;
          out->both_down += !s1.code_locked && !s1.locked;
        }
      if (s1.state == ASYNC_DSSS_RX_IDLE || t - t_seed > tmax_s)
        break;
    }
  free (ns);
  dp_air_free (&air);
  out->p = p;
  return 0;
}

static int
_g415_scene (const uint8_t *code, uint32_t seed, double cn0, int k,
             double lost_s, double tmax_s, bool refine, _g415_t *out)
{
  return _g415_scene_seeded (code, seed, cn0, k, lost_s, tmax_s, refine, NAN,
                             out);
}

/* The ghost rule's parent is the STRONGER holder by seed C/N0 (#3267, P14):
   ghost_of() is order-blind, so only the controller's order says which of
   a pair goes. The k = 2 scene of _test_ghost_is_released, the loss release
   and the refine off, so only the ghost rule can release, with the ghost
   seeded 10 dB OVER A instead of at its sidelobe's level: no holder is
   stronger than it, so the ghost rule takes nothing, and it holds to the
   scene's end. A itself is no ghost (its code lock is up), so it stays
   too. The release at the sidelobe's level is _test_ghost_is_released.
   Sabotage: drop the controller's strength comparison -> the ghost is
   released anyway. */
static int
_test_ghost_parent_is_the_stronger (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  _g415_t z;
  DP_REQUIRE (_g415_scene_seeded (code, 2, 60.0, 2, 0.0, 4.0, false, 10.0, &z)
              == 0);
  dp_async_dsss_pool_state_t *p = z.p;
  DP_REQUIRE_MSG (z.tracked, "the ghost reached tracking");
  DP_CHECK_MSG (p->released_ghost == 0,
                "a ghost stronger than every holder is no one's ghost");
  /* And by the ghost test itself it IS A's ghost (its code lock low, its
     carrier a whole k of spacings off), so only the strength order kept
     it. Read only while it is held: a released ghost's ring is empty. */
  if (p->released_ghost == 0)
    DP_REQUIRE_MSG (dp_async_dsss_pool_ghost_of (p, 0, 1) == 1,
                    "by the ghost test, receiver 1 is receiver 0's ghost");
  DP_CHECK (dp_async_dsss_pool_status (p, 0).state == ASYNC_DSSS_RX_TRACKING);
  DP_CHECK (dp_async_dsss_pool_status (p, 1).state == ASYNC_DSSS_RX_TRACKING);
  dp_async_dsss_pool_destroy (p);
  return 0;
}

/* The ghost rule at 415.1 (§8): a sidelobe ghost two lattice steps off A's
   carrier, its code lock low, is released by the ghost rule (the loss
   release off, so nothing else can); A is kept, and the released
   receiver's ring is emptied, which the state checks below rest on. A
   ghost on A's own carrier (k = 0) is NOT taken by the ghost rule -- a
   co-located true emitter looks the same to it (§8, doppler#2551). With A
   at 55 dB-Hz it holds neither flag and the loss release frees it
   (lost_confirm_s 0.5 s). The 60 dB-Hz k = 0 case under the shipped
   refine is _test_shipped_pool_frees_a_dropped_ghost: the fine hand-over
   carries the k = 2 seed to A's carrier, where the loss release frees a
   ghost that drops both flags; one that keeps a flag up is held, a rate
   over the validator (doppler#3263).
   Sabotage: drop the ghost pass -> the k = 2 ghost holds to the end; let
   the carrier test take k = 0 -> the 55 dB-Hz one leaves as a ghost; drop the
   pool's reset of a lost receiver -> the 55 dB-Hz one is never freed. The
   freed receiver reads idle after its release step: the search was not
   fed while both held, so it has no pick to seed it with then. Whether a
   later dwell seeds the ghost's cell again is a rate over the sidelobe
   rule (doppler#3263), not pinned here. */
static int
_test_ghost_is_released (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  {
    /* (2b) A weaker parent (55 dB-Hz): the k = 0 seed holds neither flag,
       and the loss release frees it. */
    _g415_t z;
    DP_REQUIRE (_g415_scene (code, 1, 55.0, 0, 0.5, 4.0, true, &z) == 0);
    dp_async_dsss_pool_state_t *p = z.p;
    DP_REQUIRE_MSG (z.tracked && z.n > 0 && z.both_down == z.n,
                    "a k = 0 ghost with both flags down");
    DP_CHECK_MSG (p->released_ghost == 0 && p->released_lost == 1,
                  "the loss release frees it");
    dp_async_dsss_pool_destroy (p);
  }
  /* The refine is OFF here, and only that keeps the seed on its lattice
     image: the shipped pool refines, and its fine hand-over takes this
     seed to A's own carrier (_test_shipped_pool_frees_a_dropped_ghost,
     doppler#3263). This is the ghost rule's pin, not the shipped
     configuration. */
  _g415_t z;
  DP_REQUIRE (_g415_scene (code, 2, 60.0, 2, 0.0, 4.0, false, &z) == 0);
  dp_async_dsss_pool_state_t *p = z.p;
  DP_CHECK_MSG (z.tracked, "the ghost reached tracking");
  printf ("k = 2 ghost: carrier %.1f Hz off A's (lattice %.1f Hz)\n", z.df_end,
          p->lattice_hz);
  DP_CHECK_MSG (p->released_ghost == 1, "the ghost was released");
  DP_CHECK (p->released_duplicate == 0);
  DP_CHECK (p->released_lost == 0);
  DP_CHECK (dp_async_dsss_pool_status (p, 0).state == ASYNC_DSSS_RX_TRACKING);
  DP_CHECK (dp_async_dsss_pool_status (p, 1).state == ASYNC_DSSS_RX_IDLE);
  DP_CHECK (p->hist_len[1] == 0);
  DP_REQUIRE_MSG (p->hist_len[0] == p->ghost_window,
                  "receiver 0's ring is full: the forgeries below rest on it");
  /* And a pool that has released one round-trips. */
  dp_async_dsss_pool_state_t *q = _g415_pool (code, 0.0, false);
  DP_REQUIRE (q != NULL);
  DP_STATE_ROUNDTRIP_TEST (dp_async_dsss_pool, p, q);
  /* Its blob holds only what control() leaves: receiver 0's full ring and
     receiver 1's empty one. Each forgery below breaks one rule set_state
     checks, and is refused. Sabotage: drop any one check -> its row
     restores. Layout: header, the 10 extra fields (hist_pos the eighth,
     then the search's two), the carry (a group:
     ASYNC_DSSS_POOL_PERIODS_PER_JOIN periods, not one), then the lengths, the
     carrier rings and the code-lock rings. */
  {
    const size_t w     = p->ghost_window,
                 bytes = dp_async_dsss_pool_state_bytes (p);
    const size_t o_pos = sizeof (dp_state_hdr_t) + 7 * sizeof (uint64_t);
    const size_t o_len
        = o_pos + 3 * sizeof (uint64_t) + p->group * sizeof (float _Complex);
    const size_t   o_car = o_len + 2 * sizeof (uint64_t);
    const size_t   o_cl  = o_car + 2 * w * sizeof (double);
    unsigned char *blob = malloc (bytes), *forged = malloc (bytes);
    DP_REQUIRE (blob && forged);
    dp_async_dsss_pool_get_state (p, blob);
    DP_REQUIRE (dp_async_dsss_pool_set_state (q, blob) == DP_OK);
    const uint64_t     past = w, over = (uint64_t)w + 1, one = 1;
    const double       tone = 1.0;
    const uint8_t      two  = 2;
    static const char *what[]
        = { "a length past the window", "a ring on a receiver not tracking",
            "an entry outside a ring's span", "a code-lock flag of 2",
            "a head past the window" };
    for (int k = 0; k < 5; k++)
      {
        memcpy (forged, blob, bytes);
        if (k == 0)
          memcpy (forged + o_len, &over, sizeof over);
        if (k == 1)
          memcpy (forged + o_len + sizeof (uint64_t), &one, sizeof one);
        if (k == 2)
          memcpy (forged + o_car + w * sizeof (double), &tone, sizeof tone);
        if (k == 3)
          memcpy (forged + o_cl, &two, 1);
        if (k == 4)
          memcpy (forged + o_pos, &past, sizeof past);
        DP_CHECK_MSG (dp_async_dsss_pool_set_state (q, forged)
                          == DP_ERR_INVALID,
                      what[k]);
      }
    free (blob);
    free (forged);
  }
  dp_async_dsss_pool_destroy (q);
  dp_async_dsss_pool_destroy (p);
  return 0;
}

/* A (70 dB-Hz at 200 Hz) plus a distinct emitter B at `cn0_b`, `df_b` from
   A, its code `delay` samples behind A's, both with data; receiver 0 takes
   A from the search and receiver 1 is seeded on B's true cell once it
   does. Over the run: whether nothing was released, both were held to the
   end, and whether, at some step with both windows full, the carrier test
   alone (B's code-lock ring cleared) or the code-lock test alone (B's
   carrier ring moved onto A's lattice) would have released B. */
typedef struct
{
  int kept, carrier_alone, lock_alone;
} _pair_t;

static _pair_t
_distinct_pair (const uint8_t *code, double cn0_b, double df_b, size_t delay)
{
  _pair_t         r = { 0, 0, 0 };
  float _Complex *a, *b;
  size_t          na, nb;
  double         *da, *db;
  const double    tsym = POOL_FS / POOL_RS;
  dp_dsss_capture (code, 1023, POOL_SPC, POOL_FS, tsym, 200.0, 70.0, 2700, 0,
                   21, &a, &na, &da);
  /* B clean (300 dB-Hz), then scaled to cn0_b against A's noise. */
  dp_dsss_capture (code, 1023, POOL_SPC, POOL_FS, tsym, 200.0 + df_b, 300.0,
                   2700, delay, 22, &b, &nb, &db);
  const size_t n = na < nb ? na : nb;
  const float  g = (float)pow (10.0, (cn0_b - 70.0) / 20.0);
  for (size_t i = 0; i < n; i++)
    a[i] += g * b[i];
  dp_async_dsss_pool_state_t *p = _ghost_pool (code);
  if (!p || p->ghost_window > 1024) /* the scratch rings below */
    return r;
  const size_t te = 1023 * POOL_SPC, w = p->ghost_window;
  /* B's code phase: its delay in chips, behind A's 0. */
  const double ph_b   = fmod (1023.0 - (double)delay / POOL_SPC, 1023.0);
  int          seeded = 0;
  for (size_t pos = 0; pos + te <= n; pos += te)
    {
      (void)dp_async_dsss_pool_push (p, a + pos, te);
      if (!seeded && p->rx[0]->state == ASYNC_DSSS_RX_TRACKING)
        seeded
            = dp_async_dsss_receiver_seed (p->rx[1], ph_b, 200.0 + df_b, cn0_b)
              == DP_OK;
      if (p->hist_len[0] < w || p->hist_len[1] < w)
        continue;
      uint8_t cl[1024];
      double  car[1024];
      memcpy (cl, p->cl_hist + w, w);
      memset (p->cl_hist + w, 0, w);
      r.carrier_alone |= dp_async_dsss_pool_ghost_of (p, 0, 1);
      memcpy (p->cl_hist + w, cl, w);
      memcpy (car, p->car_hist + w, w * sizeof *car);
      for (size_t k = 0; k < w; k++)
        p->car_hist[w + k] = p->car_hist[k] + p->lattice_hz;
      r.lock_alone |= dp_async_dsss_pool_ghost_of (p, 0, 1);
      memcpy (p->car_hist + w, car, w * sizeof *car);
    }
  r.kept = p->released_ghost == 0 && p->released_duplicate == 0
           && dp_async_dsss_pool_status (p, 0).state == ASYNC_DSSS_RX_TRACKING
           && dp_async_dsss_pool_status (p, 1).state == ASYNC_DSSS_RX_TRACKING;
  dp_async_dsss_pool_destroy (p);
  free (a);
  free (b);
  free (da);
  free (db);
  return r;
}

/* Neither test alone releases: each guards a true track the other would
   take. A co-moving emitter one lattice step from A (55 dB-Hz) passes the
   carrier test and is kept by its code lock; a weaker one on its own
   carrier (46 dB-Hz, just handed off, its code lock still low) passes the
   code-lock test and is kept by its carrier. The controller reads the
   rings once a group (ASYNC_DSSS_POOL_PERIODS_PER_JOIN periods), so this
   probe asks at 1/16 of the steps a per-period controller did, and the
   weak one must still be low at one of them: at 16 periods a group it is
   on 10 of 10 noise draws up to 46 dB-Hz and 9 of 10 at 47 (10 of 10 to
   51 at one period a step), so it sits at 46, the highest with no draw
   missing (it was 50 at one period a step). Measured on identical streams, M =
   1 against 16: the weak one is kept on all 90 draws from 44 to 52 dB-Hz in
   both, and a real ghost (_test_ghost_is_released's scene, parents at 60, 65
   and 70 dB-Hz, 10 draws each) is released on the same counts (8, 9, 8 of 10),
   a median 3 to 8 ms later at 16. Sabotage: drop the code-lock
   test -> the co-moving emitter is released; drop the carrier test -> the
   weak one is. */
static int
_test_ghost_needs_both_tests (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const _pair_t comove = _distinct_pair (code, 55.0, 1500.0, 2 * 259);
  DP_CHECK_MSG (comove.kept, "a co-moving emitter is kept");
  DP_CHECK_MSG (comove.carrier_alone, "the carrier test alone would fire");
  DP_CHECK (!comove.lock_alone);
  const _pair_t weak = _distinct_pair (code, 46.0, -437.0, 2 * 198);
  DP_CHECK_MSG (weak.kept, "a weak emitter on its own carrier is kept");
  DP_CHECK_MSG (weak.lock_alone, "the code-lock test alone would fire");
  DP_CHECK (!weak.carrier_alone);
  return 0;
}

/* A version-2 AccTrace blob, nested three deep in each receiver, is
   refused through the pool (#2094): pool -> receiver -> CarrierAcquisition
   -> PSD -> AccTrace. A version-2 trace held the mean, which version 3
   reads as a sum. The pool restores whole or nothing, so the refusal
   leaves it exactly as advanced, though its own fields and receiver 0's
   children decode first. One trace in each of the three receivers and in
   the search-only one (the refine's estimator), so four hits. */
static int
_test_nested_trace_version_refused (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t                      n;
  float _Complex             *x = _two_emitters (code, &n);
  dp_async_dsss_pool_state_t *p = _pool (code, 3, 0.0, 1);
  DP_REQUIRE (p != NULL && x != NULL);
  (void)dp_async_dsss_pool_push (p, x, 4096);
  DP_STATE_NESTED_VERSION_TEST (
      dp_async_dsss_pool, p, (void)dp_async_dsss_pool_push (p, x + 4096, 4096),
      ACC_TRACE_STATE_MAGIC, 2u, 4, 1);
  dp_async_dsss_pool_destroy (p);
  free (x);
  return 0;
}

/* The pool hands `refine` to every receiver it builds, the search's
 * included, and so refuses what the receiver refuses: refine false under a
 * search ASYNC_DSSS_RX_NO_REFINE_MIN_BINS deep. CODE7 at 1 Mchip/s and 3
 * samples a chip: no code-only window is D = 1, and 2 ms is D = 140.
 *
 * Sabotages: the pool passing true whatever it is given -> the D = 1 row
 * builds and the receivers read refine true, red; passing false -> the
 * refine-true rows red. */
static int
_test_refine_passes_through (void)
{
  static const double windows[] = { 0.0, 0.002 };
  for (size_t w = 0; w < 2; w++)
    for (int refine = 0; refine <= 1; refine++)
      {
        dp_async_dsss_pool_state_t *p = dp_async_dsss_pool_create (
            CODE7, 7, 3, 1.0e6, 35714.29, 3.0e6, 2, 58.0, 1e-2, 0.9, 500.0, 4,
            8, 0, 0.5, 4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false,
            windows[w], 0.0, 1, refine);
        const int deep = windows[w] > 0.0;
        DP_CHECK ((p != NULL) == (refine || deep));
        if (!p)
          continue;
        DP_CHECK (p->search->refine == (bool)refine);
        for (size_t i = 0; i < p->n_receivers; i++)
          DP_CHECK (p->rx[i]->refine == (bool)refine);
        dp_async_dsss_pool_destroy (p);
      }
  return 0;
}

/* The k = 2 scene under the shipped pool: the release rules as shipped
   (lost_confirm_s at objects/async_dsss_pool.toml's default, 2.0 s; the
   duplicate and ghost rules always on), run to the lock allocation
   (DP_CCSDS_415_LOCK_S) at 2.8 GHz. The fine hand-over takes the seed two
   lattice steps off A to A's own carrier, where the ghost rule leaves it
   (a k = 0 ghost and a co-located true emitter look the same to it, §8,
   doppler#2551). What frees it then is the loss release: the ghost, on a
   -24 dB sidelobe of a 60 dB-Hz A, frees when both its flags stay down for
   lost_confirm_s. Asserted on seed 2, a draw whose ghost drops both flags
   early (620 tracking steps, 511 of them both down): the loss release
   frees it inside the allocation, and nothing else releases it. Seed 2
   reads that way under gcc and clang Release, ASan and the coverage build.
   Sabotage: lost_confirm_s past the allocation -> seed 2 is held, red.

   Not asserted: that a ghost which keeps a flag up is HELD. That is the
   declared near-far limit, and it is a rate, not a draw: the record's
   FORCED arm on this same scene holds 5 of 20 at k = 2, 2.8 GHz
   (docs/design/async-dsss-receiver-measurements/final-9ce208250/
   ghosts_forced.csv, seeds 3, 9, 10, 12 and 13). Which draws are held
   moves with the tree and with the build: seed 12 is held under gcc and
   clang Release and freed under ASan and the coverage build, and seed 5,
   held in run-cen-0373a01d5, is freed on this tree. A single-seed pin
   cannot hold across CI's builds, so the held case is a count over the
   validator, which is doppler#3263's. The ghost rule's own pin, on a pool
   whose seed stays at k = 2, is _test_ghost_is_released. */
static int
_test_shipped_pool_frees_a_dropped_ghost (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  _g415_t z;
  DP_REQUIRE (
      _g415_scene (code, 2, 60.0, 2, 2.0, DP_CCSDS_415_LOCK_S, true, &z) == 0);
  dp_async_dsss_pool_state_t *p = z.p;
  printf ("k = 2 scene, shipped pool, seed 2, %.0f s: carrier %.1f Hz off "
          "A's, %ld tracking steps (%ld code up, %ld symbol up, %ld both "
          "down), released ghost %llu duplicate %llu lost %llu\n",
          DP_CCSDS_415_LOCK_S, z.df_end, z.n, z.code_up, z.sym_up, z.both_down,
          (unsigned long long)p->released_ghost,
          (unsigned long long)p->released_duplicate,
          (unsigned long long)p->released_lost);
  DP_REQUIRE_MSG (z.tracked, "the seed reached tracking");
  DP_REQUIRE (dp_async_dsss_pool_status (p, 0).state
              == ASYNC_DSSS_RX_TRACKING);
  DP_CHECK_MSG (fabs (z.df_end) < 0.5 * p->lattice_hz,
                "the fine hand-over took the seed to A's own carrier");
  DP_CHECK_MSG (p->released_lost == 1 && p->released_ghost == 0
                    && p->released_duplicate == 0,
                "the loss release frees the dropped ghost, and nothing else "
                "releases it");
  dp_async_dsss_pool_destroy (p);
  return 0;
}

/* The pool at the 415.1-B-1 point (Ccsds415Pool) builds every receiver
   and its one search from dp_ccsds_415_rx()'s table: each argument, read
   back from each of them, is the table's. The pool view makes its own
   call into dp_async_dsss_pool_create(), so a slip there (pfa and pd
   swapped, reacquire for differential) shows only here: a state blob
   carries neither. Both carriers, refine on and off. */
static int
_test_the_ccsds415_pool_is_the_point (void)
{
  uint8_t code[DP_CCSDS_415_SF];
  dp_dsss_gold_1023 (code);
  static const double fcs[] = { DP_CCSDS_415_FC_LO, DP_CCSDS_415_FC_HI };
  for (size_t f = 0; f < 2; f++)
    for (int refine = 0; refine < 2; refine++)
      {
        dp_ccsds_415_rx_t a;
        DP_REQUIRE (
            dp_ccsds_415_rx (&a, DP_CCSDS_415_SF, fcs[f], 0.4, refine, NULL)
            == 0);
        dp_s415_table_is_expected (&a, fcs[f], 0.4, refine);
        dp_async_dsss_pool_state_t *p = dp_async_dsss_pool_create_ccsds415 (
            code, DP_CCSDS_415_SF, 2, fcs[f], 0.4, 1, refine, NULL);
        DP_REQUIRE (p);
        for (size_t i = 0; i < p->n_receivers; i++)
          dp_s415_rx_is_the_table (p->rx[i], &a);
        dp_s415_rx_is_the_table (p->search, &a);
        dp_async_dsss_pool_destroy (p);
      }
  return 0;
}

/* Ccsds415Pool's refusals, in C (#3267, P29): each returns NULL with @p why
   naming its own cause, and a NULL @p why is allowed. The cases are the
   table's (a code that is not 1,023 chips, a carrier past either edge, a
   negative lost_confirm_s), the receiver count out of 1 to MAX, and a
   negative thread count. Each row's sentence is matched on a phrase only
   its cause's sentence holds, so a refusal reporting another cause is red.
   Sabotage: drop the threads < 0 check -> the -1 row builds a pool; swap
   the receiver-count and thread sentences -> both rows red. */
static int
_test_the_ccsds415_pool_refuses_with_its_cause (void)
{
  uint8_t code[DP_CCSDS_415_SF];
  dp_dsss_gold_1023 (code);
  static const struct
  {
    size_t      code_len, n_rx;
    double      fc, lost;
    int         threads;
    const char *phrase;
  } rows[] = {
    { 1022, 2, DP_CCSDS_415_FC_LO, 0.4, 1, "1,023 chips" },
    { DP_CCSDS_415_SF, 2, 2.0e9, 0.4, 1, "below DP_CCSDS_415_RX_FC_MIN" },
    { DP_CCSDS_415_SF, 2, 8.0e9, 0.4, 1, "above DP_CCSDS_415_RX_FC_MAX" },
    { DP_CCSDS_415_SF, 2, DP_CCSDS_415_FC_LO, -1.0, 1, "lost_confirm_s" },
    { DP_CCSDS_415_SF, 0, DP_CCSDS_415_FC_LO, 0.4, 1, "n_receivers" },
    { DP_CCSDS_415_SF, ASYNC_DSSS_POOL_MAX_RECEIVERS + 1, DP_CCSDS_415_FC_LO,
      0.4, 1, "n_receivers" },
    { DP_CCSDS_415_SF, 2, DP_CCSDS_415_FC_LO, 0.4, -1, "threads" },
  };
  for (size_t k = 0; k < sizeof rows / sizeof rows[0]; k++)
    {
      const char                 *why = NULL;
      dp_async_dsss_pool_state_t *p   = dp_async_dsss_pool_create_ccsds415 (
          code, rows[k].code_len, rows[k].n_rx, rows[k].fc, rows[k].lost,
          rows[k].threads, true, &why);
      if (p || !why || !strstr (why, rows[k].phrase))
        DP_TEST_ERR ("row %zu: %s\n", k, why ? why : "(why unset)");
      DP_CHECK_MSG (p == NULL, "the pool view refuses");
      DP_CHECK_MSG (why && strstr (why, rows[k].phrase),
                    "the refusal names its own cause");
      dp_async_dsss_pool_destroy (p);
      DP_CHECK (dp_async_dsss_pool_create_ccsds415 (
                    code, rows[k].code_len, rows[k].n_rx, rows[k].fc,
                    rows[k].lost, rows[k].threads, true, NULL)
                == NULL);
    }
  return 0;
}

int
main (void)
{
  DP_CHECK (_test_create_domain () == 0);
  DP_CHECK (_test_the_ccsds415_pool_is_the_point () == 0);
  DP_CHECK (_test_the_ccsds415_pool_refuses_with_its_cause () == 0);
  DP_CHECK (_test_refine_passes_through () == 0);
  DP_CHECK (_test_ghost_test_edges () == 0);
  DP_CHECK (_test_ghost_is_released () == 0);
  DP_CHECK (_test_ghost_parent_is_the_stronger () == 0);
  DP_CHECK (_test_shipped_pool_frees_a_dropped_ghost () == 0);
  DP_CHECK (_test_ghost_needs_both_tests () == 0);
  DP_CHECK (_test_search_runs_once () == 0);
  DP_CHECK (_test_duplicates_keep_the_first () == 0);
  DP_CHECK (_test_duplicate_zone_is_one_chip_circular () == 0);
  DP_CHECK (_test_reset_and_threads () == 0);
  DP_CHECK (_test_reset_and_restore_edges () == 0);
  DP_CHECK (_test_reset_clears_the_carry () == 0);
  DP_CHECK (_test_telemetry_one_producer () == 0);
  DP_CHECK (_test_lost_is_released () == 0);
  DP_CHECK (_test_release_latency () == 0);
  DP_CHECK (_test_search_restarts_and_names_survive () == 0);
  DP_CHECK (_test_reacquire_releases_itself () == 0);
  DP_CHECK (_test_borrowed_lock_lasts_one_chain () == 0);
  DP_CHECK (_test_two_emitters_two_holders () == 0);
  DP_CHECK (_test_seeds_go_strongest_first_in_index_order () == 0);
  DP_CHECK (_test_fractional_period_phase (1.1) == 0);
  DP_CHECK (_test_fractional_period_phase (1.05) == 0);
  DP_CHECK (_test_zone_keeps_the_outcome () == 0);
  DP_CHECK (_test_zone_lists_the_weaker () == 0);
  DP_CHECK (_test_zone_delays_a_same_lag_pair () == 0);
  DP_CHECK (_test_zone_keeps_a_neighbour_past_the_key () == 0);
  DP_CHECK (_test_zone_seeds_no_ghost () == 0);
  DP_CHECK (_test_threads_change_nothing () == 0);
  DP_CHECK (_test_state_round_trip () == 0);
  DP_CHECK (_test_chunk_invariance () == 0);
  DP_CHECK (_test_refused_restore_changes_nothing () == 0);
  DP_CHECK (_test_nested_trace_version_refused () == 0);
  DP_TEST_END ("test_async_dsss_pool_core");
}
