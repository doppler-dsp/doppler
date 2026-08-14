#include "dp_test.h"
#include "lockdet/lockdet_core.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int
main (void)
{

  /* create stores config; counts of 0 clamp to 1. */
  lockdet_state_t *d = lockdet_create (1.5, 1.2, 3, 2);
  DP_CHECK (d != NULL);
  if (!d)
    return 1;
  DP_CHECK (d->up_thresh == 1.5 && d->down_thresh == 1.2);
  DP_CHECK (d->n_up == 3 && d->n_down == 2);
  DP_CHECK (d->cnt == 0 && d->locked == 0);

  lockdet_state_t z;
  memset (&z, 0, sizeof z);
  lockdet_init (&z, 1.0, 1.0, 0, 0);
  DP_CHECK (z.n_up == 1 && z.n_down == 1);

  /* Declare exactly at n_up consecutive hits — not one look sooner. */
  DP_CHECK (lockdet_step (d, 2.0) == 0);
  DP_CHECK (lockdet_step (d, 2.0) == 0);
  DP_CHECK (d->cnt == 2);
  DP_CHECK (lockdet_step (d, 2.0) == 1);
  DP_CHECK (d->cnt == 0); /* run consumed by the declare */

  /* Locked: a metric in the [down, up] hysteresis band is sticky, and any
   * hit resets an in-flight drop run. */
  DP_CHECK (lockdet_step (d, 1.3) == 1); /* band: no drop progress   */
  DP_CHECK (lockdet_step (d, 1.0) == 1); /* miss 1 of 2              */
  DP_CHECK (d->cnt == 1);
  DP_CHECK (lockdet_step (d, 1.3) == 1); /* hit resets the drop run  */
  DP_CHECK (d->cnt == 0);
  DP_CHECK (lockdet_step (d, 1.0) == 1); /* miss 1 of 2 (fresh run)  */
  DP_CHECK (lockdet_step (d, 1.0) == 0); /* miss 2 of 2 -> dropped   */
  DP_CHECK (d->cnt == 0);

  /* Unlocked: a single miss resets the declare run (consecutive, not
   * cumulative); a band metric is a miss on the way up. */
  DP_CHECK (lockdet_step (d, 2.0) == 0);
  DP_CHECK (lockdet_step (d, 2.0) == 0);
  DP_CHECK (lockdet_step (d, 1.3) == 0); /* band = miss while unlocked */
  DP_CHECK (d->cnt == 0);
  DP_CHECK (lockdet_step (d, 2.0) == 0);
  DP_CHECK (lockdet_step (d, 2.0) == 0);
  DP_CHECK (lockdet_step (d, 2.0) == 1);

  /* Threshold edges are exclusive: x == up_thresh is not a hit;
   * x == down_thresh is not a miss. */
  lockdet_reset (d);
  DP_CHECK (d->locked == 0 && d->cnt == 0);
  DP_CHECK (lockdet_step (d, 1.5) == 0);
  DP_CHECK (d->cnt == 0);
  d->locked = 1;
  DP_CHECK (lockdet_step (d, 1.2) == 1);
  DP_CHECK (d->cnt == 0);

  /* configure preserves a live lock but restarts the verify run. */
  DP_CHECK (lockdet_step (d, 1.0) == 1); /* miss 1 of 2 in flight */
  DP_CHECK (d->cnt == 1);
  lockdet_configure (d, 2.0, 1.6, 4, 4);
  DP_CHECK (d->locked == 1 && d->cnt == 0);
  DP_CHECK (d->up_thresh == 2.0 && d->n_up == 4 && d->n_down == 4);

  /* reset drops the lock and clears the run; config survives. */
  lockdet_reset (d);
  DP_CHECK (d->locked == 0 && d->cnt == 0);
  DP_CHECK (d->up_thresh == 2.0 && d->n_down == 4);

  lockdet_destroy (d);

  /* n_up = n_down = 1: no time hysteresis — first hit declares, first
   * miss drops. */
  {
    lockdet_state_t e;
    memset (&e, 0, sizeof e);
    lockdet_init (&e, 0.5, 0.5, 1, 1);
    DP_CHECK (lockdet_step (&e, 0.6) == 1);
    DP_CHECK (lockdet_step (&e, 0.4) == 0);
  }

  /* steps() block path matches per-step. */
  {
    lockdet_state_t a, b;
    memset (&a, 0, sizeof a);
    memset (&b, 0, sizeof b);
    lockdet_init (&a, 1.5, 1.2, 2, 2);
    lockdet_init (&b, 1.5, 1.2, 2, 2);
    double seq[]  = { 2.0, 2.0, 1.3, 1.0, 1.0, 2.0, 2.0, 1.0 };
    size_t n      = sizeof seq / sizeof seq[0];
    int    got[8] = { 0 };
    lockdet_steps (&a, seq, got, n);
    for (size_t i = 0; i < n; i++)
      DP_CHECK (got[i] == lockdet_step (&b, seq[i]));
  }

  /* serializable state — mid-run whole-struct snapshot resumes exactly
   * (in-flight verify run included), and the envelope rejects a clobber. */
  {
    lockdet_state_t *a = lockdet_create (1.5, 1.2, 3, 2);
    DP_CHECK (lockdet_step (a, 2.0) == 0); /* cnt = 1 of 3, mid-declare */
    unsigned char blob[64];
    DP_CHECK (lockdet_state_bytes (a) <= sizeof blob);
    lockdet_get_state (a, blob);
    int ref1 = lockdet_step (a, 2.0);
    int ref2 = lockdet_step (a, 2.0);
    lockdet_destroy (a);

    lockdet_state_t *b = lockdet_create (1.5, 1.2, 3, 2);
    DP_CHECK (lockdet_set_state (b, blob) == DP_OK);
    DP_CHECK (b->cnt == 1 && b->locked == 0);
    blob[0] ^= (unsigned char)0xFF;
    DP_CHECK (lockdet_set_state (b, blob) == DP_ERR_INVALID);
    DP_CHECK (lockdet_step (b, 2.0) == ref1);
    DP_CHECK (lockdet_step (b, 2.0) == ref2);
    DP_CHECK (b->locked == 1); /* the resumed run declared on schedule */
    lockdet_destroy (b);
  }

  /* init() "doubles as a reconfigure that preserves the current decision"
   * (header) -- and it differs from configure() in exactly one way, which
   * nothing pinned: init leaves the in-flight verify RUN alone, while
   * configure clears it. Tested on a LIVE detector; the check above runs
   * init on a zeroed struct, where "preserves" is vacuously true. */
  {
    lockdet_state_t r;
    memset (&r, 0, sizeof r);
    lockdet_init (&r, 1.5, 1.2, 3, 3);
    DP_CHECK (lockdet_step (&r, 2.0) == 0); /* cnt = 1 of 3, mid-declare */
    DP_CHECK (r.cnt == 1);
    r.locked = 1; /* a live decision to preserve */
    lockdet_init (&r, 9.0, 8.0, 5, 6);
    DP_CHECK (r.locked == 1); /* decision survives  */
    DP_CHECK (r.cnt == 1);    /* run survives too — unlike configure() */
    DP_CHECK (r.up_thresh == 9.0 && r.n_up == 5 && r.n_down == 6);
    /* The contrast, on the same state: configure() clears the run. */
    lockdet_configure (&r, 9.0, 8.0, 5, 6);
    DP_CHECK (r.locked == 1 && r.cnt == 0);
  }

  /* destroy(NULL) is documented as safe ("May be NULL") and was tested by
   * nothing. */
  lockdet_destroy (NULL);

  /* An INVERTED band (up_thresh < down_thresh) is documented only as advice
   * -- "choose <= up_thresh for level hysteresis" -- and is not refused. Pin
   * what it actually does, because it is the misconfiguration a caller can
   * reach and the behaviour is the opposite of hysteresis: a look between
   * the thresholds is a hit while unlocked AND a miss while locked, so with
   * unit verify counts the flag chatters every look. */
  {
    lockdet_state_t inv;
    memset (&inv, 0, sizeof inv);
    lockdet_init (&inv, 1.0, 2.0, 1, 1);      /* up < down: inverted */
    DP_CHECK (lockdet_step (&inv, 1.5) == 1); /* mid-band declares   */
    DP_CHECK (lockdet_step (&inv, 1.5) == 0); /* ...and immediately drops */
    DP_CHECK (lockdet_step (&inv, 1.5) == 1); /* chatter, every look */
  }

  /* A NON-FINITE look is a miss in BOTH states — an unknown lock is not a
   * lock (util_core.h:71). NaN fails every comparison, so which way it falls
   * is decided by how the predicate is spelled, not by the arithmetic: the
   * drop side reads `!(x >= down_thresh)` for exactly this reason. Before
   * that, a locked detector fed NaN treated it as a HIT, reset the drop run
   * every look, and held the lock lit forever on a dead metric. */
  {
    lockdet_state_t nand;
    memset (&nand, 0, sizeof nand);
    lockdet_init (&nand, 1.5, 1.2, 2, 3);

    /* Unlocked: never declares, however many looks arrive. */
    for (int i = 0; i < 10; i++)
      DP_CHECK (lockdet_step (&nand, NAN) == 0);
    DP_CHECK (nand.cnt == 0);

    /* Locked: drops after exactly n_down, like any other miss. */
    DP_CHECK (lockdet_step (&nand, 2.0) == 0);
    DP_CHECK (lockdet_step (&nand, 2.0) == 1); /* declared */
    DP_CHECK (lockdet_step (&nand, NAN) == 1); /* miss 1 of 3 */
    DP_CHECK (lockdet_step (&nand, NAN) == 1); /* miss 2 of 3 */
    DP_CHECK (lockdet_step (&nand, NAN) == 0); /* miss 3 of 3 -> dropped */

    /* Infinities are ordinary looks, not a special case: +inf is a hit,
       -inf a miss. Only NaN is unordered. */
    lockdet_reset (&nand);
    DP_CHECK (lockdet_step (&nand, INFINITY) == 0);
    DP_CHECK (lockdet_step (&nand, INFINITY) == 1);
    DP_CHECK (lockdet_step (&nand, -INFINITY) == 1);
    DP_CHECK (lockdet_step (&nand, -INFINITY) == 1);
    DP_CHECK (lockdet_step (&nand, -INFINITY) == 0);

    /* The finite edge is UNCHANGED by the new spelling: x == down_thresh is
       still not a miss. This is what makes the rewrite safe. */
    lockdet_reset (&nand);
    (void)lockdet_step (&nand, 2.0);
    DP_CHECK (lockdet_step (&nand, 2.0) == 1);
    DP_CHECK (lockdet_step (&nand, 1.2) == 1); /* exactly at down_thresh */
    DP_CHECK (nand.cnt == 0);                  /* no drop progress */
  }

  /* steps() carries the decision AND the in-flight run ACROSS calls — the
   * header's "frames of any size with no seam". The block above runs one
   * call only, which cannot see a seam. */
  {
    lockdet_state_t one, split;
    memset (&one, 0, sizeof one);
    memset (&split, 0, sizeof split);
    lockdet_init (&one, 1.5, 1.2, 2, 2);
    lockdet_init (&split, 1.5, 1.2, 2, 2);

    double seq[] = { 2.0, 2.0, 1.3, 1.0, 1.0, 2.0, 2.0, 1.0, 1.0, 2.0 };
    size_t n     = sizeof seq / sizeof seq[0];
    int    a[10] = { 0 }, b[10] = { 0 };

    lockdet_steps (&one, seq, a, n);
    /* Split at 3 — mid-run on purpose, so a dropped carry shows up. */
    lockdet_steps (&split, seq, b, 3);
    lockdet_steps (&split, seq + 3, b + 3, n - 3);
    for (size_t i = 0; i < n; i++)
      DP_CHECK (a[i] == b[i]);
    DP_CHECK (one.cnt == split.cnt && one.locked == split.locked);

    /* n == 0 is a no-op, not a read of x. */
    uint32_t cnt_before = split.cnt;
    lockdet_steps (&split, NULL, NULL, 0);
    DP_CHECK (split.cnt == cnt_before);
  }

  /* create() clamps the verify counts too — only init()'s clamp was pinned,
   * and they are separate entry points. */
  {
    lockdet_state_t *c0 = lockdet_create (1.5, 1.2, 0, 0);
    DP_CHECK (c0 != NULL);
    DP_CHECK (c0->n_up == 1 && c0->n_down == 1);
    lockdet_destroy (c0);
  }

  /* A restore carries CONFIGURATION, not just the decision: the snapshot is
   * the whole struct, so set_state into a differently-tuned detector
   * silently re-tunes it. Documented for an identically-built instance;
   * pinned here because a caller can reach the other case. */
  {
    lockdet_state_t *src = lockdet_create (1.5, 1.2, 3, 2);
    (void)lockdet_step (src, 2.0);
    unsigned char blob[64];
    lockdet_get_state (src, blob);

    lockdet_state_t *dst = lockdet_create (99.0, 98.0, 7, 9);
    DP_CHECK (lockdet_set_state (dst, blob) == DP_OK);
    DP_CHECK (dst->up_thresh == 1.5 && dst->down_thresh == 1.2);
    DP_CHECK (dst->n_up == 3 && dst->n_down == 2);
    lockdet_destroy (src);
    lockdet_destroy (dst);
  }

  DP_TEST_END ("test_lockdet_core");
}
