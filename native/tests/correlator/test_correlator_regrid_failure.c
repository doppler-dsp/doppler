/*
 * test_correlator_regrid_failure.c -- a regrid that fails commits nothing
 * (#2761).
 *
 * dp_correlator_regrid builds every replacement first and adopts them at one
 * commit, so a plan that will not build must leave the engine as it was: the
 * old grid (n, interp, n_surf, n_noncoh, coherent_bins, window_bins, the
 * ring), the old buffers, and a sidelobe cache that is still valid for the
 * grid in effect. The per-tile scratch used to be built AFTER the commit, so
 * its failure left the new D and window_bins over the old surface, stale
 * n/interp/n_surf/n_noncoh, and a leaked ring and accumulator.
 *
 * Only the constructor reaches a widening regrid today, and a failed
 * constructor destroys the engine, so no public call can show this. This
 * test therefore compiles search_core.c into itself to call the static
 * dp_correlator_regrid on a LIVE engine, together with correlator_core.c,
 * whose one delay-Doppler map acq's regrid builds (#2752), and renames the
 * plans those two files build (a macro over dp_fft_create, for this file only)
 * to fail the Nth one. It sweeps N over EVERY plan the regrid builds -- the
 * template's transform and the map's slot plans -- and after each failure
 * requires the engine to match a fresh one: fields, buffers, and what an
 * execute returns.
 *
 * The buffers are failed too: malloc, calloc and realloc are defined over
 * counted stand-ins for the two included sources only, after every header they
 * include, so N runs over every plan AND every buffer a regrid builds.
 *
 * The carrier is the third reach (#2831): it is set through a regrid that
 * rebuilds the map, from dp_correlator_set_carrier_freq_hz and from set_state
 * for a blob at another carrier. Swept over every build those make, each must
 * return DP_ERR_MEMORY with the engine's grid, its map and its blob's bytes
 * unchanged.
 *
 * The same reach serves a second contract: a regrid re-applies the caller's
 * thread request at the new tile count, so narrowing to one tile and
 * widening again comes back to min(request, tiles) (#2797).
 */
#include "doppler/fft/fft_core.h"

/* search_fft_hook.c: the counted, failable stand-ins for the plan builder and
   the allocators. The build renames dp_fft_create to one for THIS file only
   (-D on the target), and the defines below route the two sources' malloc
   and calloc to the others, which is what puts acq's and the map's builds,
   and nothing the headers' inline helpers allocate, under the counter. */
extern int   g_calls;   /* builds since the last arm */
extern int   g_fail_at; /* the build that fails; 0 = none */
extern void *search_test_malloc (size_t n);
extern void *search_test_calloc (size_t n, size_t size);
extern void *search_test_realloc (void *p, size_t n);

/* Every header the two sources include, first, so their include guards
   keep the defines below out of every header. */
#include "../../src/correlator/correlator_map.h"
#include "doppler/clib_common.h"
#include "doppler/correlator/correlator_core.h"
#include "doppler/correlator/correlator_private.h"
#include "doppler/dp_complex.h"
#include "doppler/util/util_core.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define malloc(n) search_test_malloc (n)
#define calloc(n, size) search_test_calloc (n, size)
#define realloc(p, n) search_test_realloc (p, n)
#include "../../src/correlator/correlator_core.c"
#undef malloc
#undef calloc
#undef realloc

#include "dp_preamble_test.h"
#include "dp_test.h"

static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };
#define SPC 2

static dp_correlator_state_t *
make_engine (void)
{
  float _Complex        *pre = dp_code_preamble (CODE7, 7, SPC);
  dp_correlator_state_t *a   = dp_correlator_create_burst (
      pre, 7 * SPC, 8, 1.0e6 * SPC, 45.0, 0.0, 1e-2, 0.9, 0, 0.0, 1);
  free (pre);
  return a;
}

/* What an execute returns: the hits over a fixed stream. */
static size_t
run_hits (dp_correlator_state_t *a, dp_correlator_hit_t *h, size_t cap)
{
  float _Complex *pre = dp_code_preamble (CODE7, 7, SPC);
  const size_t    L   = 3 * a->search->n + 5;
  float _Complex *s   = malloc (L * sizeof *s);
  for (size_t k = 0; k < L; k++)
    s[k] = pre[k % (7 * SPC)];
  free (pre);
  size_t nh = dp_correlator_run (a, NULL, NULL, s, L, h, cap);
  free (s);
  return nh;
}

/* Every member is 8 bytes, so the struct has no padding and memcmp
   compares exactly the fields. */
typedef struct
{
  size_t n, interp, n_surf, n_noncoh, coherent_bins, window_bins, ring_cap,
      frame_n, sl_cn, sl_cnext, map_f;
  void *map, *ref, *parts, *ring, *nc_surface, *ref_spec, *map_work,
      *map_tiles, *map_spec, *map_rows, *map_off, *map_wt, *blk_raw;
  double carrier_freq_hz;
} snap_t;

static snap_t
snap (const dp_correlator_state_t *s)
{
  snap_t t = { .n               = s->search->n,
               .interp          = s->search->interp,
               .n_surf          = s->search->n_surf,
               .n_noncoh        = s->search->n_noncoh,
               .coherent_bins   = s->search->coherent_bins,
               .window_bins     = s->search->window_bins,
               .ring_cap        = s->ring_cap,
               .frame_n         = s->search->frame_n,
               .sl_cn           = s->search->sl_cn,
               .sl_cnext        = s->search->sl_cnext,
               .map_f           = s->search->map_f,
               .map             = s->dd_map,
               .ref             = s->search->ref,
               .parts           = s->search->parts,
               .ring            = s->ring,
               .nc_surface      = s->search->nc_surface,
               .ref_spec        = s->search->ref_spec,
               .map_work        = s->search->map_work,
               .map_tiles       = s->search->map_tiles,
               .map_spec        = s->search->map_spec,
               .map_rows        = s->search->map_rows,
               .map_off         = s->search->map_off,
               .map_wt          = s->search->map_wt,
               .blk_raw         = s->search->blk_raw,
               .carrier_freq_hz = s->search->carrier_freq_hz };
  return t;
}

#define NEW_D 4
#define NEW_NC 2
#define NEW_WINDOW 3

/* A regrid re-applies the caller's thread request, capped at the new tile
   count (#2797). Only dp_correlator_regrid widens (a public narrowing exists,
   the one-tile configure_search_raw, but nothing public widens again), so this
   file is where a narrow-then-widen can be run on a live engine. Sabotage:
   drop search_apply_threads from dp_correlator_regrid, or apply `threads` in
   place of `threads_req` -> the widened engine reads 1. */
static int
threads_follow_the_request (void)
{
  const int reqs[] = { 2, 4, 1 };
  for (size_t i = 0; i < sizeof reqs / sizeof *reqs; i++)
    {
      const int n = reqs[i];
      const int w = n < NEW_WINDOW ? n : NEW_WINDOW; /* min(N, tiles) */
      dp_correlator_state_t *a = make_engine ();
      DP_REQUIRE_MSG (a && a->search->window_bins == 1, "a one-tile engine");
      DP_CHECK (dp_correlator_set_threads (a, n) == DP_OK);
      DP_CHECK (a->search->threads
                == 1); /* one tile: serial, whatever asked */
      DP_REQUIRE_MSG (dp_correlator_regrid (a, NEW_D, NEW_NC, NEW_WINDOW, NULL,
                                            a->search->carrier_freq_hz)
                          == 0,
                      "widen");
      DP_CHECK (a->search->threads == w);
      DP_REQUIRE_MSG (dp_correlator_regrid (a, NEW_D, NEW_NC, 1, NULL,
                                            a->search->carrier_freq_hz)
                          == 0,
                      "narrow");
      DP_CHECK (a->search->threads == 1);
      DP_CHECK_MSG (a->pool == NULL, "one tile runs without a pool");
      DP_REQUIRE_MSG (dp_correlator_regrid (a, NEW_D, NEW_NC, NEW_WINDOW, NULL,
                                            a->search->carrier_freq_hz)
                          == 0,
                      "widen again");
      DP_CHECK (a->search->threads == w);
      DP_CHECK_MSG ((a->pool != NULL) == (w > 1),
                    "a pool exactly when fanned");
      DP_CHECK (a->search->threads_req == n);
      dp__search_destroy (a);
    }
  return 0;
}

/* Two carriers above fs/2 = 2 MHz (dp_correlator_carrier_freq_ok), and none.
 */
#define FC1 1.5e9
#define FC2 2.2e9

static const uint8_t CODE31[31]
    = { 1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1,
        1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0 };

/* A tiled engine at carrier @p fc, built the public way (a continuous
   engine whose span needs three tiles, test_correlator_search.c's carrier
   check), then pushed 1.5 blocks and 3 samples: mid-block and mid-frame, so
   the blob has a live blk_raw and carry for a failure to disturb. It is not
   widened with dp_correlator_regrid alone: the framer is bound by
   dp__search_reset, which every public regrid that changes the frame calls
   after it, so a bare widening leaves the framer at the old frame. */
static dp_correlator_state_t *
make_tiled (double fc)
{
  const double           span31 = 1.0e6 / (2.0 * 31.0);
  dp_correlator_state_t *a
      = dp_correlator_create_continuous (CODE31, 31, 4.0e6, 1.0e6, 0.0, 75.0,
                                         3.5 * span31, 1e-3, 0.9, 0, 31, 0.0);
  if (!a || dp_correlator_set_carrier_freq_hz (a, fc) != DP_OK)
    {
      dp__search_destroy (a);
      return NULL;
    }
  const size_t    D = a->search->coherent_bins, N = a->search->code_bins;
  const size_t    L = D * N + (D / 2) * N + 3;
  float _Complex *x = malloc (L * sizeof *x);
  for (size_t k = 0; k < L; k++)
    x[k] = CODE31[(k / 4) % 31] ? 1.0f : -1.0f;
  dp_correlator_hit_t h[8];
  (void)dp__search_push (a, x, L, h, 8);
  free (x);
  return a;
}

/* The hits two engines find on the same continuation of make_tiled's
   stream: two more blocks, so both decide at least one dwell. */
static int
same_hits (dp_correlator_state_t *a, dp_correlator_state_t *b)
{
  const size_t    D = a->search->coherent_bins, N = a->search->code_bins;
  const size_t    L = 2 * D * N;
  float _Complex *x = malloc (L * sizeof *x);
  for (size_t k = 0; k < L; k++)
    x[k] = CODE31[((k + 7) / 4) % 31] ? 1.0f : -1.0f;
  dp_correlator_hit_t ha[16], hb[16];
  const size_t        na = dp__search_push (a, x, L, ha, 16);
  const size_t        nb = dp__search_push (b, x, L, hb, 16);
  free (x);
  int same = na == nb && na > 0; /* no hits would compare nothing */
  for (size_t i = 0; same && i < na; i++)
    same = ha[i].row == hb[i].row && ha[i].col == hb[i].col
           && ha[i].test_stat == hb[i].test_stat
           && ha[i].peak_mag == hb[i].peak_mag;
  return same;
}

/* The carrier's two entries, each with every build it makes failed in
   turn. A build before the commit refuses the change: DP_ERR_MEMORY, and
   the engine's grid and map, and its blob byte for byte, stay as they
   were. A build after it is the map's worker-slot reserve, whose failure
   costs speed and never a result (dp__dd_map_reserve), so the change then
   succeeds with exactly the blob the unarmed change gives, and finds the
   same hits on the same stream after it. `via_state`
   restores a blob taken at @p to (set_state's rebuild); otherwise
   dp_correlator_set_carrier_freq_hz (@p to). Sabotage: let
   dp_correlator_regrid commit the carrier (or map_f) before its last build, or
   let set_state write the framer before its rebuild -> red. */
static int
carrier_failure_commits_nothing (double from, double to, int via_state)
{
  dp_correlator_state_t *src = via_state ? make_tiled (to) : NULL;
  void                  *in  = NULL;
  if (via_state)
    {
      DP_REQUIRE_MSG (src, "the blob's engine");
      in = malloc (dp__search_state_bytes (src));
      dp__search_get_state (src, in);
    }

  /* How many builds a successful change makes. */
  dp_correlator_state_t *probe = make_tiled (from);
  DP_REQUIRE_MSG (
      probe && probe->search->window_bins > 1 && probe->search->blk_epoch > 0
          && probe->search->blk_epoch < probe->search->coherent_bins,
      "a tiled engine, mid-block");
  g_calls         = 0;
  g_fail_at       = 0;
  const int ok    = via_state ? dp__search_set_state (probe, in)
                              : dp_correlator_set_carrier_freq_hz (probe, to);
  const int total = g_calls;
  DP_REQUIRE_MSG (ok == DP_OK && probe->search->carrier_freq_hz == to,
                  "the change succeeds unarmed");
  /* At least the map's slot plans and its buffers. */
  DP_REQUIRE_MSG (total >= 5, "and counted every build it made");
  const size_t nwant = dp__search_state_bytes (probe);
  void        *want  = malloc (nwant);
  dp__search_get_state (probe, want);
  dp__search_destroy (probe);

  int refused = 0, degraded = 0;

  for (int k = 1; k <= total; k++)
    {
      dp_correlator_state_t *live = make_tiled (from);
      DP_REQUIRE_MSG (live, "engine");
      const size_t nb     = dp__search_state_bytes (live);
      void        *before = malloc (nb), *after = malloc (nb);
      dp__search_get_state (live, before);
      const snap_t s0 = snap (live);
      g_calls         = 0;
      g_fail_at       = k;
      const int rc = via_state ? dp__search_set_state (live, in)
                               : dp_correlator_set_carrier_freq_hz (live, to);
      g_fail_at    = 0;
      DP_CHECK_MSG (rc == DP_ERR_MEMORY || rc == DP_OK,
                    "the Nth build failing refuses the change or costs "
                    "nothing");
      if (rc == DP_ERR_MEMORY)
        {
          refused++;
          const snap_t s1 = snap (live);
          DP_CHECK_MSG (memcmp (&s0, &s1, sizeof s0) == 0,
                        "the grid, the carrier and the map are as they "
                        "were");
          DP_CHECK_MSG (dp__search_state_bytes (live) == nb,
                        "the blob's size too");
          dp__search_get_state (live, after);
          DP_CHECK_MSG (memcmp (before, after, nb) == 0,
                        "and the blob is byte for byte the one before");
        }
      else
        {
          DP_CHECK_MSG (dp__search_state_bytes (live) == nwant,
                        "a degraded reserve changes no blob size");
          void *got = malloc (nwant);
          dp__search_get_state (live, got);
          DP_CHECK_MSG (memcmp (got, want, nwant) == 0,
                        "a degraded reserve gives the unarmed change's "
                        "blob");
          free (got);
          dp_correlator_state_t *ref = make_tiled (from);
          DP_REQUIRE_MSG (ref, "the reference engine");
          DP_REQUIRE (via_state ? dp__search_set_state (ref, in) == DP_OK
                                : dp_correlator_set_carrier_freq_hz (ref, to)
                                      == DP_OK);
          DP_CHECK_MSG (same_hits (live, ref),
                        "and the hits the unarmed change finds");
          dp__search_destroy (ref);
          degraded++;
        }
      free (before);
      free (after);
      dp__search_destroy (live);
    }
  /* The map's plans and buffers are built before the commit, so some
     failure refuses: the sweep reaches the path it is here for. */
  DP_CHECK_MSG (refused > 0, "some build failing refuses the change");
  /* A change that alters the map reserves slots after the commit: the
     degraded branch runs too, or its checks are vacuous. */
  DP_CHECK_MSG (degraded > 0, "and some degrades it");
  free (want);
  free (in);
  dp__search_destroy (src);
  return 0;
}

int
main (void)
{
  DP_CHECK_MSG (threads_follow_the_request () == 0,
                "a regrid re-applies the thread request");

  /* The carrier sweep: on, moved, off, through both entries. */
  const double pairs[][2] = { { 0.0, FC1 }, { FC1, FC2 }, { FC1, 0.0 } };
  for (size_t i = 0; i < sizeof pairs / sizeof *pairs; i++)
    for (int via_state = 0; via_state <= 1; via_state++)
      DP_CHECK_MSG (
          carrier_failure_commits_nothing (pairs[i][0], pairs[i][1], via_state)
              == 0,
          "a failed carrier change commits nothing");

  /* How many plans and buffers a successful widening regrid builds. */
  dp_correlator_state_t *probe = make_engine ();
  DP_REQUIRE_MSG (probe, "engine");
  g_calls   = 0;
  g_fail_at = 0;
  DP_REQUIRE_MSG (
      dp_correlator_regrid (probe, NEW_D, NEW_NC, NEW_WINDOW, NULL, 0.0) == 0,
      "the widening regrid succeeds unarmed");
  const int total = g_calls;
  DP_REQUIRE_MSG (probe->search->window_bins == NEW_WINDOW && probe->dd_map,
                  "it built the map");
  /* The template's transform, then the map's one slot: forward, inverse,
     and the two slow-time plans (slow first's, rows first's) at D > 1. */
  DP_REQUIRE_MSG (total >= 5, "and counted every plan it built");
  dp__search_destroy (probe);

  dp_correlator_state_t *fresh = make_engine ();
  dp_correlator_hit_t    want[16];
  const size_t           nw = run_hits (fresh, want, 16);
  DP_REQUIRE_MSG (nw >= 1, "the stream is acquired (a real comparison)");
  dp__search_destroy (fresh);

  for (int k = 1; k <= total; k++)
    {
      dp_correlator_state_t *live = make_engine ();
      DP_REQUIRE_MSG (live, "engine");
      const snap_t before = snap (live);
      g_calls             = 0;
      g_fail_at           = k;
      const int rc
          = dp_correlator_regrid (live, NEW_D, NEW_NC, NEW_WINDOW, NULL, 0.0);
      g_fail_at = 0;
      DP_CHECK_MSG (rc == -1, "the Nth build failing fails the regrid");
      const snap_t after = snap (live);
      DP_CHECK_MSG (memcmp (&before, &after, sizeof before) == 0,
                    "a failed regrid leaves the grid and every buffer as "
                    "they were");
      dp_correlator_hit_t got[16];
      const size_t        ng   = run_hits (live, got, 16);
      int                 same = ng == nw;
      for (size_t i = 0; same && i < ng; i++)
        same = got[i].row == want[i].row && got[i].col == want[i].col
               && got[i].test_stat == want[i].test_stat;
      DP_CHECK_MSG (same, "an execute after the failed regrid matches a "
                          "fresh engine");
      dp__search_destroy (live);
    }
  DP_TEST_END ("test_correlator_regrid_failure");
}
