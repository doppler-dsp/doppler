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
#include "dp_test.h"
#include "wfm/wfm_compose.h"
#include "wfm/wfm_plan.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define L 1024u /* samples per scene (qpsk sps=8 → 128 symbols) */

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
  return wfm_spec_to_json (&seg, 1, 0, 0, 0, 0.0);
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

/* Force the parallel per-source build and prove it is bit-identical to a full
 * serial compose. With L=1024 (< WFM_PLAN_PARALLEL_MIN_SAMPLES) the rest of
 * this file exercises only the serial fan-out fallback; this is the one case
 * that actually spins up dp_parallel_for's workers — many sources, a long
 * enough ON-time to cross the threshold. Distinct freq/seed/level per source
 * so a mis-slotted concurrent write would not accidentally still sum right. */
#define NPAR_SRC 12u
#define NPAR_LEN 8192u /* >= WFM_PLAN_PARALLEL_MIN_SAMPLES (4096) */

static int
test_parallel_build_bit_exact (void)
{
  wfm_source_t src[NPAR_SRC];
  memset (src, 0, sizeof src);
  for (unsigned k = 0; k < NPAR_SRC; k++)
    {
      /* tone / qpsk / bpsk in rotation; all clean so every source is its own
       * non-noise work item (no appended noise), maximising the fan-out. */
      src[k].type      = (k % 3 == 0) ? 0 : (k % 3 == 1) ? 4 : 3;
      src[k].freq      = -3e5 + (double)k * 5e4;
      src[k].snr       = 100.0; /* clean */
      src[k].snr_mode  = 0;
      src[k].seed      = 100u + k;
      src[k].sps       = 8;
      src[k].pn_length = 7;
      src[k].level     = -3.0 * (double)(k % 4);
    }
  wfm_segment_t seg  = { .sources     = src,
                         .n_sources   = NPAR_SRC,
                         .fs          = 1e6,
                         .num_samples = NPAR_LEN,
                         .off_samples = 0 };
  char         *json = wfm_spec_to_json (&seg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (json, "par: spec_to_json");

  size_t          nbytes = NPAR_LEN * sizeof (float _Complex);
  float _Complex *ref    = malloc (nbytes);
  float _Complex *got    = malloc (nbytes);
  DP_REQUIRE_MSG (ref && got, "par: alloc");

  /* Full serial compose — the ground truth. */
  wfm_compose_state_t *c = wfm_compose_from_json (json);
  DP_REQUIRE_MSG (c, "par: compose_from_json");
  size_t        total = 0, n;
  float complex buf[257];
  while ((n = wfm_compose_execute (c, buf, 257)) > 0 && total + n <= NPAR_LEN)
    {
      memcpy (ref + total, buf, n * sizeof *buf);
      total += n;
    }
  wfm_compose_destroy (c);
  DP_REQUIRE_MSG (total == NPAR_LEN, "par: compose length");

  /* Parallel prepare (fans the 12 source builds across cores) + baseline. */
  wfm_plan_t *p = wfm_plan_prepare (json);
  DP_REQUIRE_MSG (p, "par: prepare (parallel build)");
  DP_REQUIRE_MSG (wfm_plan_n_sources (p) == NPAR_SRC, "par: n_sources");
  DP_REQUIRE_MSG (wfm_plan_render (p, "{}", got) == NPAR_LEN,
                  "par: render baseline");
  DP_REQUIRE_MSG (memcmp (ref, got, nbytes) == 0,
                  "PAR: parallel render({}) == serial compose, bit-for-bit");

  wfm_plan_destroy (p);
  free (ref);
  free (got);
  free (json);
  return 0;
}

/* The SNR-carrying source does not have to be sources[0]. The resolver seeds
 * the appended noise source from the ANCHOR (wfm_resolve.c), while the ranged
 * off/delay draws key off sources[0] (wfm_compose.c's start_segment) — two
 * different seeds that are equal only in the scenes every other test here
 * builds, where the anchor happens to come first. Seeding the reconstructed
 * noise from sources[0] produced an entirely different realization (not a
 * rounding difference: full-scale, every sample), so this walks the anchor
 * through each position of a 3-source scene. */
#define ANCHOR_POS 3u

static int
test_noise_anchor_position (void)
{
  size_t          bytes = L * sizeof (float _Complex);
  float _Complex *ref   = malloc (bytes);
  float _Complex *got   = malloc (bytes);
  DP_REQUIRE_MSG (ref && got, "anchor: alloc");

  for (unsigned a = 0; a < ANCHOR_POS; a++)
    {
      wfm_source_t src[ANCHOR_POS];
      for (unsigned k = 0; k < ANCHOR_POS; k++)
        {
          memset (&src[k], 0, sizeof src[k]);
          src[k].type = 4; /* qpsk */
          src[k].freq = 1e5 * (double)k;
          src[k].snr  = (k == a) ? 6.0 : 100.0; /* one anchor, rest clean */
          src[k].seed = 40u + k;
          src[k].sps  = 8;
          src[k].pn_length = 7;
          src[k].level     = (k == a) ? 0.0 : -6.0;
        }
      wfm_segment_t seg  = { .sources     = src,
                             .n_sources   = ANCHOR_POS,
                             .fs          = 1e6,
                             .num_samples = L,
                             .off_samples = 0 };
      char         *json = wfm_spec_to_json (&seg, 1, 0, 0, 0, 0.0);
      DP_REQUIRE_MSG (json, "anchor: spec_to_json");
      DP_REQUIRE_MSG (compose_collect (json, ref) == L,
                      "anchor: compose length");

      wfm_plan_t *p = wfm_plan_prepare (json);
      DP_REQUIRE_MSG (p, "anchor: prepare");
      DP_REQUIRE_MSG (wfm_plan_render (p, "{}", got) == L,
                      "anchor: render baseline");
      DP_REQUIRE_MSG (
          memcmp (ref, got, bytes) == 0,
          "ANCHOR: render({}) == compose with the SNR source at any index");

      /* A seed override still moves the noise (Monte-Carlo), from any pos. */
      DP_REQUIRE_MSG (wfm_plan_render (p, "{\"seed\":4242}", got) == L,
                      "anchor: render with a seed override");
      DP_REQUIRE_MSG (memcmp (ref, got, bytes) != 0,
                      "ANCHOR: a seed override still redraws the noise");

      wfm_plan_destroy (p);
      free (json);
    }

  free (got);
  free (ref);
  return 0;
}

/* ── background=1: a contiguous prefix folds into ONE composite slot ──
 *
 * The fold is invisible to render(): the composite is just another cache
 * entry that happens to be pre-summed (base_gain 1.0, each member's own
 * 10^(level/20) baked in), so a baseline render must still equal a full
 * compose bit-for-bit while the cache holds 1 buffer instead of BG_N.
 * BG_LEN crosses both thresholds that matter — WFM_PLAN_PARALLEL_MIN_SAMPLES
 * (4096, so the group build fans out) and 2x WFM_PLAN_FOLD_CHUNK (32768, so
 * the ordered combine really is split across several work items). */
#define BG_N 6u
#define FG_N 2u
#define BG_LEN 70000u

static void
fill_src (wfm_source_t *s, unsigned k, int background)
{
  memset (s, 0, sizeof *s);
  s->type       = (k % 3 == 0) ? 0 : (k % 3 == 1) ? 4 : 3; /* tone/qpsk/bpsk */
  s->freq       = -3e5 + (double)k * 4e4;
  s->snr        = 100.0; /* clean: no appended noise source */
  s->snr_mode   = 0;
  s->seed       = 200u + k;
  s->sps        = 8;
  s->pn_length  = 7;
  s->level      = -2.0 * (double)(k % 5);
  s->background = background;
}

/* Full-compose `json` into out[n]; returns the sample count collected. */
static size_t
compose_n (const char *json, float _Complex *out, size_t n)
{
  wfm_compose_state_t *c = wfm_compose_from_json (json);
  if (!c)
    return 0;
  size_t        total = 0, got;
  float complex buf[257];
  while ((got = wfm_compose_execute (c, buf, 257)) > 0 && total + got <= n)
    {
      memcpy (out + total, buf, got * sizeof *buf);
      total += got;
    }
  wfm_compose_destroy (c);
  return total;
}

static int
test_background_fold (void)
{
  wfm_source_t src[BG_N + FG_N];
  for (unsigned k = 0; k < BG_N + FG_N; k++)
    fill_src (&src[k], k, k < BG_N);

  wfm_segment_t seg  = { .sources     = src,
                         .n_sources   = BG_N + FG_N,
                         .fs          = 1e6,
                         .num_samples = BG_LEN,
                         .off_samples = 0 };
  char         *json = wfm_spec_to_json (&seg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (json, "bg: spec_to_json");

  /* The same scene with the flag cleared: same samples, BG_N + FG_N slots. */
  wfm_source_t flat[BG_N + FG_N];
  for (unsigned k = 0; k < BG_N + FG_N; k++)
    fill_src (&flat[k], k, 0);
  wfm_segment_t fseg = seg;
  fseg.sources       = flat;
  char *fjson        = wfm_spec_to_json (&fseg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (fjson, "bg: flat spec_to_json");

  /* Foreground only — the reference for disabling the whole background. */
  wfm_segment_t gseg = seg;
  gseg.sources       = flat + BG_N;
  gseg.n_sources     = FG_N;
  char *gjson        = wfm_spec_to_json (&gseg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (gjson, "bg: fg-only spec_to_json");

  size_t          nbytes = BG_LEN * sizeof (float _Complex);
  float _Complex *ref    = malloc (nbytes);
  float _Complex *got    = malloc (nbytes);
  float _Complex *fgref  = malloc (nbytes);
  float _Complex *base   = malloc (nbytes);
  DP_REQUIRE_MSG (ref && got && fgref && base, "bg: alloc");

  DP_REQUIRE_MSG (compose_n (json, ref, BG_LEN) == BG_LEN,
                  "bg: compose length");

  wfm_plan_t *p = wfm_plan_prepare (json);
  DP_REQUIRE_MSG (p, "bg: prepare");

  /* The whole point: BG_N sources collapse to one overridable slot. */
  DP_REQUIRE_MSG (wfm_plan_n_sources (p) == 1u + FG_N,
                  "BG: n_sources == 1 + FG_N");
  wfm_plan_t *pf = wfm_plan_prepare (fjson);
  DP_REQUIRE_MSG (pf && wfm_plan_n_sources (pf) == BG_N + FG_N,
                  "bg: unfolded scene keeps a slot per source");

  /* Gate-0 for the fold: summing the prefix from zero, in spec order, is
   * exactly the partial sum compose() holds at that point. */
  DP_REQUIRE_MSG (wfm_plan_render (p, "{}", base) == BG_LEN,
                  "bg: render baseline");
  DP_REQUIRE_MSG (memcmp (ref, base, nbytes) == 0,
                  "BG: folded render({}) == compose, bit-for-bit");
  DP_REQUIRE_MSG (wfm_plan_render (pf, "{}", got) == BG_LEN,
                  "bg: unfolded baseline");
  DP_REQUIRE_MSG (memcmp (base, got, nbytes) == 0,
                  "BG: folding does not change the baseline at all");
  wfm_plan_destroy (pf);

  /* enable[0] = false drops the entire background field — what is left must
   * be exactly a compose of the foreground sources alone. */
  DP_REQUIRE_MSG (compose_n (gjson, fgref, BG_LEN) == BG_LEN,
                  "bg: fg compose length");
  DP_REQUIRE_MSG (wfm_plan_render (p, "{\"enable\":[false,true,true]}", got)
                      == BG_LEN,
                  "bg: render with background disabled");
  DP_REQUIRE_MSG (memcmp (fgref, got, nbytes) == 0,
                  "BG: enable[0]=false leaves exactly the foreground");

  /* gains[0] trims the composite as a UNIT: its members keep their relative
   * levels, so the background's contribution scales by 10^(g/20) while the
   * foreground is untouched. Checked physically (the float sum order differs
   * from any reference spec we could compose), 1e-5 relative. */
  const double gdb = -6.0;
  float        gl  = (float)pow (10.0, gdb / 20.0);
  DP_REQUIRE_MSG (wfm_plan_render (p, "{\"gains\":[-6.0,-2.0,-4.0]}", got)
                      == BG_LEN,
                  "bg: render with the background trimmed");
  double err = 0.0, mag = 0.0;
  for (size_t i = 0; i < BG_LEN; i++)
    {
      /* The override JSON pins each foreground slot to its own level (0.0 and
       * -2.0, per fill_src), so the foreground contribution is identical in
       * both renders and subtracts out — what is left is the background. */
      float _Complex bg_base = base[i] - fgref[i];
      float _Complex bg_trim = got[i] - fgref[i];
      err += cabs (bg_trim - gl * bg_base);
      mag += cabs (bg_base);
    }
  DP_REQUIRE_MSG (mag > 0.0 && err / mag < 1e-5,
                  "BG: gains[0] scales the whole background field as a unit");

  wfm_plan_destroy (p);
  free (base);
  free (fgref);
  free (got);
  free (ref);
  free (gjson);
  free (fjson);
  free (json);
  return 0;
}

/* A background source behind a foreground one is NOT a prefix: the composite
 * sums from zero, so it would only reproduce compose()'s running accumulator
 * approximately, silently costing the bit-exactness contract. prepare()
 * refuses the scene instead of degrading it. */
static int
test_background_must_be_prefix (void)
{
  wfm_source_t src[3];
  fill_src (&src[0], 0, 1);
  fill_src (&src[1], 1, 0);
  fill_src (&src[2], 2, 1); /* behind a foreground source */

  wfm_segment_t seg  = { .sources     = src,
                         .n_sources   = 3,
                         .fs          = 1e6,
                         .num_samples = L,
                         .off_samples = 0 };
  char         *json = wfm_spec_to_json (&seg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (json, "prefix: spec_to_json");
  DP_REQUIRE_MSG (wfm_plan_prepare (json) == NULL,
                  "BG: an interleaved background is rejected by prepare()");
  free (json);
  return 0;
}

/* A BUNDLED segment (a lone source carrying its own real snr) folds nothing:
 * its AWGN amplitude rides on base_gain, which the fold would overwrite with
 * 1.0 — the signal would survive, the noise would not. Flagging it must be a
 * no-op, not a silent amplitude bug. */
static int
test_background_bundled_is_not_folded (void)
{
  wfm_source_t solo;
  fill_src (&solo, 1, 1); /* qpsk, background=1 */
  solo.snr   = 9.0;       /* real snr -> bundled */
  solo.level = -3.0;

  wfm_segment_t seg  = { .sources     = &solo,
                         .n_sources   = 1,
                         .fs          = 1e6,
                         .num_samples = L,
                         .off_samples = 0 };
  char         *json = wfm_spec_to_json (&seg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (json, "bundled-bg: spec_to_json");

  size_t          bytes = L * sizeof (float _Complex);
  float _Complex *ref   = malloc (bytes);
  float _Complex *got   = malloc (bytes);
  DP_REQUIRE_MSG (ref && got, "bundled-bg: alloc");
  DP_REQUIRE_MSG (compose_n (json, ref, L) == L, "bundled-bg: compose length");

  wfm_plan_t *p = wfm_plan_prepare (json);
  DP_REQUIRE_MSG (p, "bundled-bg: prepare");
  DP_REQUIRE_MSG (wfm_plan_n_sources (p) == 1, "bundled-bg: still one slot");
  DP_REQUIRE_MSG (wfm_plan_render (p, "{}", got) == L,
                  "bundled-bg: render baseline");
  DP_REQUIRE_MSG (
      memcmp (ref, got, bytes) == 0,
      "BG: background on a bundled source is a no-op (noise gain intact)");

  wfm_plan_destroy (p);
  free (got);
  free (ref);
  free (json);
  return 0;
}

int
main (void)
{
  if (test_noise_anchor_position () || test_background_fold ()
      || test_background_must_be_prefix ()
      || test_background_bundled_is_not_folded ())
    return 1;
  if (test_parallel_build_bit_exact ())
    return 1;

  size_t          bytes = L * sizeof (float _Complex);
  float _Complex *ref   = malloc (bytes);
  float _Complex *got   = malloc (bytes);
  float _Complex *base  = malloc (bytes);
  DP_REQUIRE_MSG (ref && got && base, "alloc");

  char *json = scene_json (12.0, 0.0, 0.0);
  DP_REQUIRE_MSG (json, "scene_json");
  wfm_plan_t *p = wfm_plan_prepare (json);
  DP_REQUIRE_MSG (p, "prepare");
  DP_REQUIRE_MSG (wfm_plan_len (p) == L, "len");
  DP_REQUIRE_MSG (wfm_plan_n_sources (p) == 2, "n_sources (signals only)");
  DP_REQUIRE_MSG (wfm_plan_anchor_seed (p) == 7, "anchor seed = qpsk seed");

  /* ── Gate-0: render("{}") ≡ full compose, bit-for-bit ── */
  DP_REQUIRE_MSG (compose_collect (json, ref) == L, "compose baseline");
  DP_REQUIRE_MSG (wfm_plan_render (p, "{}", base) == L, "render baseline");
  DP_REQUIRE_MSG (memcmp (ref, base, bytes) == 0,
                  "GATE-0: render({}) == compose");
  /* NULL overrides is the same as an empty object. */
  DP_REQUIRE_MSG (wfm_plan_render (p, NULL, got) == L, "render NULL");
  DP_REQUIRE_MSG (memcmp (base, got, bytes) == 0,
                  "render(NULL) == render({})");

  /* ── SNR axis: at(6, anchor_seed) ≡ compose(scene @ snr=6) ── */
  char *json6 = scene_json (6.0, 0.0, 0.0);
  DP_REQUIRE_MSG (compose_collect (json6, ref) == L, "compose snr=6");
  DP_REQUIRE_MSG (wfm_plan_at (p, 6.0, wfm_plan_anchor_seed (p), got) == L,
                  "at(6)");
  DP_REQUIRE_MSG (memcmp (ref, got, bytes) == 0,
                  "SNR: at(6,anchor) == compose@6");
  /* render('{"snr":6}') takes the same path. */
  DP_REQUIRE_MSG (wfm_plan_render (p, "{\"snr\":6.0}", got) == L,
                  "render snr=6");
  DP_REQUIRE_MSG (memcmp (ref, got, bytes) == 0,
                  "SNR: render(snr=6) == compose@6");
  free (json6);

  /* ── gain axis (non-anchor): moving the clean tone leaves the floor put ──
   */
  char *jsong = scene_json (12.0, 0.0, -6.0);
  DP_REQUIRE_MSG (compose_collect (jsong, ref) == L, "compose tone=-6");
  DP_REQUIRE_MSG (wfm_plan_render (p, "{\"gains\":[0.0,-6.0]}", got) == L,
                  "render gain");
  DP_REQUIRE_MSG (memcmp (ref, got, bytes) == 0,
                  "GAIN: render(tone=-6) == compose");
  free (jsong);

  /* ── phase: φ=0 is the identity; a nonzero φ is a defined transform ── */
  DP_REQUIRE_MSG (wfm_plan_render (p, "{\"phases\":[0.0,0.0]}", got) == L,
                  "phase 0");
  DP_REQUIRE_MSG (memcmp (base, got, bytes) == 0, "PHASE: φ=0 == baseline");
  DP_REQUIRE_MSG (wfm_plan_render (p, "{\"phases\":[1.5,0.0]}", got) == L,
                  "phase π/2");
  DP_REQUIRE_MSG (any_diff (base, got), "PHASE: φ≠0 changes the output");

  /* ── enable: all-on is baseline; all-off leaves only the noise floor ── */
  DP_REQUIRE_MSG (wfm_plan_render (p, "{\"enable\":[true,true]}", got) == L,
                  "enable on");
  DP_REQUIRE_MSG (memcmp (base, got, bytes) == 0,
                  "ENABLE: all-on == baseline");
  DP_REQUIRE_MSG (wfm_plan_render (p, "{\"enable\":[false,false]}", got) == L,
                  "en off");
  DP_REQUIRE_MSG (any_diff (base, got), "ENABLE: all-off drops the signal");

  /* ── determinism + Monte-Carlo seed independence ── */
  DP_REQUIRE_MSG (wfm_plan_at (p, 6.0, 42, ref) == L, "at seed 42");
  DP_REQUIRE_MSG (wfm_plan_at (p, 6.0, 42, got) == L, "at seed 42 again");
  DP_REQUIRE_MSG (memcmp (ref, got, bytes) == 0,
                  "DETERMINISM: at is repeatable");
  DP_REQUIRE_MSG (wfm_plan_at (p, 6.0, 99, got) == L, "at seed 99");
  DP_REQUIRE_MSG (any_diff (ref, got), "SEED: a new seed draws new noise");

  /* ── rejects: out-of-scope specs prepare to NULL ── */
  DP_REQUIRE_MSG (wfm_plan_prepare (NULL) == NULL, "reject NULL");
  DP_REQUIRE_MSG (wfm_plan_prepare ("} not json {") == NULL,
                  "reject bad json");

  /* a ranged per-source field is still rejected (its cached render would be
   * ambiguous) */
  wfm_source_t  ranged_src = { .type      = 4,
                               .snr       = 12.0,
                               .seed      = 7,
                               .sps       = 8,
                               .pn_length = 7,
                               .ranged    = WFM_RANGE_SNR,
                               .snr_hi    = 20.0 };
  wfm_segment_t rseg       = {
    .sources = &ranged_src, .n_sources = 1, .fs = 1e6, .num_samples = L
  };
  char *jranged = wfm_spec_to_json (&rseg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (jranged, "ranged-source json");
  DP_REQUIRE_MSG (wfm_plan_prepare (jranged) == NULL,
                  "reject ranged per-source field");
  free (jranged);

  /* a ranged on-time (num_samples) is still rejected: it would invalidate
   * the fixed-length signal cache. */
  wfm_source_t plain_solo
      = { .type = 4, .snr = 12.0, .seed = 7, .sps = 8, .pn_length = 7 };
  wfm_segment_t nseg = { .sources        = &plain_solo,
                         .n_sources      = 1,
                         .fs             = 1e6,
                         .num_samples    = L,
                         .ranged         = WFM_RANGE_NUM_SAMPLES,
                         .num_samples_hi = 2 * L };
  char         *jnum = wfm_spec_to_json (&nseg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (jnum, "ranged-num-samples json");
  DP_REQUIRE_MSG (wfm_plan_prepare (jnum) == NULL,
                  "reject ranged num_samples");
  free (jnum);

  /* zero on-time is rejected. */
  wfm_segment_t zseg = {
    .sources = &plain_solo, .n_sources = 1, .fs = 1e6, .num_samples = 0
  };
  char *jzero = wfm_spec_to_json (&zseg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (jzero, "zero-num-samples json");
  DP_REQUIRE_MSG (wfm_plan_prepare (jzero) == NULL, "reject num_samples == 0");
  free (jzero);

  /* two noise sources in one segment, or a non-trailing noise source, are
   * both still rejected. */
  wfm_source_t  noise_a      = { .type = 1 /* WFM_SYNTH_NOISE */, .seed = 5 };
  wfm_source_t  noise_b      = { .type = 1, .seed = 6 };
  wfm_source_t  two_noise[2] = { noise_a, noise_b };
  wfm_segment_t nnseg
      = { .sources = two_noise, .n_sources = 2, .fs = 1e6, .num_samples = L };
  char *jnn = wfm_spec_to_json (&nnseg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (jnn, "two-noise json");
  DP_REQUIRE_MSG (wfm_plan_prepare (jnn) == NULL, "reject two noise sources");
  free (jnn);

  wfm_source_t  leading_noise[2] = { noise_a, plain_solo };
  wfm_segment_t lnseg            = {
    .sources = leading_noise, .n_sources = 2, .fs = 1e6, .num_samples = L
  };
  char *jln = wfm_spec_to_json (&lnseg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (jln, "leading-noise json");
  DP_REQUIRE_MSG (wfm_plan_prepare (jln) == NULL,
                  "reject non-trailing noise source");
  free (jln);

  /* differing per-segment sample rates are rejected: Plan assumes one
   * global fs. */
  wfm_segment_t fsdiff[2] = {
    { .sources = &plain_solo, .n_sources = 1, .fs = 1e6, .num_samples = L },
    { .sources = &plain_solo, .n_sources = 1, .fs = 2e6, .num_samples = L },
  };
  char *jfsdiff = wfm_spec_to_json (fsdiff, 2, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (jfsdiff, "differing-fs json");
  DP_REQUIRE_MSG (wfm_plan_prepare (jfsdiff) == NULL,
                  "reject differing per-segment fs");
  free (jfsdiff);

  /* an unbounded repeat/continuous scene has no fixed capacity. */
  char *jrepeat = wfm_spec_to_json (&nseg, 1, /*repeat=*/1, 0, 0, 0.0);
  DP_REQUIRE_MSG (jrepeat, "repeat json");
  DP_REQUIRE_MSG (wfm_plan_prepare (jrepeat) == NULL,
                  "reject repeat=true scene");
  free (jrepeat);
  char *jcont = wfm_spec_to_json (&nseg, 1, 0, /*continuous=*/1, 0, 0.0);
  DP_REQUIRE_MSG (jcont, "continuous json");
  DP_REQUIRE_MSG (wfm_plan_prepare (jcont) == NULL,
                  "reject continuous=true scene");
  free (jcont);

  /* anchor_seed: NULL plan, and a fully clean (no-noise) scene, both == 0. */
  DP_REQUIRE_MSG (wfm_plan_anchor_seed (NULL) == 0, "anchor_seed(NULL) == 0");
  wfm_source_t  clean_solo = { .type      = 4,
                               .snr       = 100.0, /* clean: no noise at all */
                               .seed      = 41,
                               .sps       = 8,
                               .pn_length = 7 };
  wfm_segment_t cseg       = {
    .sources = &clean_solo, .n_sources = 1, .fs = 1e6, .num_samples = L
  };
  char *jclean = wfm_spec_to_json (&cseg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (jclean, "clean json");
  wfm_plan_t *pclean = wfm_plan_prepare (jclean);
  DP_REQUIRE_MSG (pclean, "accept clean (no-noise) scene");
  DP_REQUIRE_MSG (wfm_plan_anchor_seed (pclean) == 0,
                  "anchor_seed == 0 for a no-noise scene");
  wfm_plan_destroy (pclean);
  free (jclean);

  /* ── bundled: a lone source carrying its own real SNR is now accepted;
   * its AWGN is baked into a per-instance noise-reconstruction synth, not a
   * separable external multiply (see wfm_plan.c's BUNDLED mode). ── */
  wfm_source_t solo
      = { .type = 4, .snr = 12.0, .seed = 7, .sps = 8, .pn_length = 7 };
  wfm_segment_t sseg
      = { .sources = &solo, .n_sources = 1, .fs = 1e6, .num_samples = L };
  char *jsolo = wfm_spec_to_json (&sseg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (jsolo, "solo json");
  DP_REQUIRE_MSG (compose_collect (jsolo, ref) == L, "compose solo baseline");
  wfm_plan_t *psolo = wfm_plan_prepare (jsolo);
  DP_REQUIRE_MSG (psolo, "accept bundled noisy source");
  DP_REQUIRE_MSG (wfm_plan_n_sources (psolo) == 1, "bundled n_sources == 1");
  DP_REQUIRE_MSG (wfm_plan_render (psolo, "{}", got) == L,
                  "bundled render baseline");
  DP_REQUIRE_MSG (memcmp (ref, got, bytes) == 0,
                  "BUNDLED: render({}) == compose(solo)");

  wfm_source_t solo9 = solo;
  solo9.snr          = 9.0;
  wfm_segment_t sseg9
      = { .sources = &solo9, .n_sources = 1, .fs = 1e6, .num_samples = L };
  char *jsolo9 = wfm_spec_to_json (&sseg9, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (jsolo9, "solo9 json");
  DP_REQUIRE_MSG (compose_collect (jsolo9, ref) == L, "compose solo@snr=9");
  DP_REQUIRE_MSG (wfm_plan_render (psolo, "{\"snr\":9.0}", got) == L,
                  "bundled render snr=9");
  DP_REQUIRE_MSG (memcmp (ref, got, bytes) == 0,
                  "BUNDLED SNR: render(snr=9) == compose(solo@9)");
  free (jsolo9);

  /* an enable override on a bundled segment drops both its signal AND its
   * (baked-in) noise -- the whole synth's contribution, exactly like the
   * composer's own external gain[0] would. */
  DP_REQUIRE_MSG (wfm_plan_render (psolo, "{\"enable\":[false]}", got) == L,
                  "bundled render disabled");
  for (size_t i = 0; i < L; i++)
    DP_REQUIRE_MSG (got[i] == 0.0f,
                    "BUNDLED ENABLE: disabling zeroes the output");

  wfm_plan_destroy (psolo);
  free (jsolo);

  /* A bundled source with a NON-ZERO level. Every bundled check above leaves
   * level at 0, i.e. gain exactly 1.0, where scaling the cached signal and
   * the reconstructed noise separately is trivially exact — so none of them
   * can see g*sig + g*noise drifting an ULP from the composer's single
   * g*(sig+noise). This is that case: at -3 dB it used to mismatch on ~2/3 of
   * the samples (max |diff| 1.7e-7). */
  wfm_source_t solo_lvl = solo;
  solo_lvl.level        = -3.0;
  wfm_segment_t sseg_lvl
      = { .sources = &solo_lvl, .n_sources = 1, .fs = 1e6, .num_samples = L };
  char *jlvl = wfm_spec_to_json (&sseg_lvl, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (jlvl, "solo level json");
  DP_REQUIRE_MSG (compose_collect (jlvl, ref) == L, "compose solo@level=-3");
  wfm_plan_t *plvl = wfm_plan_prepare (jlvl);
  DP_REQUIRE_MSG (plvl, "prepare bundled @ level=-3");
  DP_REQUIRE_MSG (wfm_plan_render (plvl, "{}", got) == L,
                  "bundled level render");
  DP_REQUIRE_MSG (
      memcmp (ref, got, bytes) == 0,
      "BUNDLED LEVEL: render({}) == compose(solo @ -3 dB), bit-for-bit");

  /* The same, with an SNR override: the noise is rebuilt at the new SNR and
   * still has to ride through the one shared multiply. */
  wfm_source_t solo_lvl9 = solo_lvl;
  solo_lvl9.snr          = 9.0;
  wfm_segment_t sseg_lvl9
      = { .sources = &solo_lvl9, .n_sources = 1, .fs = 1e6, .num_samples = L };
  char *jlvl9 = wfm_spec_to_json (&sseg_lvl9, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (jlvl9, "solo level snr json");
  DP_REQUIRE_MSG (compose_collect (jlvl9, ref) == L, "compose solo@-3,snr=9");
  DP_REQUIRE_MSG (wfm_plan_render (plvl, "{\"snr\":9.0}", got) == L,
                  "bundled level render snr=9");
  DP_REQUIRE_MSG (
      memcmp (ref, got, bytes) == 0,
      "BUNDLED LEVEL SNR: render(snr=9) == compose(solo @ -3 dB, snr 9)");
  free (jlvl9);

  /* Disabling still zeroes the whole contribution (ext_gain 0 -> the scale
   * pass wipes signal and noise together). */
  DP_REQUIRE_MSG (wfm_plan_render (plvl, "{\"enable\":[false]}", got) == L,
                  "bundled level render disabled");
  for (size_t i = 0; i < L; i++)
    DP_REQUIRE_MSG (got[i] == 0.0f,
                    "BUNDLED LEVEL ENABLE: disabling zeroes output");

  wfm_plan_destroy (plvl);
  free (jlvl);

  /* ── multi-segment: two segments now accepted, byte-exact vs. a full
   * compose of the same 2-segment spec. ── */
  wfm_source_t  tone2       = { .type      = 0,
                                .freq      = 2e5,
                                .snr       = 100.0,
                                .seed      = 11,
                                .sps       = 8,
                                .pn_length = 7,
                                .level     = -3.0 };
  wfm_segment_t multiseg[2] = {
    { .sources     = &solo,
      .n_sources   = 1,
      .fs          = 1e6,
      .num_samples = L,
      .off_samples = 200 },
    { .sources = &tone2, .n_sources = 1, .fs = 1e6, .num_samples = L / 2 },
  };
  char *jmulti = wfm_spec_to_json (multiseg, 2, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (jmulti, "multi-seg json");
  size_t          multi_len = L + 200 + L / 2;
  float _Complex *mref      = malloc (multi_len * sizeof *mref);
  float _Complex *mgot      = malloc (multi_len * sizeof *mgot);
  DP_REQUIRE_MSG (mref && mgot, "multi alloc");
  wfm_compose_state_t *mc = wfm_compose_from_json (jmulti);
  DP_REQUIRE_MSG (mc, "multi compose parse");
  DP_REQUIRE_MSG (wfm_compose_execute (mc, mref, multi_len) == multi_len,
                  "multi compose collect");
  wfm_compose_destroy (mc);
  wfm_plan_t *pmulti = wfm_plan_prepare (jmulti);
  DP_REQUIRE_MSG (pmulti, "accept multi-segment");
  DP_REQUIRE_MSG (wfm_plan_len (pmulti) == multi_len, "multi-segment len");
  DP_REQUIRE_MSG (wfm_plan_n_sources (pmulti) == 2, "multi-segment n_sources");
  DP_REQUIRE_MSG (wfm_plan_render (pmulti, "{}", mgot) == multi_len,
                  "multi-segment render");
  DP_REQUIRE_MSG (memcmp (mref, mgot, multi_len * sizeof *mref) == 0,
                  "MULTI-SEGMENT: render({}) == compose(2 segs)");
  wfm_plan_destroy (pmulti);
  free (jmulti);
  free (mref);
  free (mgot);

  /* ── repeats=N: a fixed-gap burst train, byte-exact vs. a full compose of
   * the same repeats=N spec (per-instance AWGN differs; signal is fixed). ──
   */
  wfm_source_t rep_src
      = { .type = 4, .snr = 10.0, .seed = 21, .sps = 8, .pn_length = 7 };
  size_t        rep_gap = 64;
  size_t        n_rep   = 4;
  wfm_segment_t repseg  = { .sources     = &rep_src,
                            .n_sources   = 1,
                            .fs          = 1e6,
                            .num_samples = L / 4,
                            .off_samples = rep_gap,
                            .repeats     = n_rep };
  char         *jrep    = wfm_spec_to_json (&repseg, 1, 0, 0, 0, 0.0);
  size_t        rep_len = n_rep * (L / 4 + rep_gap);
  DP_REQUIRE_MSG (jrep, "repeats json");
  float _Complex *rref = malloc (rep_len * sizeof *rref);
  float _Complex *rgot = malloc (rep_len * sizeof *rgot);
  DP_REQUIRE_MSG (rref && rgot, "repeats alloc");
  wfm_compose_state_t *rc = wfm_compose_from_json (jrep);
  DP_REQUIRE_MSG (rc, "repeats compose parse");
  DP_REQUIRE_MSG (wfm_compose_execute (rc, rref, rep_len) == rep_len,
                  "repeats compose collect");
  wfm_compose_destroy (rc);
  wfm_plan_t *prep = wfm_plan_prepare (jrep);
  DP_REQUIRE_MSG (prep, "accept repeats");
  DP_REQUIRE_MSG (wfm_plan_len (prep) == rep_len, "repeats len");
  DP_REQUIRE_MSG (wfm_plan_render (prep, "{}", rgot) == rep_len,
                  "repeats render");
  DP_REQUIRE_MSG (memcmp (rref, rgot, rep_len * sizeof *rref) == 0,
                  "REPEATS: render({}) == compose(repeats=N)");
  /* Each instance's noise differs -- the 4 (equal-length) bursts are not
   * identical to each other. */
  DP_REQUIRE_MSG (
      memcmp (rgot, rgot + (L / 4 + rep_gap), (L / 4) * sizeof *rgot) != 0,
      "REPEATS: instance 0 and instance 1 AWGN differ");
  wfm_plan_destroy (prep);
  free (jrep);
  free (rref);
  free (rgot);

  /* ── ranged gap + repeats: baseline (no seed override) reproduces a full
   * compose bit-for-bit (epoch 0, each segment's own dseed); a seed
   * override redraws the gap length, changing the materialized length. ── */
  wfm_source_t rgsrc
      = { .type = 4, .snr = 10.0, .seed = 33, .sps = 8, .pn_length = 7 };
  wfm_segment_t rgseg
      = { .sources          = &rgsrc,
          .n_sources        = 1,
          .fs               = 1e6,
          .num_samples      = L / 4,
          .off_samples      = 32,
          .off_samples_hi   = 256,
          .delay_samples    = 16,
          .delay_samples_hi = 128,
          .ranged           = WFM_RANGE_OFF_SAMPLES | WFM_RANGE_DELAY_SAMPLES,
          .repeats          = 3 };
  char *jrg = wfm_spec_to_json (&rgseg, 1, 0, 0, 0, 0.0);
  DP_REQUIRE_MSG (jrg, "ranged-gap json");
  wfm_compose_state_t *rgc = wfm_compose_from_json (jrg);
  DP_REQUIRE_MSG (rgc, "ranged-gap compose parse");
  float _Complex rgref[8192];
  size_t rg_len = wfm_compose_execute (rgc, rgref, 8192);
  DP_REQUIRE_MSG (rg_len > 0 && rg_len < 8192, "ranged-gap compose collect");
  wfm_compose_destroy (rgc);
  wfm_plan_t *prg = wfm_plan_prepare (jrg);
  DP_REQUIRE_MSG (prg, "accept ranged gap");
  DP_REQUIRE_MSG (wfm_plan_len (prg) >= rg_len,
                  "ranged-gap len is a worst-case capacity");
  float _Complex rgbuf[8192];
  size_t rg_got = wfm_plan_render (prg, "{}", rgbuf);
  DP_REQUIRE_MSG (rg_got == rg_len, "RANGED GAP: baseline length == compose");
  DP_REQUIRE_MSG (memcmp (rgref, rgbuf, rg_len * sizeof *rgref) == 0,
                  "RANGED GAP: render({}) == compose (epoch-0 draw matches)");
  size_t rg_len1 = wfm_plan_render (prg, "{\"seed\":101}", rgbuf);
  size_t rg_len2 = wfm_plan_render (prg, "{\"seed\":202}", rgbuf);
  DP_REQUIRE_MSG (rg_len1 > 0 && rg_len2 > 0, "seeded renders produce output");
  DP_REQUIRE_MSG (rg_len1 != rg_len2 || rg_len1 != rg_got,
                  "RANGED GAP: a seed override redraws the gap length");
  wfm_plan_destroy (prg);
  free (jrg);

  /* ── save / restore ────────────────────────────────────────────────────
   * A restored Plan renders bit-identically to the original; a fingerprint
   * mismatch rebuilds from the embedded spec (never NULL, never wrong); a
   * corrupt/truncated blob is rejected. */
  {
    size_t blen = wfm_plan_save_bytes (p);
    DP_REQUIRE_MSG (blen > 0, "save_bytes > 0");
    uint8_t *blob = malloc (blen);
    DP_REQUIRE_MSG (blob, "alloc blob");
    DP_REQUIRE_MSG (wfm_plan_save (p, blob) == blen,
                    "save returns bytes written");

    wfm_plan_t *pr = wfm_plan_restore (blob, blen);
    DP_REQUIRE_MSG (pr, "restore");
    DP_REQUIRE_MSG (wfm_plan_len (pr) == L && wfm_plan_n_sources (pr) == 2,
                    "restore: structure matches the spec");
    DP_REQUIRE_MSG (wfm_plan_render (pr, "{}", got) == L,
                    "restore render baseline");
    DP_REQUIRE_MSG (memcmp (base, got, bytes) == 0,
                    "SAVE/RESTORE: baseline is bit-exact");
    char *j6 = scene_json (6.0, 0.0, 0.0);
    DP_REQUIRE_MSG (compose_collect (j6, ref) == L, "compose snr=6 (restore)");
    DP_REQUIRE_MSG (wfm_plan_at (pr, 6.0, wfm_plan_anchor_seed (pr), got) == L,
                    "restore at(6)");
    DP_REQUIRE_MSG (
        memcmp (ref, got, bytes) == 0,
        "SAVE/RESTORE: an override on the restored Plan is bit-exact");
    free (j6);
    wfm_plan_destroy (pr);

    /* Fingerprint mismatch (corrupt the dsp_hash at offset 8): the fast path
     * is refused and the Plan is rebuilt from the embedded spec — still exact.
     */
    blob[8] ^= 0xFFu;
    wfm_plan_t *pm = wfm_plan_restore (blob, blen);
    DP_REQUIRE_MSG (pm, "restore rebuilds on fingerprint mismatch (not NULL)");
    DP_REQUIRE_MSG (wfm_plan_render (pm, "{}", got) == L,
                    "rebuilt render baseline");
    DP_REQUIRE_MSG (memcmp (base, got, bytes) == 0,
                    "SAVE/RESTORE: fingerprint-mismatch rebuild is bit-exact");
    wfm_plan_destroy (pm);
    blob[8] ^= 0xFFu; /* undo */

    /* Malformed / truncated blobs are rejected outright. */
    uint8_t m0 = blob[0];
    blob[0]    = 'X';
    DP_REQUIRE_MSG (wfm_plan_restore (blob, blen) == NULL, "reject bad magic");
    blob[0] = m0;
    DP_REQUIRE_MSG (wfm_plan_restore (blob, 10) == NULL,
                    "reject truncated blob");

    free (blob);

    /* dump/load: the file round-trip renders bit-identically. */
    const char *path = "test_wfm_plan_dump.bin";
    DP_REQUIRE_MSG (wfm_plan_dump (p, path) == 0, "dump to file");
    wfm_plan_t *pl = wfm_plan_load (path);
    DP_REQUIRE_MSG (pl, "load from file");
    DP_REQUIRE_MSG (wfm_plan_render (pl, "{}", got) == L,
                    "loaded render baseline");
    DP_REQUIRE_MSG (memcmp (base, got, bytes) == 0,
                    "DUMP/LOAD: baseline bit-exact");
    wfm_plan_destroy (pl);
    remove (path);
    DP_REQUIRE_MSG (wfm_plan_load ("no_such_wfm_plan_file.bin") == NULL,
                    "load of a missing file is NULL");
  }

  wfm_plan_destroy (p);
  free (json);
  free (ref);
  free (got);
  free (base);
  printf ("test_wfm_plan: all checks passed\n");
  return 0;
}
