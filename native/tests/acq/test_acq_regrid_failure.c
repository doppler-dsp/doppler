/*
 * test_acq_regrid_failure.c -- a regrid that fails commits nothing (#2761).
 *
 * acq_regrid builds every replacement first and adopts them at one commit,
 * so a plan that will not build must leave the engine as it was: the old
 * grid (n, interp, n_surf, n_noncoh, coherent_bins, window_bins, the ring),
 * the old buffers, and a sidelobe cache that is still valid for the grid in
 * effect. The per-tile scratch used to be built AFTER the commit, so its
 * failure left the new D and window_bins over the old surface, stale
 * n/interp/n_surf/n_noncoh, and a leaked ring and accumulator.
 *
 * Only the constructor reaches a widening regrid today, and a failed
 * constructor destroys the engine, so no public call can show this. This
 * test therefore compiles acq_core.c into itself to call the static
 * acq_regrid on a LIVE engine, together with corr2d_core.c, whose one
 * delay-Doppler map acq's regrid builds (#2752), and renames the plans
 * those two files build (a macro over dp_fft_create, for this file only) to
 * fail the Nth one. It sweeps N over EVERY plan the regrid builds -- the
 * template's transform and the map's slot plans -- and after each failure
 * requires the engine to match a fresh one: fields, buffers, and what an
 * execute returns.
 *
 * The same reach serves a second contract: a regrid re-applies the caller's
 * thread request at the new tile count, so narrowing to one tile and
 * widening again comes back to min(request, tiles) (#2797).
 */
#include "doppler/fft/fft_core.h"

/* acq_fft_hook.c: the counted, failable stand-in for the plan builder. The
   build renames dp_fft_create to it for THIS file only (-D on the target),
   which is what puts acq_core.c's plans, and nothing the correlator builds,
   under the counter. */
extern int g_calls;   /* plans acq_core.c has built since the last arm */
extern int g_fail_at; /* the call that fails; 0 = none */

#include "../../src/acq/acq_core.c"
#include "../../src/corr2d/corr2d_core.c"

#include "dp_preamble_test.h"
#include "dp_test.h"

static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };
#define SPC 2

static dp_acq_state_t *
make_engine (void)
{
  float _Complex *pre = dp_code_preamble (CODE7, 7, SPC);
  dp_acq_state_t *a = dp_acq_create_burst (pre, 7 * SPC, 8, 1.0e6 * SPC, 45.0,
                                           0.0, 1e-2, 0.9, 0, 0.0, 1);
  free (pre);
  return a;
}

/* What an execute returns: the hits over a fixed stream. */
static size_t
run_hits (dp_acq_state_t *a, acq_result_t *h, size_t cap)
{
  float _Complex *pre = dp_code_preamble (CODE7, 7, SPC);
  const size_t    L   = 3 * a->n + 5;
  float _Complex *s   = malloc (L * sizeof *s);
  for (size_t k = 0; k < L; k++)
    s[k] = pre[k % (7 * SPC)];
  free (pre);
  size_t nh = dp_acq_run (a, NULL, NULL, s, L, h, cap);
  free (s);
  return nh;
}

typedef struct
{
  size_t n, interp, n_surf, n_noncoh, coherent_bins, window_bins, ring_cap,
      frame_n, sl_cn, sl_cnext;
  void *map, *ref, *parts, *ring, *nc_surface, *ref_spec, *map_work,
      *map_tiles;
} snap_t;

static snap_t
snap (const dp_acq_state_t *s)
{
  snap_t t = { s->n,        s->interp,        s->n_surf,
               s->n_noncoh, s->coherent_bins, s->window_bins,
               s->ring_cap, s->frame_n,       s->sl_cn,
               s->sl_cnext, s->map,           s->ref,
               s->parts,    s->ring,          s->nc_surface,
               s->ref_spec, s->map_work,      s->map_tiles };
  return t;
}

#define NEW_D 4
#define NEW_NC 2
#define NEW_WINDOW 3

/* A regrid re-applies the caller's thread request, capped at the new tile
   count (#2797). Only acq_regrid widens (a public narrowing exists, the
   one-tile configure_search_raw, but nothing public widens again), so this
   file is where a narrow-then-widen can be run on a live engine. Sabotage:
   drop acq_apply_threads from acq_regrid, or apply `threads` in place of
   `threads_req` -> the widened engine reads 1. */
static int
threads_follow_the_request (void)
{
  const int reqs[] = { 2, 4, 1 };
  for (size_t i = 0; i < sizeof reqs / sizeof *reqs; i++)
    {
      const int       n = reqs[i];
      const int       w = n < NEW_WINDOW ? n : NEW_WINDOW; /* min(N, tiles) */
      dp_acq_state_t *a = make_engine ();
      DP_REQUIRE_MSG (a && a->window_bins == 1, "a one-tile engine");
      DP_CHECK (dp_acq_set_threads (a, n) == DP_OK);
      DP_CHECK (a->threads == 1); /* one tile: serial, whatever asked */
      DP_REQUIRE_MSG (
          acq_regrid (a, NEW_D, NEW_NC, NEW_WINDOW, NULL, a->carrier_freq_hz)
              == 0,
          "widen");
      DP_CHECK (a->threads == w);
      DP_REQUIRE_MSG (
          acq_regrid (a, NEW_D, NEW_NC, 1, NULL, a->carrier_freq_hz) == 0,
          "narrow");
      DP_CHECK (a->threads == 1);
      DP_CHECK_MSG (a->pool == NULL, "one tile runs without a pool");
      DP_REQUIRE_MSG (
          acq_regrid (a, NEW_D, NEW_NC, NEW_WINDOW, NULL, a->carrier_freq_hz)
              == 0,
          "widen again");
      DP_CHECK (a->threads == w);
      DP_CHECK_MSG ((a->pool != NULL) == (w > 1),
                    "a pool exactly when fanned");
      DP_CHECK (a->threads_req == n);
      dp_acq_destroy (a);
    }
  return 0;
}

int
main (void)
{
  DP_CHECK_MSG (threads_follow_the_request () == 0,
                "a regrid re-applies the thread request");

  /* How many plans a successful widening regrid builds. */
  dp_acq_state_t *probe = make_engine ();
  DP_REQUIRE_MSG (probe, "engine");
  g_calls   = 0;
  g_fail_at = 0;
  DP_REQUIRE_MSG (acq_regrid (probe, NEW_D, NEW_NC, NEW_WINDOW, NULL, 0.0)
                      == 0,
                  "the widening regrid succeeds unarmed");
  const int total = g_calls;
  DP_REQUIRE_MSG (probe->window_bins == NEW_WINDOW && probe->map,
                  "it built the map");
  /* The template's transform, then the map's one slot: forward, inverse,
     and the two slow-time plans (slow first's, rows first's) at D > 1. */
  DP_REQUIRE_MSG (total >= 5, "and counted every plan it built");
  dp_acq_destroy (probe);

  dp_acq_state_t *fresh = make_engine ();
  acq_result_t    want[16];
  const size_t    nw = run_hits (fresh, want, 16);
  DP_REQUIRE_MSG (nw >= 1, "the stream is acquired (a real comparison)");
  dp_acq_destroy (fresh);

  for (int k = 1; k <= total; k++)
    {
      dp_acq_state_t *live = make_engine ();
      DP_REQUIRE_MSG (live, "engine");
      const snap_t before = snap (live);
      g_calls             = 0;
      g_fail_at           = k;
      const int rc = acq_regrid (live, NEW_D, NEW_NC, NEW_WINDOW, NULL, 0.0);
      g_fail_at    = 0;
      DP_CHECK_MSG (rc == -1, "the Nth plan failing fails the regrid");
      const snap_t after = snap (live);
      DP_CHECK_MSG (memcmp (&before, &after, sizeof before) == 0,
                    "a failed regrid leaves the grid and every buffer as "
                    "they were");
      acq_result_t got[16];
      const size_t ng   = run_hits (live, got, 16);
      int          same = ng == nw;
      for (size_t i = 0; same && i < ng; i++)
        same = got[i].doppler_bin == want[i].doppler_bin
               && got[i].code_phase == want[i].code_phase
               && got[i].test_stat == want[i].test_stat;
      DP_CHECK_MSG (same, "an execute after the failed regrid matches a "
                          "fresh engine");
      dp_acq_destroy (live);
    }
  DP_TEST_END ("test_acq_regrid_failure");
}
