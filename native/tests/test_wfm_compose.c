/*
 * test_wfm_compose.c — multi-segment composer (Phase B).
 *
 * Verifies segment sequencing, off-time gaps (zeros), once-through completion,
 * and repeat looping — all over the reused Phase-A synth engine.
 */
#define _GNU_SOURCE
#include "wfm/wfm_compose.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int
main (void)
{
  /* tone @100kHz (1000 on, 500 off), then qpsk (4096 on, 0 off). */
  wfm_source_t  src0    = { .type      = 0,
                            .freq      = 1e5,
                            .snr       = 100.0,
                            .snr_mode  = 0,
                            .seed      = 1,
                            .sps       = 8,
                            .pn_length = 7,
                            .pn_poly   = 0 };
  wfm_source_t  src1    = { .type      = 4,
                            .freq      = 0,
                            .snr       = 100.0,
                            .snr_mode  = 0,
                            .seed      = 5,
                            .sps       = 8,
                            .pn_length = 7,
                            .pn_poly   = 0 };
  wfm_segment_t segs[2] = {
    { .sources     = &src0,
      .n_sources   = 1,
      .fs          = 1e6,
      .num_samples = 1000,
      .off_samples = 500 },
    { .sources     = &src1,
      .n_sources   = 1,
      .fs          = 1e6,
      .num_samples = 4096,
      .off_samples = 0 },
  };

  /* ── once-through: collect the whole stream in odd-sized chunks ── */
  wfm_compose_state_t *c = wfm_compose_create (segs, 2, 0, 0);
  CHECK (c, "create");
  static float complex all[8192];
  size_t               total = 0, n;
  float complex        buf[777];
  while ((n = wfm_compose_execute (c, buf, 777)) > 0)
    {
      CHECK (total + n <= 8192, "overflow");
      for (size_t i = 0; i < n; i++)
        all[total + i] = buf[i];
      total += n;
    }
  CHECK (total == 1000 + 500 + 4096, "total sample count");

  /* tone region non-zero; off region exactly zero; qpsk region non-zero */
  for (size_t i = 0; i < 1000; i++)
    CHECK (all[i] != 0.0f, "tone region should be non-zero");
  for (size_t i = 1000; i < 1500; i++)
    CHECK (all[i] == 0.0f, "off-time gap should be zero");
  for (size_t i = 1500; i < 1500 + 4096; i++)
    CHECK (all[i] != 0.0f, "qpsk region should be non-zero");

  /* tone sits at +0.1 cyc/sample (100kHz / 1MHz): correlation ≈ 1 */
  double re = 0, im = 0;
  for (int k = 0; k < 1000; k++)
    {
      double ph = -2.0 * M_PI * 0.1 * k;
      re += creal (all[k]) * cos (ph) - cimag (all[k]) * sin (ph);
      im += creal (all[k]) * sin (ph) + cimag (all[k]) * cos (ph);
    }
  CHECK (sqrt (re * re + im * im) / 1000.0 > 0.95, "tone freq/correlation");
  wfm_compose_destroy (c);

  /* ── repeat: the sequence loops, execute never returns short ── */
  wfm_compose_state_t *r = wfm_compose_create (segs, 2, 1, 0);
  CHECK (r, "create repeat");
  for (int it = 0; it < 8; it++)
    CHECK (wfm_compose_execute (r, all, 8192) == 8192, "repeat loops full");
  wfm_compose_destroy (r);

  /* ── JSON round-trip: spec → JSON → spec produces identical output ── */
  char *json = wfm_spec_to_json (segs, 2, 0, 0, 0.0);
  CHECK (json, "to_json");
  CHECK (strstr (json, "\"tone\"") && strstr (json, "\"qpsk\""), "type names");
  CHECK (strstr (json, "\"version\""), "version tag");
  wfm_compose_state_t *jc = wfm_compose_from_json (json);
  CHECK (jc, "from_json");
  static float complex jall[8192];
  size_t               jtotal = 0;
  while ((n = wfm_compose_execute (jc, buf, 777)) > 0)
    {
      for (size_t i = 0; i < n; i++)
        jall[jtotal + i] = buf[i];
      jtotal += n;
    }
  CHECK (jtotal == total, "round-trip sample count");
  /* re-collect the direct stream for a byte comparison */
  wfm_compose_state_t *d      = wfm_compose_create (segs, 2, 0, 0);
  size_t               dtotal = 0;
  while ((n = wfm_compose_execute (d, buf, 777)) > 0)
    {
      for (size_t i = 0; i < n; i++)
        all[dtotal + i] = buf[i];
      dtotal += n;
    }
  for (size_t i = 0; i < total; i++)
    CHECK (jall[i] == all[i], "JSON round-trip must be sample-identical");
  wfm_compose_destroy (jc);
  wfm_compose_destroy (d);

  /* bad spec → NULL (unknown type, empty segments) */
  CHECK (!wfm_compose_from_json ("{\"segments\":[{\"type\":\"bogus\"}]}"),
         "unknown type rejected");
  CHECK (!wfm_compose_from_json ("{\"segments\":[]}"), "empty rejected");

  free (json);

  /* ── json-template: the dumped example must parse and compose ── */
  {
    char *tpl = wfm_spec_template_json ();
    CHECK (tpl, "template built");
    CHECK (strstr (tpl, "\"version\""), "template version tag");
    CHECK (strstr (tpl, "\"sum\""), "template shows a multi-source segment");
    wfm_compose_state_t *tc = wfm_compose_from_json (tpl);
    CHECK (tc, "template round-trips through from_json");
    size_t tt = 0;
    while ((n = wfm_compose_execute (tc, buf, 777)) > 0)
      tt += n;
    /* 10000 tone + (8000 on + 2000 off gap) bits + 10000 mix */
    CHECK (tt == 30000, "template sample count");
    wfm_compose_destroy (tc);
    free (tpl);
  }

  /* ── level: a segment at -6.0206 dBFS is the level-0 stream × 0.5 ── */
  {
    wfm_source_t src0
        = { .type = 0, .snr = 100.0, .seed = 1, .sps = 8, .pn_length = 7 };
    wfm_source_t src6 = src0;
    src6.level        = -6.020599913; /* gain 0.5 */
    wfm_segment_t s0
        = { .sources = &src0, .n_sources = 1, .fs = 1e6, .num_samples = 64 };
    wfm_segment_t s6
        = { .sources = &src6, .n_sources = 1, .fs = 1e6, .num_samples = 64 };
    float complex        a[64], b[64];
    wfm_compose_state_t *ca = wfm_compose_create (&s0, 1, 0, 0);
    wfm_compose_state_t *cb = wfm_compose_create (&s6, 1, 0, 0);
    CHECK (wfm_compose_execute (ca, a, 64) == 64, "level a");
    CHECK (wfm_compose_execute (cb, b, 64) == 64, "level b");
    int ok = 1;
    for (int i = 0; i < 64; i++)
      if (cabsf (b[i] - a[i] * 0.5f) > 1e-6f)
        ok = 0;
    CHECK (ok, "level -6 dBFS == 0.5 * level-0 stream");
    wfm_compose_destroy (ca);
    wfm_compose_destroy (cb);
  }

  /* ── 1 source ≡ bundled: a noisy single source == direct wfm_synth_steps ──
   */
  {
    wfm_source_t  src = { .type      = 4, /* qpsk */
                          .snr       = 9.0,
                          .snr_mode  = 3,
                          .seed      = 7,
                          .sps       = 4,
                          .pn_length = 7 };
    wfm_segment_t seg
        = { .sources = &src, .n_sources = 1, .fs = 1e6, .num_samples = 200 };
    float complex        viac[200], direct[200];
    wfm_compose_state_t *c = wfm_compose_create (&seg, 1, 0, 0);
    CHECK (wfm_compose_execute (c, viac, 200) == 200, "1src execute");
    wfm_compose_destroy (c);
    wfm_synth_state_t *s
        = wfm_synth_create (4, 1e6, 0.0, 9.0, 3, 7, 4, 7, 0, 0, 0.0);
    wfm_synth_steps (s, direct, 200);
    wfm_synth_destroy (s);
    int ok = 1;
    for (int i = 0; i < 200; i++)
      if (viac[i] != direct[i]) /* same wfm_synth_steps call → bit-identical */
        ok = 0;
    CHECK (ok, "1-source segment == bundled wfm_synth_steps (bit-exact)");
  }

  /* ── 2-source accumulate: segment sum == g0*synth0 + g1*synth1 ── */
  {
    wfm_source_t srcs[2] = {
      { .type = 0, .freq = 0.0, .snr = 100.0, .seed = 1 }, /* tone */
      { .type  = 0,
        .freq  = 2e5,
        .snr   = 100.0,
        .seed  = 2,
        .level = -6.020599913 }, /* tone -6 dB */
    };
    wfm_segment_t seg
        = { .sources = srcs, .n_sources = 2, .fs = 1e6, .num_samples = 100 };
    float complex        sum[100];
    wfm_compose_state_t *c = wfm_compose_create (&seg, 1, 0, 0);
    CHECK (wfm_compose_execute (c, sum, 100) == 100, "2src execute");
    wfm_compose_destroy (c);
    /* reference: render each source and add with the same gains + order. */
    wfm_synth_state_t *sa
        = wfm_synth_create (0, 1e6, 0.0, 100.0, 0, 1, 1, 7, 0, 0, 0.0);
    wfm_synth_state_t *sb
        = wfm_synth_create (0, 1e6, 2e5, 100.0, 0, 2, 1, 7, 0, 0, 0.0);
    float complex ba[100], bb[100];
    wfm_synth_steps (sa, ba, 100);
    wfm_synth_steps (sb, bb, 100);
    wfm_synth_destroy (sa);
    wfm_synth_destroy (sb);
    float gb = (float)pow (10.0, -6.020599913 / 20.0);
    int   ok = 1;
    for (int i = 0; i < 100; i++)
      {
        float complex ref = ba[i] + gb * bb[i];
        if (cabsf (sum[i] - ref) > 1e-5f)
          ok = 0;
      }
    CHECK (ok, "2-source sum == s0 + 0.5*s1");
  }

  /* ── noise resolve: snr on a source → a WFM_SYNTH_NOISE source at the floor
   * ──
   */
  {
    wfm_source_t srcs[2] = {
      { .type = 0, .snr = 10.0, .snr_mode = 1, .level = 0.0 }, /* anchor, fs */
      { .type  = 0,
        .freq  = 2e5,
        .snr   = 100.0,
        .level = -20.0 }, /* interferer */
    };
    wfm_segment_t seg
        = { .sources = srcs, .n_sources = 2, .fs = 1e6, .num_samples = 16 };
    wfm_compose_state_t *c = wfm_compose_create (&seg, 1, 0, 0);
    CHECK (c, "resolve create");
    size_t               nseg;
    const wfm_segment_t *rs = wfm_compose_segments (c, &nseg, NULL, NULL);
    CHECK (rs[0].n_sources == 3, "noise source appended (2 → 3)");
    CHECK (rs[0].sources[0].snr >= WFM_SYNTH_SNR_CLEAN, "anchor cleaned");
    CHECK (rs[0].sources[2].type == WFM_SYNTH_NOISE,
           "appended is WFM_SYNTH_NOISE");
    CHECK (fabs (rs[0].sources[2].level - (-10.0)) < 1e-9,
           "floor = level - snr_fs = -10 dBFS");
    wfm_compose_destroy (c);
  }

  /* ── resolve is idempotent: a clean+explicit-noise spec is a fixed point ──
   */
  {
    wfm_source_t resolved[3] = {
      { .type = 0, .snr = 100.0, .level = 0.0 },
      { .type = 0, .freq = 2e5, .snr = 100.0, .level = -20.0 },
      { .type = WFM_SYNTH_NOISE, .level = -10.0 }, /* explicit floor */
    };
    wfm_segment_t seg = {
      .sources = resolved, .n_sources = 3, .fs = 1e6, .num_samples = 16
    };
    wfm_compose_state_t *c = wfm_compose_create (&seg, 1, 0, 0);
    CHECK (c, "idempotent create");
    size_t               nseg;
    const wfm_segment_t *rs = wfm_compose_segments (c, &nseg, NULL, NULL);
    CHECK (rs[0].n_sources == 3, "idempotent: no second noise source");
    CHECK (fabs (rs[0].sources[2].level - (-10.0)) < 1e-9, "floor preserved");
    wfm_compose_destroy (c);
  }

  /* ── reject: a non-anchor source over-specifying snr AND level ── */
  {
    wfm_source_t bad[2] = {
      { .type = 0, .snr = 10.0, .level = 0.0 }, /* anchor */
      { .type = 0, .snr = 5.0, .level = -3.0 }, /* non-anchor: snr + level */
    };
    wfm_segment_t seg
        = { .sources = bad, .n_sources = 2, .fs = 1e6, .num_samples = 16 };
    CHECK (!wfm_compose_create (&seg, 1, 0, 0),
           "reject non-anchor snr + level");
  }

  /* ── JSON "sum" round-trip: a multi-source segment serialises + reparses ──
   */
  {
    wfm_source_t srcs[2] = {
      { .type      = 4, /* qpsk */
        .snr       = 12.0,
        .snr_mode  = 3,
        .seed      = 3,
        .sps       = 4,
        .pn_length = 7 },
      { .type = 0, .freq = 1.5e5, .snr = 100.0, .level = -10.0 }, /* tone */
    };
    wfm_segment_t seg
        = { .sources = srcs, .n_sources = 2, .fs = 1e6, .num_samples = 4096 };
    wfm_compose_state_t *c = wfm_compose_create (&seg, 1, 0, 0);
    CHECK (c, "sum-json create");
    size_t               ns;
    int                  rp, ct;
    const wfm_segment_t *rs   = wfm_compose_segments (c, &ns, &rp, &ct);
    char                *json = wfm_spec_to_json (rs, ns, rp, ct, 0.0);
    CHECK (json && strstr (json, "\"sum\""), "sum array emitted");
    /* reparse and compare sample-for-sample. */
    wfm_compose_state_t *jc = wfm_compose_from_json (json);
    CHECK (jc, "sum from_json");
    float complex a[4096], b[4096];
    CHECK (wfm_compose_execute (c, a, 4096) == 4096, "sum direct");
    CHECK (wfm_compose_execute (jc, b, 4096) == 4096, "sum reparsed");
    int ok = 1;
    for (int i = 0; i < 4096; i++)
      if (a[i] != b[i])
        ok = 0;
    CHECK (ok, "JSON sum round-trip sample-identical");
    free (json);
    wfm_compose_destroy (c);
    wfm_compose_destroy (jc);
  }

  /* ── headroom rides in the record: emitted when set, extracted back ── */
  {
    wfm_source_t  src = { .type = 0, .snr = 100.0, .sps = 8, .pn_length = 7 };
    wfm_segment_t seg
        = { .sources = &src, .n_sources = 1, .fs = 1e6, .num_samples = 16 };
    char *j6 = wfm_spec_to_json (&seg, 1, 0, 0, 6.0);
    CHECK (j6 && strstr (j6, "\"headroom\""), "headroom emitted when set");
    CHECK (fabs (wfm_spec_headroom (j6) - 6.0) < 1e-9, "headroom extracted");
    free (j6);
    char *j0 = wfm_spec_to_json (&seg, 1, 0, 0, 0.0);
    CHECK (j0 && !strstr (j0, "\"headroom\""), "headroom omitted at 0 dB");
    CHECK (wfm_spec_headroom (j0) == 0.0, "absent headroom → 0");
    free (j0);
  }

  /* ── ranged field: freq drawn uniformly per repeat, reproducibly ── */
  {
    /* A near-noiseless tone whose freq is a [lo, hi] draw. The noise stream is
     * deterministic per epoch, so any epoch-to-epoch sample difference is the
     * freq draw alone; a second composer must reproduce it bit-for-bit (the
     * draw hashes seed+epoch, it carries no RNG state). */
    wfm_source_t  rsrc = { .type     = 0,
                           .freq     = 0.05,
                           .freq_hi  = 0.45,
                           .ranged   = WFM_RANGE_FREQ,
                           .snr      = 100.0,
                           .snr_mode = 1,
                           .seed     = 7,
                           .sps      = 1 };
    wfm_segment_t rseg
        = { .sources = &rsrc, .n_sources = 1, .fs = 1e6, .num_samples = 128 };
    float complex        a[256], b[256];
    wfm_compose_state_t *c1 = wfm_compose_create (&rseg, 1, 1, 0);
    CHECK (wfm_compose_execute (c1, a, 256) == 256, "ranged freq exec");
    int diff = 0;
    for (int i = 0; i < 128; i++)
      if (a[i] != a[128 + i])
        diff = 1;
    CHECK (diff, "ranged freq → fresh draw each repeat");
    wfm_compose_state_t *c2 = wfm_compose_create (&rseg, 1, 1, 0);
    CHECK (wfm_compose_execute (c2, b, 256) == 256, "ranged freq exec 2");
    for (int i = 0; i < 256; i++)
      CHECK (a[i] == b[i], "ranged draw reproducible across composers");
    wfm_compose_destroy (c1);
    wfm_compose_destroy (c2);
  }

  /* ── ranged fields round-trip through JSON as [lo, hi] arrays ── */
  {
    wfm_source_t  s  = { .type     = 0,
                         .freq     = 100.0,
                         .freq_hi  = 200.0,
                         .ranged   = WFM_RANGE_FREQ,
                         .snr      = 10.0,
                         .snr_mode = 1,
                         .seed     = 1,
                         .sps      = 1 };
    wfm_segment_t g  = { .sources        = &s,
                         .n_sources      = 1,
                         .fs             = 1e6,
                         .num_samples    = 64,
                         .off_samples    = 10,
                         .off_samples_hi = 30,
                         .ranged         = WFM_RANGE_OFF_SAMPLES };
    char         *js = wfm_spec_to_json (&g, 1, 0, 0, 0.0);
    CHECK (js, "ranged spec to json");
    CHECK (strstr (js, "200") && strstr (js, "30"),
           "ranges emitted as arrays");
    wfm_compose_state_t *c = wfm_compose_from_json (js);
    CHECK (c, "ranged spec parse");
    size_t               nseg = 0;
    const wfm_segment_t *gg   = wfm_compose_segments (c, &nseg, NULL, NULL);
    CHECK (nseg == 1, "ranged round-trip seg count");
    CHECK (gg[0].ranged & WFM_RANGE_OFF_SAMPLES, "off range bit survives");
    CHECK (gg[0].off_samples == 10 && gg[0].off_samples_hi == 30,
           "off range bounds survive");
    CHECK (gg[0].sources[0].ranged & WFM_RANGE_FREQ,
           "freq range bit survives");
    CHECK (gg[0].sources[0].freq == 100.0 && gg[0].sources[0].freq_hi == 200.0,
           "freq range bounds survive");
    free (js);
    wfm_compose_destroy (c);
  }

  printf ("test_wfm_compose: OK (total=%zu, json round-trip, level, sum, "
          "resolve, sum-json, headroom, ranged fields)\n",
          total);
  return 0;
}
