/*
 * test_adr_telemetry_oom.c -- an attach that cannot allocate is
 * DP_ERR_MEMORY, and changes nothing (#2846).
 *
 * dp_async_dsss_receiver_set_telemetry first attaches every loop to a
 * scratch context it creates, then to the caller's. Three allocations can
 * fail on that path:
 *   - the scratch itself (dp_tlm_create);
 *   - the searcher's attach to the scratch, which builds a scratch of its own;
 *   - the searcher's attach to the caller's context, for the same reason.
 * Each must refuse with DP_ERR_MEMORY, not the DP_ERR_INVALID of a bad
 * argument. It must also leave everything as it was: the caller's new
 * table holds no name, and the receiver stays on its previous context,
 * under its previous prefix and decimation.
 *
 * No public call can make an allocation fail, so this test compiles
 * async_dsss_receiver_core.c into itself. The build renames dp_tlm_create
 * and dp_acq_set_telemetry for this file only (-D on the target), routing
 * them through the counted stand-ins in adr_tlm_hook.c, and fails the Nth
 * call of either. Each case runs from a detached receiver and from one
 * already attached elsewhere, and each requires the armed failure to have
 * fired, so no case can pass by never reaching its branch.
 *
 * Sabotage: drop any of the three DP_ERR_MEMORY assignments in
 * set_telemetry -> that case reads DP_ERR_INVALID, red. Attach the
 * searcher last again -> the third case leaves names on the new table,
 * red.
 */
#include "../../src/async_dsss_receiver/async_dsss_receiver_core.c"

#include "dp_test.h"

#include <string.h>

extern int g_tlm_calls, g_tlm_fail_at, g_acq_calls, g_acq_fail_at, g_fired;

static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

/* A searching receiver, so the searcher's attach is on the path (the
   construction _test_telemetry_reaches_the_search uses). */
static dp_async_dsss_receiver_state_t *
make_rx (void)
{
  return dp_async_dsss_receiver_create (
      CODE7, 7, 1.0e6, 35714.29, 4.0 * 1.0e6, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8,
      0, 0.5, 4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
}

static void
disarm (void)
{
  g_tlm_calls = g_tlm_fail_at = g_acq_calls = g_acq_fail_at = g_fired = 0;
}

typedef struct
{
  const char *name;
  int         tlm_at; /* fail this dp_tlm_create call; 0 = none */
  int         acq_at; /* fail this dp_acq_set_telemetry call; 0 = none */
} oom_case_t;

static const oom_case_t CASES[] = {
  { "the receiver's scratch", 1, 0 },
  { "the searcher's attach to the scratch", 0, 1 },
  { "the searcher's attach to the caller's context", 0, 2 },
};

/* One case from one starting state: attached (to `old`, prefix "rx",
   decimation 1) or detached. Returns 0, or 1 when a DP_REQUIRE stopped it. */
static int
run_case (const oom_case_t *c, int attached)
{
  dp_async_dsss_receiver_state_t *rx  = make_rx ();
  dp_tlm_t                       *old = dp_tlm_create (1 << 12);
  dp_tlm_t                       *t2  = dp_tlm_create (1 << 12);
  DP_REQUIRE (rx && rx->acq && old && t2);
  disarm ();
  if (attached)
    DP_REQUIRE (dp_async_dsss_receiver_set_telemetry (rx, old, "rx", 1)
                == DP_OK);
  const size_t   old_probes = dp_tlm_probe_count (old);
  dp_tlm_t      *was_tlm    = rx->tlm;
  void          *was_acq    = rx->acq->tlm.ctx;
  const uint32_t was_decim  = rx->tlm_decim;
  char           was_prefix[DP_TLM_NAME_MAX];
  memcpy (was_prefix, rx->tlm_prefix, sizeof was_prefix);

  disarm ();
  g_tlm_fail_at = c->tlm_at;
  g_acq_fail_at = c->acq_at;
  const int rc  = dp_async_dsss_receiver_set_telemetry (rx, t2, "new", 2);
  const int hit = g_fired;
  disarm ();

  DP_REQUIRE_MSG (hit, c->name);
  DP_CHECK_MSG (rc == DP_ERR_MEMORY, c->name);
  DP_CHECK_MSG (dp_tlm_probe_count (t2) == 0, c->name);
  DP_CHECK_MSG (rx->tlm == was_tlm && rx->acq->tlm.ctx == was_acq, c->name);
  DP_CHECK_MSG (rx->tlm_decim == was_decim
                    && memcmp (rx->tlm_prefix, was_prefix, sizeof was_prefix)
                           == 0,
                c->name);
  DP_CHECK_MSG (dp_tlm_probe_count (old) == old_probes, c->name);
  /* And the same attach, unarmed, succeeds: nothing was left half-done. */
  DP_CHECK_MSG (dp_async_dsss_receiver_set_telemetry (rx, t2, "new", 2)
                    == DP_OK,
                c->name);
  DP_CHECK_MSG (dp_tlm_probe_count (t2) > 0, c->name);

  DP_CHECK (dp_async_dsss_receiver_set_telemetry (rx, NULL, NULL, 1) == DP_OK);
  dp_async_dsss_receiver_destroy (rx);
  dp_tlm_destroy (t2);
  dp_tlm_destroy (old);
  return 0;
}

int
main (void)
{
  for (size_t i = 0; i < sizeof CASES / sizeof *CASES; i++)
    for (int attached = 0; attached <= 1; attached++)
      DP_CHECK_MSG (run_case (&CASES[i], attached) == 0, CASES[i].name);
  DP_TEST_END ("test_adr_telemetry_oom");
}
