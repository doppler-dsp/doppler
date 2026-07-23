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
  char         *json = wfm_spec_to_json (&seg, 1, 0, 0, 0.0);
  CHECK (json, "par: spec_to_json");

  size_t          nbytes = NPAR_LEN * sizeof (float _Complex);
  float _Complex *ref    = malloc (nbytes);
  float _Complex *got    = malloc (nbytes);
  CHECK (ref && got, "par: alloc");

  /* Full serial compose — the ground truth. */
  wfm_compose_state_t *c = wfm_compose_from_json (json);
  CHECK (c, "par: compose_from_json");
  size_t        total = 0, n;
  float complex buf[257];
  while ((n = wfm_compose_execute (c, buf, 257)) > 0 && total + n <= NPAR_LEN)
    {
      memcpy (ref + total, buf, n * sizeof *buf);
      total += n;
    }
  wfm_compose_destroy (c);
  CHECK (total == NPAR_LEN, "par: compose length");

  /* Parallel prepare (fans the 12 source builds across cores) + baseline. */
  wfm_plan_t *p = wfm_plan_prepare (json);
  CHECK (p, "par: prepare (parallel build)");
  CHECK (wfm_plan_n_sources (p) == NPAR_SRC, "par: n_sources");
  CHECK (wfm_plan_render (p, "{}", got) == NPAR_LEN, "par: render baseline");
  CHECK (memcmp (ref, got, nbytes) == 0,
         "PAR: parallel render({}) == serial compose, bit-for-bit");

  wfm_plan_destroy (p);
  free (ref);
  free (got);
  free (json);
  return 0;
}

int
main (void)
{
  if (test_parallel_build_bit_exact ())
    return 1;

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
  char *jranged = wfm_spec_to_json (&rseg, 1, 0, 0, 0.0);
  CHECK (jranged, "ranged-source json");
  CHECK (wfm_plan_prepare (jranged) == NULL, "reject ranged per-source field");
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
  char         *jnum = wfm_spec_to_json (&nseg, 1, 0, 0, 0.0);
  CHECK (jnum, "ranged-num-samples json");
  CHECK (wfm_plan_prepare (jnum) == NULL, "reject ranged num_samples");
  free (jnum);

  /* zero on-time is rejected. */
  wfm_segment_t zseg = {
    .sources = &plain_solo, .n_sources = 1, .fs = 1e6, .num_samples = 0
  };
  char *jzero = wfm_spec_to_json (&zseg, 1, 0, 0, 0.0);
  CHECK (jzero, "zero-num-samples json");
  CHECK (wfm_plan_prepare (jzero) == NULL, "reject num_samples == 0");
  free (jzero);

  /* two noise sources in one segment, or a non-trailing noise source, are
   * both still rejected. */
  wfm_source_t  noise_a      = { .type = 1 /* WFM_SYNTH_NOISE */, .seed = 5 };
  wfm_source_t  noise_b      = { .type = 1, .seed = 6 };
  wfm_source_t  two_noise[2] = { noise_a, noise_b };
  wfm_segment_t nnseg
      = { .sources = two_noise, .n_sources = 2, .fs = 1e6, .num_samples = L };
  char *jnn = wfm_spec_to_json (&nnseg, 1, 0, 0, 0.0);
  CHECK (jnn, "two-noise json");
  CHECK (wfm_plan_prepare (jnn) == NULL, "reject two noise sources");
  free (jnn);

  wfm_source_t  leading_noise[2] = { noise_a, plain_solo };
  wfm_segment_t lnseg            = {
    .sources = leading_noise, .n_sources = 2, .fs = 1e6, .num_samples = L
  };
  char *jln = wfm_spec_to_json (&lnseg, 1, 0, 0, 0.0);
  CHECK (jln, "leading-noise json");
  CHECK (wfm_plan_prepare (jln) == NULL, "reject non-trailing noise source");
  free (jln);

  /* differing per-segment sample rates are rejected: Plan assumes one
   * global fs. */
  wfm_segment_t fsdiff[2] = {
    { .sources = &plain_solo, .n_sources = 1, .fs = 1e6, .num_samples = L },
    { .sources = &plain_solo, .n_sources = 1, .fs = 2e6, .num_samples = L },
  };
  char *jfsdiff = wfm_spec_to_json (fsdiff, 2, 0, 0, 0.0);
  CHECK (jfsdiff, "differing-fs json");
  CHECK (wfm_plan_prepare (jfsdiff) == NULL,
         "reject differing per-segment fs");
  free (jfsdiff);

  /* an unbounded repeat/continuous scene has no fixed capacity. */
  char *jrepeat = wfm_spec_to_json (&nseg, 1, /*repeat=*/1, 0, 0.0);
  CHECK (jrepeat, "repeat json");
  CHECK (wfm_plan_prepare (jrepeat) == NULL, "reject repeat=true scene");
  free (jrepeat);
  char *jcont = wfm_spec_to_json (&nseg, 1, 0, /*continuous=*/1, 0.0);
  CHECK (jcont, "continuous json");
  CHECK (wfm_plan_prepare (jcont) == NULL, "reject continuous=true scene");
  free (jcont);

  /* anchor_seed: NULL plan, and a fully clean (no-noise) scene, both == 0. */
  CHECK (wfm_plan_anchor_seed (NULL) == 0, "anchor_seed(NULL) == 0");
  wfm_source_t  clean_solo = { .type      = 4,
                               .snr       = 100.0, /* clean: no noise at all */
                               .seed      = 41,
                               .sps       = 8,
                               .pn_length = 7 };
  wfm_segment_t cseg       = {
    .sources = &clean_solo, .n_sources = 1, .fs = 1e6, .num_samples = L
  };
  char *jclean = wfm_spec_to_json (&cseg, 1, 0, 0, 0.0);
  CHECK (jclean, "clean json");
  wfm_plan_t *pclean = wfm_plan_prepare (jclean);
  CHECK (pclean, "accept clean (no-noise) scene");
  CHECK (wfm_plan_anchor_seed (pclean) == 0,
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
  char *jsolo = wfm_spec_to_json (&sseg, 1, 0, 0, 0.0);
  CHECK (jsolo, "solo json");
  CHECK (compose_collect (jsolo, ref) == L, "compose solo baseline");
  wfm_plan_t *psolo = wfm_plan_prepare (jsolo);
  CHECK (psolo, "accept bundled noisy source");
  CHECK (wfm_plan_n_sources (psolo) == 1, "bundled n_sources == 1");
  CHECK (wfm_plan_render (psolo, "{}", got) == L, "bundled render baseline");
  CHECK (memcmp (ref, got, bytes) == 0,
         "BUNDLED: render({}) == compose(solo)");

  wfm_source_t solo9 = solo;
  solo9.snr          = 9.0;
  wfm_segment_t sseg9
      = { .sources = &solo9, .n_sources = 1, .fs = 1e6, .num_samples = L };
  char *jsolo9 = wfm_spec_to_json (&sseg9, 1, 0, 0, 0.0);
  CHECK (jsolo9, "solo9 json");
  CHECK (compose_collect (jsolo9, ref) == L, "compose solo@snr=9");
  CHECK (wfm_plan_render (psolo, "{\"snr\":9.0}", got) == L,
         "bundled render snr=9");
  CHECK (memcmp (ref, got, bytes) == 0,
         "BUNDLED SNR: render(snr=9) == compose(solo@9)");
  free (jsolo9);

  /* an enable override on a bundled segment drops both its signal AND its
   * (baked-in) noise -- the whole synth's contribution, exactly like the
   * composer's own external gain[0] would. */
  CHECK (wfm_plan_render (psolo, "{\"enable\":[false]}", got) == L,
         "bundled render disabled");
  for (size_t i = 0; i < L; i++)
    CHECK (got[i] == 0.0f, "BUNDLED ENABLE: disabling zeroes the output");

  wfm_plan_destroy (psolo);
  free (jsolo);

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
  char *jmulti = wfm_spec_to_json (multiseg, 2, 0, 0, 0.0);
  CHECK (jmulti, "multi-seg json");
  size_t          multi_len = L + 200 + L / 2;
  float _Complex *mref      = malloc (multi_len * sizeof *mref);
  float _Complex *mgot      = malloc (multi_len * sizeof *mgot);
  CHECK (mref && mgot, "multi alloc");
  wfm_compose_state_t *mc = wfm_compose_from_json (jmulti);
  CHECK (mc, "multi compose parse");
  CHECK (wfm_compose_execute (mc, mref, multi_len) == multi_len,
         "multi compose collect");
  wfm_compose_destroy (mc);
  wfm_plan_t *pmulti = wfm_plan_prepare (jmulti);
  CHECK (pmulti, "accept multi-segment");
  CHECK (wfm_plan_len (pmulti) == multi_len, "multi-segment len");
  CHECK (wfm_plan_n_sources (pmulti) == 2, "multi-segment n_sources");
  CHECK (wfm_plan_render (pmulti, "{}", mgot) == multi_len,
         "multi-segment render");
  CHECK (memcmp (mref, mgot, multi_len * sizeof *mref) == 0,
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
  char         *jrep    = wfm_spec_to_json (&repseg, 1, 0, 0, 0.0);
  size_t        rep_len = n_rep * (L / 4 + rep_gap);
  CHECK (jrep, "repeats json");
  float _Complex *rref = malloc (rep_len * sizeof *rref);
  float _Complex *rgot = malloc (rep_len * sizeof *rgot);
  CHECK (rref && rgot, "repeats alloc");
  wfm_compose_state_t *rc = wfm_compose_from_json (jrep);
  CHECK (rc, "repeats compose parse");
  CHECK (wfm_compose_execute (rc, rref, rep_len) == rep_len,
         "repeats compose collect");
  wfm_compose_destroy (rc);
  wfm_plan_t *prep = wfm_plan_prepare (jrep);
  CHECK (prep, "accept repeats");
  CHECK (wfm_plan_len (prep) == rep_len, "repeats len");
  CHECK (wfm_plan_render (prep, "{}", rgot) == rep_len, "repeats render");
  CHECK (memcmp (rref, rgot, rep_len * sizeof *rref) == 0,
         "REPEATS: render({}) == compose(repeats=N)");
  /* Each instance's noise differs -- the 4 (equal-length) bursts are not
   * identical to each other. */
  CHECK (memcmp (rgot, rgot + (L / 4 + rep_gap), (L / 4) * sizeof *rgot) != 0,
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
  char *jrg = wfm_spec_to_json (&rgseg, 1, 0, 0, 0.0);
  CHECK (jrg, "ranged-gap json");
  wfm_compose_state_t *rgc = wfm_compose_from_json (jrg);
  CHECK (rgc, "ranged-gap compose parse");
  float _Complex rgref[8192];
  size_t rg_len = wfm_compose_execute (rgc, rgref, 8192);
  CHECK (rg_len > 0 && rg_len < 8192, "ranged-gap compose collect");
  wfm_compose_destroy (rgc);
  wfm_plan_t *prg = wfm_plan_prepare (jrg);
  CHECK (prg, "accept ranged gap");
  CHECK (wfm_plan_len (prg) >= rg_len,
         "ranged-gap len is a worst-case capacity");
  float _Complex rgbuf[8192];
  size_t rg_got = wfm_plan_render (prg, "{}", rgbuf);
  CHECK (rg_got == rg_len, "RANGED GAP: baseline length == compose");
  CHECK (memcmp (rgref, rgbuf, rg_len * sizeof *rgref) == 0,
         "RANGED GAP: render({}) == compose (epoch-0 draw matches)");
  size_t rg_len1 = wfm_plan_render (prg, "{\"seed\":101}", rgbuf);
  size_t rg_len2 = wfm_plan_render (prg, "{\"seed\":202}", rgbuf);
  CHECK (rg_len1 > 0 && rg_len2 > 0, "seeded renders produce output");
  CHECK (rg_len1 != rg_len2 || rg_len1 != rg_got,
         "RANGED GAP: a seed override redraws the gap length");
  wfm_plan_destroy (prg);
  free (jrg);

  wfm_plan_destroy (p);
  free (json);
  free (ref);
  free (got);
  free (base);
  printf ("test_wfm_plan: all checks passed\n");
  return 0;
}
