#include "lockdet/lockdet_core.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

int
main (void)
{
  int _fails = 0;

  /* create stores config; counts of 0 clamp to 1. */
  lockdet_state_t *d = lockdet_create (1.5, 1.2, 3, 2);
  CHECK (d != NULL);
  if (!d)
    return 1;
  CHECK (d->up_thresh == 1.5 && d->down_thresh == 1.2);
  CHECK (d->n_up == 3 && d->n_down == 2);
  CHECK (d->cnt == 0 && d->locked == 0);

  lockdet_state_t z;
  memset (&z, 0, sizeof z);
  lockdet_init (&z, 1.0, 1.0, 0, 0);
  CHECK (z.n_up == 1 && z.n_down == 1);

  /* Declare exactly at n_up consecutive hits — not one look sooner. */
  CHECK (lockdet_step (d, 2.0) == 0);
  CHECK (lockdet_step (d, 2.0) == 0);
  CHECK (d->cnt == 2);
  CHECK (lockdet_step (d, 2.0) == 1);
  CHECK (d->cnt == 0); /* run consumed by the declare */

  /* Locked: a metric in the [down, up] hysteresis band is sticky, and any
   * hit resets an in-flight drop run. */
  CHECK (lockdet_step (d, 1.3) == 1); /* band: no drop progress   */
  CHECK (lockdet_step (d, 1.0) == 1); /* miss 1 of 2              */
  CHECK (d->cnt == 1);
  CHECK (lockdet_step (d, 1.3) == 1); /* hit resets the drop run  */
  CHECK (d->cnt == 0);
  CHECK (lockdet_step (d, 1.0) == 1); /* miss 1 of 2 (fresh run)  */
  CHECK (lockdet_step (d, 1.0) == 0); /* miss 2 of 2 -> dropped   */
  CHECK (d->cnt == 0);

  /* Unlocked: a single miss resets the declare run (consecutive, not
   * cumulative); a band metric is a miss on the way up. */
  CHECK (lockdet_step (d, 2.0) == 0);
  CHECK (lockdet_step (d, 2.0) == 0);
  CHECK (lockdet_step (d, 1.3) == 0); /* band = miss while unlocked */
  CHECK (d->cnt == 0);
  CHECK (lockdet_step (d, 2.0) == 0);
  CHECK (lockdet_step (d, 2.0) == 0);
  CHECK (lockdet_step (d, 2.0) == 1);

  /* Threshold edges are exclusive: x == up_thresh is not a hit;
   * x == down_thresh is not a miss. */
  lockdet_reset (d);
  CHECK (d->locked == 0 && d->cnt == 0);
  CHECK (lockdet_step (d, 1.5) == 0);
  CHECK (d->cnt == 0);
  d->locked = 1;
  CHECK (lockdet_step (d, 1.2) == 1);
  CHECK (d->cnt == 0);

  /* configure preserves a live lock but restarts the verify run. */
  CHECK (lockdet_step (d, 1.0) == 1); /* miss 1 of 2 in flight */
  CHECK (d->cnt == 1);
  lockdet_configure (d, 2.0, 1.6, 4, 4);
  CHECK (d->locked == 1 && d->cnt == 0);
  CHECK (d->up_thresh == 2.0 && d->n_up == 4 && d->n_down == 4);

  /* reset drops the lock and clears the run; config survives. */
  lockdet_reset (d);
  CHECK (d->locked == 0 && d->cnt == 0);
  CHECK (d->up_thresh == 2.0 && d->n_down == 4);

  lockdet_destroy (d);

  /* n_up = n_down = 1: no time hysteresis — first hit declares, first
   * miss drops. */
  {
    lockdet_state_t e;
    memset (&e, 0, sizeof e);
    lockdet_init (&e, 0.5, 0.5, 1, 1);
    CHECK (lockdet_step (&e, 0.6) == 1);
    CHECK (lockdet_step (&e, 0.4) == 0);
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
      CHECK (got[i] == lockdet_step (&b, seq[i]));
  }

  /* serializable state — mid-run whole-struct snapshot resumes exactly
   * (in-flight verify run included), and the envelope rejects a clobber. */
  {
    lockdet_state_t *a = lockdet_create (1.5, 1.2, 3, 2);
    CHECK (lockdet_step (a, 2.0) == 0); /* cnt = 1 of 3, mid-declare */
    unsigned char blob[64];
    CHECK (lockdet_state_bytes (a) <= sizeof blob);
    lockdet_get_state (a, blob);
    int ref1 = lockdet_step (a, 2.0);
    int ref2 = lockdet_step (a, 2.0);
    lockdet_destroy (a);

    lockdet_state_t *b = lockdet_create (1.5, 1.2, 3, 2);
    CHECK (lockdet_set_state (b, blob) == DP_OK);
    CHECK (b->cnt == 1 && b->locked == 0);
    blob[0] ^= (unsigned char)0xFF;
    CHECK (lockdet_set_state (b, blob) == DP_ERR_INVALID);
    CHECK (lockdet_step (b, 2.0) == ref1);
    CHECK (lockdet_step (b, 2.0) == ref2);
    CHECK (b->locked == 1); /* the resumed run declared on schedule */
    lockdet_destroy (b);
  }

  if (_fails)
    {
      fprintf (stderr, "test_lockdet_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_lockdet_core PASSED\n");
  return 0;
}
