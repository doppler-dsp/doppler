/*
 * test_wfm_plan.c — component-cache stimulus engine (wfm_plan).
 *
 * The contract is bit-exactness against a full compose. Every scene is built
 * as segments, serialized with wfm_spec_to_json(), and fed to BOTH
 * wfm_compose_from_json() (the reference) and wfm_plan_prepare() (the cache),
 * so they share one parse+resolve path. Gate-0 is render("{}") ≡ compose; the
 * per-axis tests re-materialize a variation and memcmp it against a full
 * compose of the equivalently-modified spec.
 */
#define _GNU_SOURCE
#include "wfm/wfm_compose.h"
#include "wfm/wfm_plan.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define L 1024u /* samples per scene (qpsk sps=8 → 128 symbols) */

#define CHECK(cond, msg)                                                      \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL: %s\n", msg);                                \
          return 1;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/* Serialize a 2-source scene (qpsk anchor @ snr + a clean tone) to spec JSON.
 * qpsk carries the SNR (the noise anchor, seed 7); the tone is clean (seed 3).
 * Caller free()s the returned string. */
static char *
scene_json (double qpsk_snr, double qpsk_level, double tone_level)
{
  wfm_source_t  qpsk   = { .type      = 4, /* WFM_SYNTH_QPSK */
                           .freq      = 0.0,
                           .snr       = qpsk_snr,
                           .snr_mode  = 0,
                           .seed      = 7,
                           .sps       = 8,
                           .pn_length = 7,
                           .level     = qpsk_level };
  wfm_source_t  tone   = { .type      = 0, /* WFM_SYNTH_TONE */
                           .freq      = 1e5,
                           .snr       = 100.0, /* clean */
                           .snr_mode  = 0,
                           .seed      = 3,
                           .sps       = 8,
                           .pn_length = 7,
                           .level     = tone_level };
  wfm_source_t  two[2] = { qpsk, tone };
  wfm_segment_t seg    = { .sources     = two,
                           .n_sources   = 2,
                           .fs          = 1e6,
                           .num_samples = L,
                           .off_samples = 0 };
  return wfm_spec_to_json (&seg, 1, 0, 0, 0.0);
}

/* Full-compose a spec into out[L]; returns the sample count collected. */
static size_t
compose_collect (const char *json, float _Complex *out)
{
  wfm_compose_state_t *c = wfm_compose_from_json (json);
  if (!c)
    return 0;
  size_t        total = 0, n;
  float complex buf[257];
  while ((n = wfm_compose_execute (c, buf, 257)) > 0 && total + n <= L)
    {
      memcpy (out + total, buf, n * sizeof *buf);
      total += n;
    }
  wfm_compose_destroy (c);
  return total;
}

static int
any_diff (const float _Complex *a, const float _Complex *b)
{
  for (size_t i = 0; i < L; i++)
    if (a[i] != b[i])
      return 1;
  return 0;
}

int
main (void)
{
  size_t          bytes = L * sizeof (float _Complex);
  float _Complex *ref   = malloc (bytes);
  float _Complex *got   = malloc (bytes);
  float _Complex *base  = malloc (bytes);
  CHECK (ref && got && base, "alloc");

  char *json = scene_json (12.0, 0.0, 0.0);
  CHECK (json, "scene_json");
  wfm_plan_t *p = wfm_plan_prepare (json);
  CHECK (p, "prepare");
  CHECK (wfm_plan_len (p) == L, "len");
  CHECK (wfm_plan_n_sources (p) == 2, "n_sources (signals only)");
  CHECK (wfm_plan_anchor_seed (p) == 7, "anchor seed = qpsk seed");

  /* ── Gate-0: render("{}") ≡ full compose, bit-for-bit ── */
  CHECK (compose_collect (json, ref) == L, "compose baseline");
  CHECK (wfm_plan_render (p, "{}", base) == L, "render baseline");
  CHECK (memcmp (ref, base, bytes) == 0, "GATE-0: render({}) == compose");
  /* NULL overrides is the same as an empty object. */
  CHECK (wfm_plan_render (p, NULL, got) == L, "render NULL");
  CHECK (memcmp (base, got, bytes) == 0, "render(NULL) == render({})");

  /* ── SNR axis: at(6, anchor_seed) ≡ compose(scene @ snr=6) ── */
  char *json6 = scene_json (6.0, 0.0, 0.0);
  CHECK (compose_collect (json6, ref) == L, "compose snr=6");
  CHECK (wfm_plan_at (p, 6.0, wfm_plan_anchor_seed (p), got) == L, "at(6)");
  CHECK (memcmp (ref, got, bytes) == 0, "SNR: at(6,anchor) == compose@6");
  /* render('{"snr":6}') takes the same path. */
  CHECK (wfm_plan_render (p, "{\"snr\":6.0}", got) == L, "render snr=6");
  CHECK (memcmp (ref, got, bytes) == 0, "SNR: render(snr=6) == compose@6");
  free (json6);

  /* ── gain axis (non-anchor): moving the clean tone leaves the floor put ──
   */
  char *jsong = scene_json (12.0, 0.0, -6.0);
  CHECK (compose_collect (jsong, ref) == L, "compose tone=-6");
  CHECK (wfm_plan_render (p, "{\"gains\":[0.0,-6.0]}", got) == L,
         "render gain");
  CHECK (memcmp (ref, got, bytes) == 0, "GAIN: render(tone=-6) == compose");
  free (jsong);

  /* ── phase: φ=0 is the identity; a nonzero φ is a defined transform ── */
  CHECK (wfm_plan_render (p, "{\"phases\":[0.0,0.0]}", got) == L, "phase 0");
  CHECK (memcmp (base, got, bytes) == 0, "PHASE: φ=0 == baseline");
  CHECK (wfm_plan_render (p, "{\"phases\":[1.5,0.0]}", got) == L, "phase π/2");
  CHECK (any_diff (base, got), "PHASE: φ≠0 changes the output");

  /* ── enable: all-on is baseline; all-off leaves only the noise floor ── */
  CHECK (wfm_plan_render (p, "{\"enable\":[true,true]}", got) == L,
         "enable on");
  CHECK (memcmp (base, got, bytes) == 0, "ENABLE: all-on == baseline");
  CHECK (wfm_plan_render (p, "{\"enable\":[false,false]}", got) == L,
         "en off");
  CHECK (any_diff (base, got), "ENABLE: all-off drops the signal");

  /* ── determinism + Monte-Carlo seed independence ── */
  CHECK (wfm_plan_at (p, 6.0, 42, ref) == L, "at seed 42");
  CHECK (wfm_plan_at (p, 6.0, 42, got) == L, "at seed 42 again");
  CHECK (memcmp (ref, got, bytes) == 0, "DETERMINISM: at is repeatable");
  CHECK (wfm_plan_at (p, 6.0, 99, got) == L, "at seed 99");
  CHECK (any_diff (ref, got), "SEED: a new seed draws new noise");

  /* ── rejects: out-of-scope specs prepare to NULL ── */
  CHECK (wfm_plan_prepare (NULL) == NULL, "reject NULL");
  CHECK (wfm_plan_prepare ("} not json {") == NULL, "reject bad json");

  /* single bundled noisy source is not separable */
  wfm_source_t solo
      = { .type = 4, .snr = 12.0, .seed = 7, .sps = 8, .pn_length = 7 };
  wfm_segment_t sseg
      = { .sources = &solo, .n_sources = 1, .fs = 1e6, .num_samples = L };
  char *jsolo = wfm_spec_to_json (&sseg, 1, 0, 0, 0.0);
  CHECK (jsolo, "solo json");
  CHECK (wfm_plan_prepare (jsolo) == NULL, "reject bundled noisy source");
  free (jsolo);

  /* two segments is out of v1 scope */
  wfm_segment_t twoseg[2] = {
    { .sources = &solo, .n_sources = 1, .fs = 1e6, .num_samples = L },
    { .sources = &solo, .n_sources = 1, .fs = 1e6, .num_samples = L },
  };
  char *j2 = wfm_spec_to_json (twoseg, 2, 0, 0, 0.0);
  CHECK (j2, "2-seg json");
  CHECK (wfm_plan_prepare (j2) == NULL, "reject multi-segment");
  free (j2);

  wfm_plan_destroy (p);
  free (json);
  free (ref);
  free (got);
  free (base);
  printf ("test_wfm_plan: all checks passed\n");
  return 0;
}
