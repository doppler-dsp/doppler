

# File lockdet\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**lockdet**](dir_87531a87e500e672b7d093c5682794b4.md) **>** [**lockdet\_core.h**](lockdet__core_8h.md)

[Go to the documentation of this file](lockdet__core_8h.md)


```C++

#ifndef LOCKDET_CORE_H
#define LOCKDET_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include "util/util_core.h" /* saturate() — the NaN policy, shared */
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    double up_thresh;   
    double down_thresh; 
    uint32_t n_up;      
    uint32_t n_down;    
    uint32_t cnt;       
    int locked;         
  } lockdet_state_t;

  void lockdet_init(lockdet_state_t *state, double up_thresh,
                    double down_thresh, uint32_t n_up, uint32_t n_down);

  lockdet_state_t *lockdet_create(double up_thresh, double down_thresh,
                                  uint32_t n_up, uint32_t n_down);

  void lockdet_destroy(lockdet_state_t *state);

  void lockdet_configure(lockdet_state_t *state, double up_thresh,
                         double down_thresh, uint32_t n_up, uint32_t n_down);

  void lockdet_reset(lockdet_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Whole-struct POD snapshot (pointer-free); the decision flag and the
   * in-flight verify run resume exactly.
   */
#define LOCKDET_STATE_MAGIC DP_FOURCC('L', 'K', 'D', 'T')
#define LOCKDET_STATE_VERSION 1u

  size_t lockdet_state_bytes(const lockdet_state_t *state);
  void lockdet_get_state(const lockdet_state_t *state, void *blob);
  int lockdet_set_state(lockdet_state_t *state, const void *blob);

  JM_FORCEINLINE JM_HOT int
  lockdet_step (lockdet_state_t *state, double x)
  {
    /* An unknown lock is not a lock. Send a non-finite look to the floor
       through the SHARED primitive rather than encoding the policy here:
       saturate()'s own documentation names a lock statistic as the caller
       that wants NaN at the floor, and until now no lock detector called
       it, so that paragraph described a caller who did not exist.
       Doing the substitution once, up front, is also what keeps the two
       comparisons below plain. NaN fails every comparison, so a detector
       that handles it inline has to encode the policy in the SPELLING of a
       predicate (`!(x >= t)` rather than `x < t`) — which is subtle enough
       that the drop side was written the other way and held the lock lit
       forever on a dead metric.
       The bounds are infinite because the substitution is the only job:
       every finite look, and both infinities, pass through untouched. */
    x = saturate (x, -INFINITY, INFINITY, -INFINITY);

    if (!state->locked)
      {
        if (x > state->up_thresh)
          {
            if (++state->cnt >= state->n_up)
              {
                state->locked = 1;
                state->cnt    = 0;
              }
          }
        else
          state->cnt = 0;
      }
    else
      {
        if (x < state->down_thresh)
          {
            if (++state->cnt >= state->n_down)
              {
                state->locked = 0;
                state->cnt    = 0;
              }
          }
        else
          state->cnt = 0;
      }
    return state->locked;
  }

  void lockdet_steps (lockdet_state_t *state, const double *x, int *out,
                      size_t n);

#ifdef __cplusplus
}
#endif

#endif /* LOCKDET_CORE_H */
```


