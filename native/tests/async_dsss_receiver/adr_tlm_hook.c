/*
 * adr_tlm_hook.c -- the counted, failable stand-ins test_adr_telemetry_oom
 * routes async_dsss_receiver_core.c's telemetry allocations through. The
 * target renames dp_tlm_create and dp_correlator_set_telemetry for that test's
 * own translation unit only, so this file, compiled apart with the renames
 * undone, calls the real ones.
 *
 * Two counters, one per stand-in: the Nth call of either can be armed to
 * fail. A failed dp_tlm_create returns NULL, as an allocation failure does.
 * A failed dp_correlator_set_telemetry returns DP_ERR_MEMORY having registered
 * nothing, which is what the real attach does when its own scratch context
 * cannot be allocated: it checks every name on a scratch before it writes
 * to the caller's table.
 */
#include "doppler/correlator/correlator_core.h"
#include "doppler/dp_tlm/dp_tlm_core.h"

int g_tlm_calls;   /* dp_tlm_create calls since the last arm */
int g_tlm_fail_at; /* the call that fails; 0 = none */
int g_acq_calls; /* dp_correlator_set_telemetry attaches since the last arm */
int g_acq_fail_at; /* the call that fails; 0 = none */
int g_fired;       /* an armed failure happened */

dp_tlm_t *
adr_test_tlm_create (size_t ring_records)
{
  g_tlm_calls++;
  if (g_tlm_fail_at && g_tlm_calls == g_tlm_fail_at)
    {
      g_fired = 1;
      return NULL;
    }
  return dp_tlm_create (ring_records);
}

/* Counts ATTACHES only: a detach is the same call with a NULL context, it
   allocates nothing, and the receiver ignores its return. */
int
adr_test_acq_set_telemetry (dp_correlator_state_t *state, dp_tlm_t *tlm,
                            const char *prefix, uint32_t decim)
{
  if (!tlm)
    return dp_correlator_set_telemetry (state, tlm, prefix, decim, NULL);
  g_acq_calls++;
  if (g_acq_fail_at && g_acq_calls == g_acq_fail_at)
    {
      g_fired = 1;
      return DP_ERR_MEMORY;
    }
  return dp_correlator_set_telemetry (state, tlm, prefix, decim, NULL);
}
