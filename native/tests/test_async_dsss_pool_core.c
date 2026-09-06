/**
 * @file test_async_dsss_pool_core.c
 * @brief AsyncDsssPool -- the holder's lifecycle on the shipped C stimulus
 *        (design section 8.2): assigned once, tracked, released, assigned
 *        again; a second emitter takes a second slot and a full pool counts
 *        the drop; the transitions reach the log; the same records across
 *        threads; the state round-trips mid-stream.
 *
 * The stimulus is dp_dsss_capture(): a 1023-chip code at 5 Mcps, two
 * samples per chip, asynchronous BPSK data at 2700 sym/s, a fixed carrier
 * offset, AWGN from the C/N0 -- one capture per emitter, summed. The
 * searcher is the continuous engine at D = 1 (no window in this stimulus)
 * over +-6 kHz, so two emitters two rows apart are two peaks; the release
 * interval is 0.3 s so a switched-off emitter is released inside the test.
 * The searcher's false alarms are part of the lifecycle: at pfa 1e-3 a
 * noise peak seeds a free slot every few hundred ms, refines to nothing,
 * and is released one interval later -- so every expectation below is
 * about the EMITTER's slot, never an exact count of slots.
 */
#include "async_dsss_pool/async_dsss_pool_core.h"
#include "dp_dsss_test.h"
#include "dp_rng_test.h"
#include "dp_test.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SF 1023u
#define SPC 2u
#define TE (SF * SPC)
#define CHIP_RATE 5.0e6
#define FS (CHIP_RATE * (double)SPC)
#define SYM_RATE 2700.0
#define TSYM (FS / SYM_RATE)
#define CN0 47.0
#define DU 6000.0   /* the searcher's span: +-6 kHz, D = 1 rows of 4.9 kHz */
#define LOST_S 0.3  /* the release interval, short enough to see       */
#define N_SYM 2700u /* one second per capture                          */
#define PRE_SILENCE 3u

static uint8_t g_code[SF];

static void
make_code (void)
{
  uint32_t cst = 13;
  for (size_t i = 0; i < SF; i++)
    g_code[i] = (uint8_t)(dp_bit (&cst) > 0 ? 0u : 1u);
}

static async_dsss_pool_state_t *
make_pool (size_t n_slots, int threads)
{
  return async_dsss_pool_create (g_code, SF, CHIP_RATE, SYM_RATE, SPC, 2, CN0,
                                 1e-3, 0.9, DU, 1, 0.0, 4, n_slots, threads,
                                 0.0, LOST_S, 0.0, 4, 8, 0, 0.5, 4, 14.0, 64,
                                 8, false, 100000);
}

/* One emitter: the capture at `doppler_hz`, its signal starting `delay`
   samples in (a chip phase), noise from the seed. */
typedef struct
{
  float _Complex *x;
  size_t          n;
  double         *data;
} cap_t;

static cap_t
emitter (double doppler_hz, size_t delay, uint32_t seed)
{
  cap_t c;
  dp_dsss_capture (g_code, SF, SPC, FS, TSYM, doppler_hz, CN0, N_SYM,
                   PRE_SILENCE + delay, seed, &c.x, &c.n, &c.data);
  return c;
}

/* The sum of two captures (the second's tail past the first's end is
   dropped) -- two emitters on one channel. */
static cap_t
sum2 (const cap_t *a, const cap_t *b)
{
  cap_t c;
  c.n    = a->n < b->n ? a->n : b->n;
  c.x    = malloc (c.n * sizeof *c.x);
  c.data = NULL;
  for (size_t i = 0; i < c.n; i++)
    c.x[i] = a->x[i] + b->x[i];
  return c;
}

/* Feed `n` samples from `x` in epochs; returns the assigned count after. */
static size_t
feed (async_dsss_pool_state_t *p, const float _Complex *x, size_t n)
{
  size_t last = 0;
  for (size_t pos = 0; pos + TE <= n; pos += TE)
    last = async_dsss_pool_push (p, x + pos, TE);
  return last;
}

/* The code phase an emitter that started `delay` samples after
   PRE_SILENCE has at stream position `sample`: the capture's chip 0 is at
   PRE_SILENCE + delay, and the code advances a chip every SPC samples. */
static double
truth_chip (size_t delay, uint64_t sample)
{
  return dp_fmod_pos (((double)sample - (double)(PRE_SILENCE + delay))
                          / (double)SPC,
                      (double)SF);
}

/* The slot holding the emitter seeded at `doppler_hz` (within the
   searcher's row) AND at that emitter's code phase (within a chip) -- a
   false alarm in the same row is at another phase -- or n_slots; and how
   many slots hold it: one, or the table has failed. */
static size_t
slot_of (async_dsss_pool_state_t *p, double doppler_hz, size_t delay,
         size_t *count)
{
  size_t found = p->n_slots, n = 0;
  for (size_t i = 0; i < p->n_slots; i++)
    {
      async_dsss_pool_slot_t r = async_dsss_pool_status (p, i);
      if (!r.assigned
          || fabs (r.seed_doppler_hz - doppler_hz) > p->acq->doppler_res_hz)
        continue;
      double dc = fabs (r.seed_chip_phase - truth_chip (delay, r.seed_sample));
      if (dc > SF / 2.0)
        dc = SF - dc;
      if (dc <= 1.0)
        {
          found = i;
          n++;
        }
    }
  if (count)
    *count = n;
  return found;
}

static int
_test_arg_validation (void)
{
  DP_CHECK (async_dsss_pool_create (NULL, 0, CHIP_RATE, SYM_RATE, SPC, 2, CN0,
                                    1e-2, 0.9, DU, 1, 0.0, 4, 2, 1, 0.0,
                                    LOST_S, 0.0, 4, 8, 0, 0.5, 4, 14.0, 64, 8,
                                    false, 100000)
            == NULL);
  DP_CHECK (async_dsss_pool_create (g_code, SF, CHIP_RATE, SYM_RATE, SPC, 2,
                                    CN0, 1e-2, 0.9, DU, 1, 0.0, 4, 0, 1, 0.0,
                                    LOST_S, 0.0, 4, 8, 0, 0.5, 4, 14.0, 64, 8,
                                    false, 100000)
            == NULL); /* n_slots 0 */
  DP_CHECK (async_dsss_pool_create (g_code, SF, CHIP_RATE, SYM_RATE, SPC, 2,
                                    CN0, 1e-2, 0.9, DU, 1, 0.0, 4, 2, 1, -1.0,
                                    LOST_S, 0.0, 4, 8, 0, 0.5, 4, 14.0, 64, 8,
                                    false, 100000)
            == NULL); /* carrier < 0 */
  async_dsss_pool_state_t *p = make_pool (3, 1);
  DP_CHECK (p != NULL);
  if (!p)
    return 1;
  DP_CHECK (p->n_slots == 3 && p->n_assigned == 0 && p->events == 0);
  /* A slot that does not exist: a zero record with state -1, no symbols. */
  async_dsss_pool_slot_t r = async_dsss_pool_status (p, 3);
  DP_CHECK (r.state == -1 && r.assigned == 0);
  float _Complex out[4];
  DP_CHECK (async_dsss_pool_symbols (p, 3, out, 4) == 0);
  DP_CHECK (async_dsss_pool_symbols (p, 0, out, 4) == 0);
  DP_CHECK (async_dsss_pool_status (p, 0).state == ASYNC_DSSS_RX_IDLE);
  async_dsss_pool_destroy (p);
  return 0;
}

/* One emitter, three slots: assigned ONCE (the searcher keeps hitting it
   every dwell; every later hit is inside its own row's zone), tracked with
   symbols out, released by the rule when it goes off, and assigned again
   into a free slot when it returns. */
static int
_test_one_emitter_lifecycle (void)
{
  cap_t                    e = emitter (1500.0, 40, 100u);
  async_dsss_pool_state_t *p = make_pool (3, 1);
  DP_REQUIRE (p != NULL);

  /* All but the last eight epochs, then those one at a time with the
     symbols read after each: an epoch carries half a symbol, so one push
     may emit none. */
  const size_t    tail     = 8 * TE;
  size_t          assigned = feed (p, e.x, e.n - tail);
  size_t          count    = 0;
  size_t          slot     = slot_of (p, 1500.0, 40, &count);
  float _Complex *syms
      = malloc (async_dsss_pool_symbols_max_out (p) * sizeof *syms);
  size_t ns = 0, ns_other = 0;
  for (size_t pos = e.n - tail; pos + TE <= e.n; pos += TE)
    {
      assigned = async_dsss_pool_push (p, e.x + pos, TE);
      for (size_t i = 0; i < 3; i++)
        {
          size_t k = async_dsss_pool_symbols (
              p, i, syms, async_dsss_pool_symbols_max_out (p));
          if (i == slot)
            ns += k;
          else
            ns_other += k;
        }
    }
  DP_CHECK_MSG (assigned >= 1 && count == 1,
                "one emitter takes exactly one slot, however many dwells "
                "hit it");
  DP_CHECK (p->dropped == 0);
  DP_REQUIRE (slot < 3);
  async_dsss_pool_slot_t r = async_dsss_pool_status (p, slot);
  DP_CHECK_MSG (r.assigned == 1 && r.state == ASYNC_DSSS_RX_TRACKING
                    && r.code_locked == 1,
                "the assigned receiver tracks the emitter with code lock");
  DP_CHECK_MSG (fabs (r.seed_doppler_hz - 1500.0) <= p->acq->doppler_res_hz,
                "the seed's Doppler is the emitter's row");
  DP_CHECK_MSG (fabs (r.doppler_hz - 1500.0) < 100.0,
                "and the live Doppler has converged on it");
  DP_CHECK (r.assigned_samples > 0 && r.seed_sample < p->samples_consumed);
  /* The seed's chip phase against the capture's, within the searcher's
     half-chip cell. */
  double dc = fabs (r.seed_chip_phase - truth_chip (40, r.seed_sample));
  if (dc > SF / 2.0)
    dc = SF - dc;
  DP_CHECK_MSG (dc <= 0.5, "the seed's chip phase is the emitter's, within "
                           "the searcher's cell");
  DP_CHECK_MSG (ns > 0, "the last pushes' symbols are readable by slot");
  /* A slot holding a false alarm may emit symbols of noise; one holding
     nothing emits none. */
  for (size_t i = 0; i < 3; i++)
    if (i != slot && !async_dsss_pool_status (p, i).assigned)
      DP_CHECK (async_dsss_pool_symbols (p, i, syms,
                                         async_dsss_pool_symbols_max_out (p))
                == 0);
  (void)ns_other;
  DP_CHECK_MSG (p->events >= 2, "seeded and tracking were counted");
  const uint64_t events_tracking = p->events;

  /* Off the air: noise alone for twice the release interval. */
  size_t          n_off = (size_t)(2.0 * LOST_S * FS);
  float _Complex *nz;
  size_t          nn;
  double         *nd;
  dp_dsss_capture (g_code, SF, SPC, FS, TSYM, 0.0, CN0, 1, n_off, 200u, &nz,
                   &nn, &nd);
  assigned = feed (p, nz, n_off);
  (void)slot_of (p, 1500.0, 40, &count);
  DP_CHECK_MSG (count == 0, "the receiver reports lost and the pool "
                            "releases its slot");
  DP_CHECK_MSG (p->events >= events_tracking + 2, "lost and released");
  free (nz);
  free (nd);

  /* Back on the air: assigned again -- the one re-assignment. */
  assigned     = feed (p, e.x, e.n);
  size_t again = slot_of (p, 1500.0, 40, &count);
  DP_CHECK_MSG (assigned >= 1 && count == 1 && again < 3,
                "a returning emitter is a new detection into a free slot");
  DP_CHECK (async_dsss_pool_status (p, again).state == ASYNC_DSSS_RX_TRACKING);

  async_dsss_pool_reset (p);
  DP_CHECK (p->n_assigned == 0 && p->events == 0 && p->samples_consumed == 0);
  free (syms);
  async_dsss_pool_destroy (p);
  free (e.x);
  free (e.data);
  return 0;
}

/* Two emitters two rows apart at different code phases: two slots, each
   seeded at its own emitter; with one slot, the second is counted dropped.
   Across threads the records are the same, bit for bit. */
static int
_test_two_emitters_and_a_full_pool (void)
{
  cap_t a = emitter (1500.0, 40, 101u);
  cap_t b = emitter (-3500.0, 900, 102u);
  cap_t s = sum2 (&a, &b);

  async_dsss_pool_state_t *p1 = make_pool (4, 1);
  async_dsss_pool_state_t *p2 = make_pool (4, 3);
  async_dsss_pool_state_t *p0 = make_pool (1, 1);
  DP_REQUIRE (p1 && p2 && p0);
  DP_CHECK (feed (p1, s.x, s.n) >= 2);
  DP_CHECK (feed (p2, s.x, s.n) >= 2);
  DP_CHECK_MSG (p1->dropped == 0, "nothing is dropped with slots to spare");
  size_t na = 0, nb = 0;
  size_t sa = slot_of (p1, 1500.0, 40, &na),
         sb = slot_of (p1, -3500.0, 900, &nb);
  DP_CHECK_MSG (na == 1 && nb == 1 && sa != sb,
                "each emitter holds exactly one slot of its own");
  DP_REQUIRE (sa < 4 && sb < 4);
  async_dsss_pool_slot_t ra = async_dsss_pool_status (p1, sa);
  async_dsss_pool_slot_t rb = async_dsss_pool_status (p1, sb);
  DP_CHECK_MSG (ra.state == ASYNC_DSSS_RX_TRACKING && ra.code_locked == 1
                    && rb.state == ASYNC_DSSS_RX_TRACKING
                    && rb.code_locked == 1,
                "both track their emitter with code lock");
  DP_CHECK_MSG (fabs (ra.doppler_hz - 1500.0) < 100.0
                    && fabs (rb.doppler_hz + 3500.0) < 100.0,
                "and the live Doppler is each emitter's own");
  /* Threads: the same assignments, the same symbols, the same counts. */
  DP_CHECK (p2->n_assigned == p1->n_assigned && p2->events == p1->events
            && p2->dropped == p1->dropped);
  size_t          cap = async_dsss_pool_symbols_max_out (p1);
  float _Complex *s1  = malloc (cap * sizeof *s1);
  float _Complex *s2  = malloc (cap * sizeof *s2);
  for (size_t i = 0; i < 4; i++)
    {
      size_t n1 = async_dsss_pool_symbols (p1, i, s1, cap);
      size_t n2 = async_dsss_pool_symbols (p2, i, s2, cap);
      DP_CHECK (n1 == n2);
      DP_CHECK_MSG (n1 == 0 || memcmp (s1, s2, n1 * sizeof *s1) == 0,
                    "the receivers' symbols are bit-identical across "
                    "threads");
    }
  free (s1);
  free (s2);
  /* One slot: the first emitter seen holds it, the other is dropped --
     and counted every dwell it is seen, so at least once. */
  DP_CHECK (feed (p0, s.x, s.n) == 1);
  DP_CHECK_MSG (p0->n_assigned == 1 && p0->dropped >= 1,
                "a full pool never exceeds n_slots and counts the drop");
  DP_CHECK_MSG (slot_of (p0, 1500.0, 40, NULL) == 0
                    || slot_of (p0, -3500.0, 900, NULL) == 0,
                "and the one slot holds one of the emitters");
  async_dsss_pool_destroy (p0);
  async_dsss_pool_destroy (p1);
  async_dsss_pool_destroy (p2);
  free (s.x);
  free (a.x);
  free (a.data);
  free (b.x);
  free (b.data);
  return 0;
}

/* The event log: every counted transition is an appended event, and the
   labels are the design's. */
static int
_test_event_log (void)
{
  char path[64];
  (void)snprintf (path, sizeof path, "/tmp/dp_pool_test_%d.events",
                  (int)getpid ());
  dp_event_log_t *log = dp_event_log_open (path, 0.0);
  DP_REQUIRE (log != NULL);
  cap_t                    e = emitter (1500.0, 40, 103u);
  async_dsss_pool_state_t *p = make_pool (2, 1);
  DP_REQUIRE (p != NULL);
  DP_CHECK (async_dsss_pool_set_event_log (p, log) == DP_OK);
  (void)feed (p, e.x, e.n);
  size_t          n_off = (size_t)(2.0 * LOST_S * FS);
  float _Complex *nz;
  size_t          nn;
  double         *nd;
  dp_dsss_capture (g_code, SF, SPC, FS, TSYM, 0.0, CN0, 1, n_off, 201u, &nz,
                   &nn, &nd);
  (void)feed (p, nz, n_off);
  DP_CHECK_MSG (dp_event_log_count (log) == p->events && p->events >= 4,
                "every transition the pool counted reached the log");
  DP_CHECK (dp_event_log_close (log) == DP_OK);
  /* The labels, in order: seeded, tracking, lost, released. */
  FILE *f = fopen (path, "r");
  DP_REQUIRE (f != NULL);
  static const char *const want[4]
      = { "\"seeded\"", "\"tracking\"", "\"lost\"", "\"released\"" };
  char line[DP_EVENT_LOG_LINE_MAX];
  int  found = 0;
  while (found < 4 && fgets (line, sizeof line, f))
    if (strstr (line, want[found]))
      found++;
  fclose (f);
  DP_CHECK_MSG (found == 4, "seeded, tracking, lost and released, in order");
  DP_CHECK (async_dsss_pool_set_event_log (p, NULL) == DP_OK);
  dp_event_log_destroy (log);
  remove (path);
  free (nz);
  free (nd);
  async_dsss_pool_destroy (p);
  free (e.x);
  free (e.data);
  return 0;
}

/* A mid-stream split resumes bit for bit in a fresh pool; the envelope
   and a foreign slot count are rejected. */
static int
_test_state_roundtrip (void)
{
  cap_t                    e    = emitter (1500.0, 40, 104u);
  async_dsss_pool_state_t *ref  = make_pool (2, 1);
  async_dsss_pool_state_t *live = make_pool (2, 1);
  async_dsss_pool_state_t *cold = make_pool (2, 1);
  DP_REQUIRE (ref && live && cold);
  const size_t half = (e.n / TE / 2) * TE;
  (void)feed (ref, e.x, e.n);
  (void)feed (live, e.x, half);
  DP_CHECK (slot_of (live, 1500.0, 40, NULL) < 2);
  size_t cb   = async_dsss_pool_state_bytes (live);
  void  *blob = malloc (cb);
  async_dsss_pool_get_state (live, blob);
  DP_CHECK (async_dsss_pool_set_state (cold, blob) == DP_OK);
  DP_CHECK (cold->n_assigned == live->n_assigned
            && cold->samples_consumed == live->samples_consumed
            && memcmp (cold->rows, live->rows, 2 * sizeof *cold->rows) == 0);
  /* The rest of the stream through both; the same symbols out. */
  size_t          cap = 0;
  float _Complex *sl = NULL, *sc = NULL;
  for (size_t pos = half; pos + TE <= e.n; pos += TE)
    {
      (void)async_dsss_pool_push (live, e.x + pos, TE);
      (void)async_dsss_pool_push (cold, e.x + pos, TE);
      if (!sl)
        {
          cap = async_dsss_pool_symbols_max_out (live);
          sl  = malloc (cap * sizeof *sl);
          sc  = malloc (cap * sizeof *sc);
        }
      for (size_t i = 0; i < 2; i++)
        {
          size_t nl = async_dsss_pool_symbols (live, i, sl, cap);
          size_t nc = async_dsss_pool_symbols (cold, i, sc, cap);
          DP_CHECK (nl == nc);
          if (nl && memcmp (sl, sc, nl * sizeof *sl) != 0)
            {
              DP_CHECK_MSG (0, "a resumed pool's symbols are bit-identical");
              break;
            }
        }
    }
  DP_CHECK (cold->events == live->events
            && cold->n_assigned == live->n_assigned);
  free (sl);
  free (sc);
  /* Rejects: a clobbered envelope, and a pool of another size. */
  ((char *)blob)[0] ^= (char)0xFF;
  DP_CHECK (async_dsss_pool_set_state (cold, blob) == DP_ERR_INVALID);
  ((char *)blob)[0] ^= (char)0xFF;
  async_dsss_pool_state_t *other = make_pool (3, 1);
  DP_REQUIRE (other != NULL);
  DP_CHECK (async_dsss_pool_set_state (other, blob) == DP_ERR_INVALID);
  free (blob);
  async_dsss_pool_destroy (other);
  async_dsss_pool_destroy (cold);
  async_dsss_pool_destroy (live);
  async_dsss_pool_destroy (ref);
  free (e.x);
  free (e.data);
  return 0;
}

/* doppler#1261: a hand-over past loop 1's pull-in leaves the receiver
   code-locked, carrier-unlocked, and loop 1 free-running -- measured here:
   the searcher at pfa 1e-2 (a 6-epoch dwell) seeds this capture 1847 Hz
   for a 1500 Hz emitter and the reported Doppler climbs to 2300 Hz within
   a second with the symbol lock down throughout. The table must not key
   its zone on that: the row holds the last LOCKED Doppler (the seed, here)
   while the status wanders. The same stimulus at pfa 1e-3 seeds 1395 Hz,
   loop 1 pulls in, and the row follows the locked loop. Sabotage: refresh
   the row from the status regardless of `locked` -> red. */
static int
_test_table_holds_the_locked_doppler (void)
{
  cap_t e = emitter (1500.0, 40, 101u);
  for (int k = 0; k < 2; k++)
    {
      const double             pfa = k ? 1e-3 : 1e-2;
      async_dsss_pool_state_t *p   = async_dsss_pool_create (
          g_code, SF, CHIP_RATE, SYM_RATE, SPC, 2, CN0, pfa, 0.9, DU, 1, 0.0,
          4, 3, 1, 0.0, LOST_S, 0.0, 4, 8, 0, 0.5, 4, 14.0, 64, 8, false,
          100000);
      DP_REQUIRE (p != NULL);
      /* The reproduction needs the refine's OLD two-block dwell: the floor
         of #1265 (seven blocks) hands over within loop 1's pull-in and
         the wander never happens -- which is that fix's own pin, in
         test_async_dsss_receiver_core. Removed here so the table's rule
         is still tested against a carrier that free-runs. */
      DP_CHECK (async_dsss_pool_set_refine_min_blocks (p, 0) == DP_OK);
      double worst_status = 0.0;
      int    unlocked = 0, tracking = 0;
      /* The row and the seed at the LAST tracking block, read inside the
         loop: the slot can be empty once the capture ends (the receiver
         released), and a record read then is all zeros -- an assertion on
         it passed vacuously until this was noticed (2026-09-06). */
      double row = 0.0, seed = 0.0;
      for (size_t pos = 0; pos + TE <= e.n; pos += TE)
        {
          (void)async_dsss_pool_push (p, e.x + pos, TE);
          size_t slot = slot_of (p, 1500.0, 40, NULL);
          if (slot == p->n_slots)
            continue;
          async_dsss_pool_slot_t r = async_dsss_pool_status (p, slot);
          if (r.state != ASYNC_DSSS_RX_TRACKING)
            continue;
          tracking++;
          unlocked += !r.locked;
          if (fabs (r.doppler_hz - 1500.0) > worst_status)
            worst_status = fabs (r.doppler_hz - 1500.0);
          row  = p->rows[slot].doppler_hz;
          seed = r.seed_doppler_hz;
        }
      DP_REQUIRE (tracking > 0);
      printf ("  pfa %.0e: status wandered to %.0f Hz off; carrier unlocked "
              "on %d of %d tracking blocks; the row ended at %.0f Hz, the "
              "seed was %.0f\n",
              pfa, worst_status, unlocked, tracking, row, seed);
      if (k == 0)
        {
          DP_CHECK_MSG (worst_status > 500.0 && unlocked > tracking / 2,
                        "the stimulus reproduces #1261: loop 1 wanders "
                        "with the carrier unlocked");
          DP_CHECK_MSG (row == seed,
                        "and the table never took the unlocked estimate: "
                        "the row is still the seed's");
        }
      else
        DP_CHECK_MSG (fabs (row - 1500.0) < 100.0 && unlocked < tracking / 10,
                      "locked, the row follows the loop within 100 Hz");
      async_dsss_pool_destroy (p);
    }
  free (e.x);
  free (e.data);
  return 0;
}

int
main (void)
{
  make_code ();
  (void)_test_arg_validation ();
  (void)_test_table_holds_the_locked_doppler ();
  (void)_test_one_emitter_lifecycle ();
  (void)_test_two_emitters_and_a_full_pool ();
  (void)_test_event_log ();
  (void)_test_state_roundtrip ();
  DP_TEST_END ("test_async_dsss_pool_core");
}
