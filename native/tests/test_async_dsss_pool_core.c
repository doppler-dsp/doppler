/**
 * @file test_async_dsss_pool_core.c
 * @brief AsyncDsssPool: the controller's rules, each pinned against its
 *        header claim (async_dsss_pool_core.h), and each test's sabotage
 *        named in its comment.
 */
#include "doppler/async_dsss_pool/async_dsss_pool_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"
#include "dp_chunk_inv.h"
#include "dp_dsss_test.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_test.h"
#include <math.h>
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

static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

/* A pool on the Gold-1023 design point; the receiver tests' _slow_rx()
   numbers, n receivers, the release rule off unless asked. */
static dp_async_dsss_pool_state_t *
_pool (const uint8_t *code, size_t n, double lost_confirm_s, int threads)
{
  return dp_async_dsss_pool_create (code, 1023, n, POOL_CR, POOL_RS, POOL_SPC,
                                    2, 47.0, 1e-3, 0.9, 500.0, 4, 8, 0, 0.5, 4,
                                    14.0, 64, 8, false, 100000, 0.0,
                                    lost_confirm_s, false, 0.0, 0.0, threads);
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
  DP_CHECK (dp_async_dsss_pool_create (NULL, 1023, 2, POOL_CR, POOL_RS,
                                       POOL_SPC, 2, 47.0, 1e-3, 0.9, 500.0, 4,
                                       8, 0, 0.5, 4, 14.0, 64, 8, false,
                                       100000, 0.0, 0.0, false, 0.0, 0.0, 1)
            == NULL);
  dp_async_dsss_pool_state_t *p
      = _pool (code, ASYNC_DSSS_POOL_MAX_RECEIVERS, 0.0, 0);
  DP_REQUIRE (p != NULL);
  DP_CHECK (p->n_receivers == ASYNC_DSSS_POOL_MAX_RECEIVERS);
  dp_async_dsss_pool_destroy (p);
  /* A search over several Doppler tiles (+/-5 kHz, tiles 3 kHz apart)
     would start an engine pool of every core on its own; in a pool it
     runs one thread. */
  p = dp_async_dsss_pool_create (
      code, 1023, 2, POOL_CR, POOL_RS, POOL_SPC, 2, 60.0, 1e-3, 0.9, 5000.0, 4,
      8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0, 0);
  DP_REQUIRE (p != NULL && p->rx[0]->acq->window_bins > 1);
  for (size_t i = 0; i < p->n_receivers; i++)
    DP_CHECK (dp_async_dsss_receiver_get_threads (p->rx[i]) == 1);
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
   one each, and the third keeps searching, excluded from both; nothing is
   a duplicate. Sabotage: skip the exclusions -> the third (and every
   reset) piles onto A; skip the ranks -> receivers 0 and 1 seed on A in
   one dwell and one is released as a duplicate. */
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
  DP_CHECK (st[2].state == ASYNC_DSSS_RX_SEARCHING);
  DP_CHECK (_chip_dist (st[0].chip_phase, st[1].chip_phase, 1023.0) > 100.0);
  DP_CHECK (p->released_duplicate == 0);
  /* The searcher is excluded from both holders, and a holder from the
     other. */
  DP_CHECK (dp_async_dsss_receiver_get_n_exclusions (p->rx[2]) == 2);
  DP_CHECK (dp_async_dsss_receiver_get_n_exclusions (p->rx[0]) == 1);
  DP_CHECK (dp_async_dsss_receiver_get_rank (p->rx[2]) == 0);
  /* The holders track and emit; the searcher never emitted. */
  DP_CHECK (st[0].state == ASYNC_DSSS_RX_TRACKING
            && st[1].state == ASYNC_DSSS_RX_TRACKING);
  DP_CHECK (emitted[0] > 0 && emitted[1] > 0);
  DP_CHECK (emitted[2] == 0);
  dp_async_dsss_pool_destroy (p);
  free (x);
  return 0;
}

/* Duplicates: two holders within a chip hold one signal, and the keeper
   is tracking over refining, then the longer in its state, then the
   higher C/N0, then the lower index; the other is reset and excluded
   from the keeper's phase. The holders here are seeded by hand through
   the borrow, so each rung is set up exactly. Sabotage: drop the
   duplicate pass -> both keep refining; reverse any rung -> the wrong
   receiver is reset. */
static int
_test_duplicates_keep_the_first (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  float _Complex z[2046] = { 0 };
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
          (void)dp_async_dsss_pool_push (p, z, 2046);
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
      (void)dp_async_dsss_pool_push (p, z, 2046);
      DP_CHECK (p->released_duplicate == before + 1);
      const size_t loser = 1 - expect_keeper;
      DP_CHECK_MSG (dp_async_dsss_pool_status (p, expect_keeper).state
                        == ASYNC_DSSS_RX_REFINING,
                    "the keeper is still refining");
      DP_CHECK_MSG (dp_async_dsss_pool_status (p, loser).state
                        == ASYNC_DSSS_RX_SEARCHING,
                    "the loser is reset");
      /* The loser now excludes the keeper's phase, and searches at rank 0
         of the two searchers (receiver 2 being the other). */
      DP_CHECK (dp_async_dsss_receiver_get_n_exclusions (p->rx[loser]) == 1);
      DP_CHECK (dp_async_dsss_receiver_get_rank (p->rx[loser])
                == (loser < 2 ? 0u : 1u));
      DP_CHECK (dp_async_dsss_receiver_get_rank (p->rx[2]) == 1);
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
  float _Complex z[2046] = { 0 };
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
      const size_t held = dp_async_dsss_pool_push (p, z, 2046);
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
  float _Complex *off = malloc (tail_n * sizeof *off);
  {
    const double sigma = 1.0 / sqrt (pow (10.0, cn0 / 10.0) / fs);
    uint32_t     st    = 99;
    for (size_t i = 0; i < tail_n; i++)
      off[i] = (float _Complex) (sigma / sqrt (2.0)) * dp_cgauss (&st);
  }
  dp_async_dsss_pool_state_t *p = dp_async_dsss_pool_create (
      CODE7, 7, 1, 1.0e6, sym_rate, 4, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, confirm_s, false, 0.0, 0.0, 1);
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
      /* The controller runs right after every whole period, and status()
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

/* The thread count changes nothing but the cost: one worker and three
   give the same state, byte for byte, and the same symbols. Sabotage:
   share one output row across receivers -> the symbols differ; run the
   controller inside the workers -> the order, and so the bytes, vary. */
static int
_test_threads_change_nothing (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t                      n;
  float _Complex             *x = _two_emitters (code, &n);
  dp_async_dsss_pool_state_t *a = _pool (code, 3, 0.0, 1);
  dp_async_dsss_pool_state_t *b = _pool (code, 3, 0.0, 3);
  DP_REQUIRE (a && b);
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
  DP_REQUIRE (half % (1023 * POOL_SPC) != 0 && half < CI_B_OFF);
  dp_async_dsss_pool_state_t *a = _pool (code, 3, CI_CONFIRM_S, 2);
  dp_async_dsss_pool_state_t *b = _pool (code, 3, CI_CONFIRM_S, 1);
  dp_async_dsss_pool_state_t *c = _pool (code, 2, CI_CONFIRM_S, 1);
  DP_REQUIRE (a && b && c);
  (void)dp_async_dsss_pool_push (a, x, half);
  DP_REQUIRE (a->carry_len == half % (1023 * POOL_SPC));
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
  const size_t rx2   = sizeof (dp_state_hdr_t) + sizeof (uint64_t) * 8
                       + src->period * sizeof (float _Complex)
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

/* reset() hands back every receiver searching, with no exclusions and
   rank 0, and the clock and counts at 0; set_threads refuses a negative
   count unchanged. Sabotage: leave the exclusions -> a reset pool's
   receivers still skip the old phases. */
static int
_test_reset_and_threads (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  float _Complex z[2046]        = { 0 };
  dp_async_dsss_pool_state_t *p = _pool (code, 3, 0.0, 1);
  DP_REQUIRE (p != NULL);
  DP_REQUIRE (dp_async_dsss_receiver_seed (p->rx[0], 10.0, 0.0, 45.0)
              == DP_OK);
  DP_REQUIRE (dp_async_dsss_receiver_seed (p->rx[1], 10.2, 0.0, 45.0)
              == DP_OK);
  (void)dp_async_dsss_pool_push (p, z, 2046);
  DP_REQUIRE (p->released_duplicate == 1 && p->samples == 2046);
  DP_REQUIRE (dp_async_dsss_receiver_get_n_exclusions (p->rx[1]) == 1);
  dp_async_dsss_pool_reset (p);
  DP_CHECK (p->samples == 0 && p->released_duplicate == 0
            && p->released_lost == 0);
  for (size_t i = 0; i < 3; i++)
    {
      DP_CHECK (dp_async_dsss_pool_status (p, i).state
                == ASYNC_DSSS_RX_SEARCHING);
      DP_CHECK (dp_async_dsss_receiver_get_n_exclusions (p->rx[i]) == 0);
      DP_CHECK (dp_async_dsss_receiver_get_rank (p->rx[i]) == 0);
    }
  DP_CHECK (dp_async_dsss_pool_set_threads (p, -1) == DP_ERR_INVALID);
  DP_CHECK (p->threads == 1);
  DP_CHECK (dp_async_dsss_pool_set_threads (p, 2) == DP_OK);
  DP_CHECK (dp_async_dsss_pool_get_threads (p) == 2);
  dp_async_dsss_pool_destroy (p);
  return 0;
}

/* Chunk invariance (dp_chunk_inv.h): the controller acts at code-period
   positions of the stream, so how the stream is cut into pushes changes
   nothing. One spec per holder, each comparing that receiver's symbols,
   on the two-emitter stream through the hand-offs, the duplicate checks
   and tracking (the third receiver searches throughout and emits none:
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
      code, 1023, 2, POOL_CR, POOL_RS, POOL_SPC, 2, 47.0, 1e-3, 0.9, 5000.0, 4,
      8, 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0, 1);
}

/* A real ghost is released. A alone at 70 dB-Hz, 1500 Hz, with data;
   receiver 0 takes it from the search. A probe built like the receivers,
   listing 16 peaks a dwell, names A's sidelobes, and receiver 1 is seeded
   on the first one more than a chip from A (chip 413, a tile off): the
   seed the search makes when noise lifts a sidelobe past its gate. It
   tracks A's carrier a lattice step away without code lock, and is
   released one window after it starts tracking; A is kept. Sabotage:
   drop the ghost pass -> receiver 1 holds to the end; compare the C/N0
   the other way -> the parent never qualifies and nothing is released;
   skip hist_clear on the release -> the reset receiver's ring outlives
   its track and the state round trip below refuses it. */
static int
_test_ghost_is_released (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const size_t    te = 1023 * POOL_SPC;
  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (code, 1023, POOL_SPC, POOL_FS, POOL_FS / POOL_RS, 1500.0,
                   70.0, 1000, 0, 4, &x, &n, &data);
  dp_async_dsss_pool_state_t     *p  = _ghost_pool (code);
  dp_async_dsss_receiver_state_t *pr = dp_async_dsss_receiver_create (
      code, 1023, POOL_CR, POOL_RS, POOL_SPC, 2, 47.0, 1e-3, 0.9, 5000.0, 4, 8,
      0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
  DP_REQUIRE (p && pr);
  (void)dp_async_dsss_receiver_set_threads (pr, 1);
  DP_REQUIRE (dp_acq_set_max_peaks (pr->acq, 16) == DP_OK);
  int    seeded = 0, tracked = 0;
  double ghost_ph = 0.0;
  for (size_t b = 0; b + te <= n && !p->released_ghost; b += te)
    {
      (void)dp_async_dsss_pool_push (p, x + b, te);
      tracked |= p->hist_len[1] > 0;
      if (seeded)
        continue;
      acq_result_t h[16];
      const size_t k = dp_acq_push (pr->acq, x + b, te, h, 16);
      if (k < 2 || h[0].samples_consumed != b + te)
        continue;
      acq_handoff_t h0, hi;
      dp_acq_build_handoff (pr->acq, &h[0], 1023, POOL_SPC, &h0);
      for (size_t i = 1; i < k && !seeded; i++)
        {
          dp_acq_build_handoff (pr->acq, &h[i], 1023, POOL_SPC, &hi);
          if (_chip_dist (hi.chip_phase, h0.chip_phase, 1023.0) > 1.0)
            {
              DP_REQUIRE (dp_async_dsss_receiver_seed (p->rx[1], hi.chip_phase,
                                                       hi.doppler_hz_est,
                                                       hi.cn0_dbhz_est)
                          == DP_OK);
              seeded   = 1;
              ghost_ph = hi.chip_phase;
            }
        }
    }
  DP_REQUIRE_MSG (seeded, "the probe listed a sidelobe");
  DP_CHECK (_chip_dist (ghost_ph, dp_async_dsss_pool_status (p, 0).chip_phase,
                        1023.0)
            > 100.0);
  DP_CHECK_MSG (tracked, "the ghost reached tracking");
  DP_CHECK_MSG (p->released_ghost == 1, "the ghost was released");
  DP_CHECK (p->released_duplicate == 0);
  DP_CHECK (dp_async_dsss_pool_status (p, 0).state == ASYNC_DSSS_RX_TRACKING);
  DP_CHECK (dp_async_dsss_pool_status (p, 1).state == ASYNC_DSSS_RX_SEARCHING);
  DP_CHECK (p->hist_len[1] == 0);
  /* And a pool that has released one round-trips. */
  dp_async_dsss_pool_state_t *q = _ghost_pool (code);
  DP_REQUIRE (q != NULL);
  DP_STATE_ROUNDTRIP_TEST (dp_async_dsss_pool, p, q);
  /* Its blob holds only what control() leaves: receiver 0's full ring and
     receiver 1's empty one. Each forgery below breaks one rule set_state
     checks, and is refused. Sabotage: drop any one check -> its row
     restores. Layout: header, the 8 extra fields (hist_pos last), the
     carry, then the lengths, the carrier rings and the code-lock rings. */
  {
    const size_t w     = p->ghost_window,
                 bytes = dp_async_dsss_pool_state_bytes (p);
    const size_t o_pos = sizeof (dp_state_hdr_t) + 7 * sizeof (uint64_t);
    const size_t o_len
        = o_pos + sizeof (uint64_t) + p->period * sizeof (float _Complex);
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
  dp_async_dsss_receiver_destroy (pr);
  free (x);
  free (data);
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
   carrier (50 dB-Hz, just handed off, its code lock still low) passes the
   code-lock test and is kept by its carrier. Sabotage: drop the code-lock
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
  const _pair_t weak = _distinct_pair (code, 50.0, -437.0, 2 * 198);
  DP_CHECK_MSG (weak.kept, "a weak emitter on its own carrier is kept");
  DP_CHECK_MSG (weak.lock_alone, "the code-lock test alone would fire");
  DP_CHECK (!weak.carrier_alone);
  return 0;
}

int
main (void)
{
  DP_CHECK (_test_create_domain () == 0);
  DP_CHECK (_test_ghost_test_edges () == 0);
  DP_CHECK (_test_ghost_is_released () == 0);
  DP_CHECK (_test_ghost_needs_both_tests () == 0);
  DP_CHECK (_test_duplicates_keep_the_first () == 0);
  DP_CHECK (_test_duplicate_zone_is_one_chip_circular () == 0);
  DP_CHECK (_test_reset_and_threads () == 0);
  DP_CHECK (_test_telemetry_one_producer () == 0);
  DP_CHECK (_test_lost_is_released () == 0);
  DP_CHECK (_test_two_emitters_two_holders () == 0);
  DP_CHECK (_test_threads_change_nothing () == 0);
  DP_CHECK (_test_state_round_trip () == 0);
  DP_CHECK (_test_chunk_invariance () == 0);
  DP_CHECK (_test_refused_restore_changes_nothing () == 0);
  DP_TEST_END ("test_async_dsss_pool_core");
}
