/*
 * test_wfm_compose.c — multi-segment composer (Phase B).
 *
 * Verifies segment sequencing, gaps (noise-floor by default, zeros when
 * clean/off), delays, once-through completion, and repeat looping — all
 * over the reused Phase-A synth engine.
 */
#define _GNU_SOURCE
#include "dp_test.h"
#include "wfm/wfm_compose.h"
#include "wfm/wfm_dsp.h"   /* wfm_frame_dsss_* for the dsss burst section */
#include "wfm/wfm_frame.h" /* the descriptor the unspread frame section reads */
#include "wfm_synth/wfm_synth_core.h"

#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The standalone-Synth half of the shared bridge. It has no header of its own
   — the Python binding declares it the same way (wfm_compose_ext.c) — and the
   unspread-frame section below asserts it refuses exactly what the composer's
   path refuses, which is the only way "they share the attach" is checkable. */
extern wfm_synth_state_t *wfm_source_to_synth (const wfm_source_t *, double);

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
  DP_REQUIRE_MSG (c, "create");
  static float complex all[8192];
  size_t               total = 0, n;
  float complex        buf[777];
  while ((n = wfm_compose_execute (c, buf, 777)) > 0)
    {
      DP_REQUIRE_MSG (total + n <= 8192, "overflow");
      for (size_t i = 0; i < n; i++)
        all[total + i] = buf[i];
      total += n;
    }
  DP_REQUIRE_MSG (total == 1000 + 500 + 4096, "total sample count");

  /* tone region non-zero; off region exactly zero; qpsk region non-zero */
  for (size_t i = 0; i < 1000; i++)
    DP_REQUIRE_MSG (all[i] != 0.0f, "tone region should be non-zero");
  for (size_t i = 1000; i < 1500; i++)
    DP_REQUIRE_MSG (all[i] == 0.0f, "off-time gap should be zero");
  for (size_t i = 1500; i < 1500 + 4096; i++)
    DP_REQUIRE_MSG (all[i] != 0.0f, "qpsk region should be non-zero");

  /* tone sits at +0.1 cyc/sample (100kHz / 1MHz): correlation ≈ 1 */
  double re = 0, im = 0;
  for (int k = 0; k < 1000; k++)
    {
      double ph = -2.0 * M_PI * 0.1 * k;
      re += creal (all[k]) * cos (ph) - cimag (all[k]) * sin (ph);
      im += creal (all[k]) * sin (ph) + cimag (all[k]) * cos (ph);
    }
  DP_REQUIRE_MSG (sqrt (re * re + im * im) / 1000.0 > 0.95,
                  "tone freq/correlation");
  wfm_compose_destroy (c);

  /* ── repeat: the sequence loops, execute never returns short ── */
  wfm_compose_state_t *r = wfm_compose_create (segs, 2, 1, 0);
  DP_REQUIRE_MSG (r, "create repeat");
  for (int it = 0; it < 8; it++)
    DP_REQUIRE_MSG (wfm_compose_execute (r, all, 8192) == 8192,
                    "repeat loops full");
  wfm_compose_destroy (r);

  /* ── JSON round-trip: spec → JSON → spec produces identical output ── */
  char *json = wfm_spec_to_json (segs, 2, 0, 0, 0.0);
  DP_REQUIRE_MSG (json, "to_json");
  DP_REQUIRE_MSG (strstr (json, "\"tone\"") && strstr (json, "\"qpsk\""),
                  "type names");
  DP_REQUIRE_MSG (strstr (json, "\"version\""), "version tag");
  wfm_compose_state_t *jc = wfm_compose_from_json (json);
  DP_REQUIRE_MSG (jc, "from_json");
  static float complex jall[8192];
  size_t               jtotal = 0;
  while ((n = wfm_compose_execute (jc, buf, 777)) > 0)
    {
      for (size_t i = 0; i < n; i++)
        jall[jtotal + i] = buf[i];
      jtotal += n;
    }
  DP_REQUIRE_MSG (jtotal == total, "round-trip sample count");
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
    DP_REQUIRE_MSG (jall[i] == all[i],
                    "JSON round-trip must be sample-identical");
  wfm_compose_destroy (jc);
  wfm_compose_destroy (d);

  /* bad spec → NULL (unknown type, empty segments) */
  DP_REQUIRE_MSG (
      !wfm_compose_from_json ("{\"segments\":[{\"type\":\"bogus\"}]}"),
      "unknown type rejected");
  DP_REQUIRE_MSG (!wfm_compose_from_json ("{\"segments\":[]}"),
                  "empty rejected");

  free (json);

  /* ── json-template: the dumped example must parse and compose ── */
  {
    char *tpl = wfm_spec_template_json ();
    DP_REQUIRE_MSG (tpl, "template built");
    DP_REQUIRE_MSG (strstr (tpl, "\"version\""), "template version tag");
    DP_REQUIRE_MSG (strstr (tpl, "\"sum\""),
                    "template shows a multi-source segment");
    wfm_compose_state_t *tc = wfm_compose_from_json (tpl);
    DP_REQUIRE_MSG (tc, "template round-trips through from_json");
    size_t tt = 0;
    while ((n = wfm_compose_execute (tc, buf, 777)) > 0)
      tt += n;
    /* 10000 tone + (8000 on + 2000 off gap) bits + 10000 mix */
    DP_REQUIRE_MSG (tt == 30000, "template sample count");
    wfm_compose_destroy (tc);
    free (tpl);
  }

  /* ── symbols JSON round-trip (gh #331): a type="symbols" source carries an
   *    explicit complex constellation array that must survive to_json →
   *    from_json. Both the 1-source inline serializer and the multi-source
   *    "sum" serializer are exercised; each must compose sample-identically.
   * ──
   */
  {
    float complex c0[8] = { 1 + 1 * I, -1 + 1 * I, 1 - 1 * I,  -1 - 1 * I,
                            1 + 1 * I, 1 - 1 * I,  -1 + 1 * I, -1 - 1 * I };
    float complex c1[8] = { 1 - 1 * I,  1 + 1 * I, -1 - 1 * I, -1 + 1 * I,
                            -1 - 1 * I, 1 + 1 * I, 1 - 1 * I,  -1 + 1 * I };
    wfm_source_t  a0    = { .type      = WFM_SYNTH_SYMBOLS,
                            .snr       = 100.0,
                            .seed      = 1,
                            .sps       = 4,
                            .symbols   = c0,
                            .n_symbols = 8 };
    wfm_source_t  a1    = { .type      = WFM_SYNTH_SYMBOLS,
                            .snr       = 100.0,
                            .seed      = 2,
                            .sps       = 4,
                            .level     = -3.0,
                            .symbols   = c1,
                            .n_symbols = 8 };
    /* both the inline (1-source) and "sum" (2-source) serializer paths */
    wfm_source_t  one[1]   = { a0 };
    wfm_source_t  both[2]  = { a0, a1 };
    wfm_segment_t segs2[2] = {
      { .sources = one, .n_sources = 1, .fs = 1e6, .num_samples = 256 },
      { .sources = both, .n_sources = 2, .fs = 1e6, .num_samples = 256 }
    };
    char *js = wfm_spec_to_json (segs2, 2, 0, 0, 0.0);
    DP_REQUIRE_MSG (js, "symbols to_json");
    DP_REQUIRE_MSG (strstr (js, "\"symbols\""),
                    "symbols type + array serialized");
    wfm_compose_state_t *jc = wfm_compose_from_json (js);
    DP_REQUIRE_MSG (jc, "symbols from_json");
    wfm_compose_state_t *dc = wfm_compose_create (segs2, 2, 0, 0);
    DP_REQUIRE_MSG (jc && dc, "symbols states built");
    float complex ja[600], da[600];
    size_t        jt = 0, dt = 0;
    while ((n = wfm_compose_execute (jc, buf, 333)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          ja[jt + i] = buf[i];
        jt += n;
      }
    while ((n = wfm_compose_execute (dc, buf, 333)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          da[dt + i] = buf[i];
        dt += n;
      }
    DP_REQUIRE_MSG (jt == dt && jt == 512, "symbols round-trip sample count");
    int ok = 1;
    for (size_t i = 0; i < jt; i++)
      if (ja[i] != da[i])
        ok = 0;
    DP_REQUIRE_MSG (
        ok, "symbols JSON round-trip must be sample-identical (gh #331)");
    wfm_compose_destroy (jc);
    wfm_compose_destroy (dc);
    free (js);
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
    DP_REQUIRE_MSG (wfm_compose_execute (ca, a, 64) == 64, "level a");
    DP_REQUIRE_MSG (wfm_compose_execute (cb, b, 64) == 64, "level b");
    int ok = 1;
    for (int i = 0; i < 64; i++)
      if (cabsf (b[i] - a[i] * 0.5f) > 1e-6f)
        ok = 0;
    DP_REQUIRE_MSG (ok, "level -6 dBFS == 0.5 * level-0 stream");
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
    DP_REQUIRE_MSG (wfm_compose_execute (c, viac, 200) == 200, "1src execute");
    wfm_compose_destroy (c);
    wfm_synth_state_t *s
        = wfm_synth_create (4, 1e6, 0.0, 9.0, 3, 7, 4, 7, 0, 0, 0.0);
    wfm_synth_steps (s, direct, 200);
    wfm_synth_destroy (s);
    int ok = 1;
    for (int i = 0; i < 200; i++)
      if (viac[i] != direct[i]) /* same wfm_synth_steps call → bit-identical */
        ok = 0;
    DP_REQUIRE_MSG (ok,
                    "1-source segment == bundled wfm_synth_steps (bit-exact)");
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
    DP_REQUIRE_MSG (wfm_compose_execute (c, sum, 100) == 100, "2src execute");
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
    DP_REQUIRE_MSG (ok, "2-source sum == s0 + 0.5*s1");
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
    DP_REQUIRE_MSG (c, "resolve create");
    size_t               nseg;
    const wfm_segment_t *rs = wfm_compose_segments (c, &nseg, NULL, NULL);
    DP_REQUIRE_MSG (rs[0].n_sources == 3, "noise source appended (2 → 3)");
    DP_REQUIRE_MSG (rs[0].sources[0].snr >= WFM_SYNTH_SNR_CLEAN,
                    "anchor cleaned");
    DP_REQUIRE_MSG (rs[0].sources[2].type == WFM_SYNTH_NOISE,
                    "appended is WFM_SYNTH_NOISE");
    DP_REQUIRE_MSG (fabs (rs[0].sources[2].level - (-10.0)) < 1e-9,
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
    DP_REQUIRE_MSG (c, "idempotent create");
    size_t               nseg;
    const wfm_segment_t *rs = wfm_compose_segments (c, &nseg, NULL, NULL);
    DP_REQUIRE_MSG (rs[0].n_sources == 3,
                    "idempotent: no second noise source");
    DP_REQUIRE_MSG (fabs (rs[0].sources[2].level - (-10.0)) < 1e-9,
                    "floor preserved");
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
    DP_REQUIRE_MSG (!wfm_compose_create (&seg, 1, 0, 0),
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
    DP_REQUIRE_MSG (c, "sum-json create");
    size_t               ns;
    int                  rp, ct;
    const wfm_segment_t *rs   = wfm_compose_segments (c, &ns, &rp, &ct);
    char                *json = wfm_spec_to_json (rs, ns, rp, ct, 0.0);
    DP_REQUIRE_MSG (json && strstr (json, "\"sum\""), "sum array emitted");
    /* reparse and compare sample-for-sample. */
    wfm_compose_state_t *jc = wfm_compose_from_json (json);
    DP_REQUIRE_MSG (jc, "sum from_json");
    float complex a[4096], b[4096];
    DP_REQUIRE_MSG (wfm_compose_execute (c, a, 4096) == 4096, "sum direct");
    DP_REQUIRE_MSG (wfm_compose_execute (jc, b, 4096) == 4096, "sum reparsed");
    int ok = 1;
    for (int i = 0; i < 4096; i++)
      if (a[i] != b[i])
        ok = 0;
    DP_REQUIRE_MSG (ok, "JSON sum round-trip sample-identical");
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
    DP_REQUIRE_MSG (j6 && strstr (j6, "\"headroom\""),
                    "headroom emitted when set");
    DP_REQUIRE_MSG (fabs (wfm_spec_headroom (j6) - 6.0) < 1e-9,
                    "headroom extracted");
    free (j6);
    char *j0 = wfm_spec_to_json (&seg, 1, 0, 0, 0.0);
    DP_REQUIRE_MSG (j0 && !strstr (j0, "\"headroom\""),
                    "headroom omitted at 0 dB");
    DP_REQUIRE_MSG (wfm_spec_headroom (j0) == 0.0, "absent headroom → 0");
    free (j0);
  }

  /* ── seed_advance: none = byte-identical repeat; all = whole seed advances
   *    (PN code changes) with the first pass unchanged ── */
  {
    wfm_source_t  pn = { .type      = 2, /* pn, no noise (snr 100) */
                         .snr       = 100.0,
                         .snr_mode  = 0,
                         .seed      = 1,
                         .sps       = 1,
                         .pn_length = 7 };
    wfm_segment_t seg
        = { .sources = &pn, .n_sources = 1, .fs = 1e6, .num_samples = 127 };
    float complex none[254], all[254];

    wfm_compose_state_t *cn = wfm_compose_create (&seg, 1, 1, 0);
    wfm_compose_set_seed_advance (cn, WFM_SEED_ADVANCE_NONE);
    DP_REQUIRE_MSG (wfm_compose_execute (cn, none, 254) == 254,
                    "seedadv none exec");

    wfm_compose_state_t *ca = wfm_compose_create (&seg, 1, 1, 0);
    wfm_compose_set_seed_advance (ca, WFM_SEED_ADVANCE_ALL);
    DP_REQUIRE_MSG (wfm_compose_execute (ca, all, 254) == 254,
                    "seedadv all exec");

    int none_same = 1, all_diff = 0, first_same = 1;
    for (int i = 0; i < 127; i++)
      {
        if (none[i] != none[127 + i])
          none_same = 0; /* none: repeat byte-identical */
        if (all[i] != all[127 + i])
          all_diff = 1; /* all: code changes on repeat */
        if (none[i] != all[i])
          first_same = 0; /* first pass is the unmodified seed */
      }
    DP_REQUIRE_MSG (none_same, "seed_advance none → byte-identical repeat");
    DP_REQUIRE_MSG (all_diff,
                    "seed_advance all → signal/code changes on repeat");
    DP_REQUIRE_MSG (first_same, "seed_advance all → first pass unchanged");
    wfm_compose_destroy (cn);
    wfm_compose_destroy (ca);
  }

  /* ── seed_advance noise: a noisy source's repeat is a fresh realization ──
   */
  {
    wfm_source_t  noisy = { .type     = 0, /* tone @ DC + AWGN */
                            .freq     = 0,
                            .snr      = 3.0,
                            .snr_mode = 1,
                            .seed     = 1,
                            .sps      = 1 };
    wfm_segment_t seg
        = { .sources = &noisy, .n_sources = 1, .fs = 1e6, .num_samples = 128 };
    float complex        z[256];
    wfm_compose_state_t *c = wfm_compose_create (&seg, 1, 1, 0);
    wfm_compose_set_seed_advance (c, WFM_SEED_ADVANCE_NOISE);
    DP_REQUIRE_MSG (wfm_compose_execute (c, z, 256) == 256,
                    "seedadv noise exec");
    int diff = 0;
    for (int i = 0; i < 128; i++)
      if (z[i] != z[128 + i])
        diff = 1;
    DP_REQUIRE_MSG (diff, "seed_advance noise → fresh noise on repeat");
    wfm_compose_destroy (c);
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
    DP_REQUIRE_MSG (wfm_compose_execute (c1, a, 256) == 256,
                    "ranged freq exec");
    int diff = 0;
    for (int i = 0; i < 128; i++)
      if (a[i] != a[128 + i])
        diff = 1;
    DP_REQUIRE_MSG (diff, "ranged freq → fresh draw each repeat");
    wfm_compose_state_t *c2 = wfm_compose_create (&rseg, 1, 1, 0);
    DP_REQUIRE_MSG (wfm_compose_execute (c2, b, 256) == 256,
                    "ranged freq exec 2");
    for (int i = 0; i < 256; i++)
      DP_REQUIRE_MSG (a[i] == b[i],
                      "ranged draw reproducible across composers");
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
    DP_REQUIRE_MSG (js, "ranged spec to json");
    DP_REQUIRE_MSG (strstr (js, "200") && strstr (js, "30"),
                    "ranges emitted as arrays");
    wfm_compose_state_t *c = wfm_compose_from_json (js);
    DP_REQUIRE_MSG (c, "ranged spec parse");
    size_t               nseg = 0;
    const wfm_segment_t *gg   = wfm_compose_segments (c, &nseg, NULL, NULL);
    DP_REQUIRE_MSG (nseg == 1, "ranged round-trip seg count");
    DP_REQUIRE_MSG (gg[0].ranged & WFM_RANGE_OFF_SAMPLES,
                    "off range bit survives");
    DP_REQUIRE_MSG (gg[0].off_samples == 10 && gg[0].off_samples_hi == 30,
                    "off range bounds survive");
    DP_REQUIRE_MSG (gg[0].sources[0].ranged & WFM_RANGE_FREQ,
                    "freq range bit survives");
    DP_REQUIRE_MSG (gg[0].sources[0].freq == 100.0
                        && gg[0].sources[0].freq_hi == 200.0,
                    "freq range bounds survive");
    free (js);
    wfm_compose_destroy (c);
  }

  /* ── ranged snr/level/f_end + ranged num/off all drawn on execute ── */
  {
    /* A chirp with every per-source ranged field set, in a segment whose on-
     * and off-times are themselves ranged. Executing forces start_segment to
     * draw each one — the freq path is covered above; this exercises the snr,
     * level, f_end and sample-count draws. Two repeats so the draws refire. */
    wfm_source_t  rsrc = { .type     = WFM_SYNTH_CHIRP,
                           .freq     = 0.01,
                           .freq_hi  = 0.02,
                           .f_end    = 0.03,
                           .f_end_hi = 0.04,
                           .snr      = 20.0,
                           .snr_hi   = 40.0,
                           .level    = -6.0,
                           .level_hi = -1.0,
                           .ranged   = WFM_RANGE_FREQ | WFM_RANGE_FEND
                                       | WFM_RANGE_SNR | WFM_RANGE_LEVEL,
                           .snr_mode = 1,
                           .seed     = 3,
                           .sps      = 1 };
    wfm_segment_t rseg
        = { .sources        = &rsrc,
            .n_sources      = 1,
            .fs             = 1e6,
            .num_samples    = 32,
            .num_samples_hi = 64,
            .off_samples    = 8,
            .off_samples_hi = 16,
            .ranged         = WFM_RANGE_NUM_SAMPLES | WFM_RANGE_OFF_SAMPLES };
    float complex        buf[512];
    wfm_compose_state_t *c = wfm_compose_create (&rseg, 1, 1, 0);
    DP_REQUIRE_MSG (wfm_compose_execute (c, buf, 512) == 512,
                    "ranged snr/level/f_end/num/off exec");
    wfm_compose_destroy (c);
  }

  /* ── empty on-time with a trailing gap: the off-only segment branch ── */
  {
    /* num_samples 0 → no synth started → straight to the PHASE_OFF gap. */
    wfm_source_t  src = { .type = 0, .snr = 100.0, .seed = 1, .sps = 1 };
    wfm_segment_t g
        = { .sources = &src, .n_sources = 1, .fs = 1e6, .off_samples = 8 };
    float complex        buf[8];
    wfm_compose_state_t *c   = wfm_compose_create (&g, 1, 0, 0);
    size_t               got = wfm_compose_execute (c, buf, 8);
    DP_REQUIRE_MSG (got == 8, "off-only segment emits the gap");
    for (size_t i = 0; i < got; i++)
      DP_REQUIRE_MSG (buf[i] == 0.0f, "off-only gap is zeros");
    wfm_compose_destroy (c);
  }

  /* ── chirp f_end range emits + round-trips through JSON ── */
  {
    wfm_source_t  s = { .type     = WFM_SYNTH_CHIRP,
                        .freq     = 1e5,
                        .f_end    = 2e5,
                        .f_end_hi = 3e5,
                        .ranged   = WFM_RANGE_FEND,
                        .snr      = 100.0,
                        .seed     = 1,
                        .sps      = 1 };
    wfm_segment_t g
        = { .sources = &s, .n_sources = 1, .fs = 1e6, .num_samples = 32 };
    char *js = wfm_spec_to_json (&g, 1, 0, 0, 0.0);
    DP_REQUIRE_MSG (js && strstr (js, "f_end"), "chirp f_end present");
    DP_REQUIRE_MSG (strstr (js, "300000") != NULL, "f_end hi bound emitted");
    wfm_compose_state_t *c = wfm_compose_from_json (js);
    DP_REQUIRE_MSG (c, "chirp f_end json parse");
    size_t               nseg = 0;
    const wfm_segment_t *gg   = wfm_compose_segments (c, &nseg, NULL, NULL);
    DP_REQUIRE_MSG (gg[0].sources[0].ranged & WFM_RANGE_FEND,
                    "f_end range bit survives");
    DP_REQUIRE_MSG (gg[0].sources[0].f_end == 2e5
                        && gg[0].sources[0].f_end_hi == 3e5,
                    "f_end range bounds survive");
    free (js);
    wfm_compose_destroy (c);
  }

  /* ── dsss: a two-code burst source, byte-identical to the pre-spread
   * bits path it replaces, with intrinsic on-time and a JSON round-trip ── */
  {
    /* small geometry: 8-chip acq ×3, 4-chip data code, 5 payload bits,
     * 2-bit sync, crc16 — sps 2, data-symbol Es/N0 6 dB. */
    uint8_t acq[8]   = { 1, 0, 1, 1, 0, 0, 1, 0 };
    uint8_t dcode[4] = { 0, 1, 1, 0 };
    uint8_t sync[2]  = { 1, 0 };
    uint8_t pay[5]   = { 1, 0, 0, 1, 1 };

    wfm_source_t dsss = { .type        = WFM_SYNTH_DSSS,
                          .snr         = 6.0,
                          .snr_mode    = 3, /* esno: outer data symbol */
                          .seed        = 7,
                          .sps         = 2,
                          .pn_length   = 7,
                          .acq_code    = acq,
                          .n_acq_code  = 8,
                          .acq_reps    = 3,
                          .data_code   = dcode,
                          .n_data_code = 4,
                          .sync        = sync,
                          .n_sync      = 2,
                          .bits        = pay, /* payload */
                          .n_bits      = 5,
                          .crc         = 1 };
    /* deliberately wrong num_samples: the intrinsic on-time must win */
    wfm_segment_t g = { .sources     = &dsss,
                        .n_sources   = 1,
                        .fs          = 1e6,
                        .num_samples = 17,
                        .off_samples = 10 };

    size_t nchips = wfm_frame_dsss_nchips (8, 3, 4, 2, 5, 1);
    DP_REQUIRE_MSG (nchips == 8 * 3 + (2 + 5 + 16) * 4, "burst chip count");
    size_t on = nchips * 2;

    wfm_compose_state_t *c = wfm_compose_create (&g, 1, 0, 0);
    DP_REQUIRE_MSG (c, "dsss create");
    size_t               nseg = 0;
    const wfm_segment_t *gg   = wfm_compose_segments (c, &nseg, NULL, NULL);
    DP_REQUIRE_MSG (gg[0].num_samples == on, "dsss on-time is intrinsic");
    static float complex dall[1024];
    size_t               dt = 0;
    while ((n = wfm_compose_execute (c, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          dall[dt + i] = buf[i];
        dt += n;
      }
    DP_REQUIRE_MSG (dt == on + 10, "dsss burst + gap length");
    /* (c stays alive: gg borrows its segments for the JSON emit below.) */

    /* equivalent hand-spread bits segment at the hand-converted fs SNR (the
     * exact conversion the demo used: esno − 10log10(sf·sps), mode fs). */
    static uint8_t chips[512];
    DP_REQUIRE_MSG (
        wfm_frame_dsss_chips (acq, 8, 3, dcode, 4, sync, 2, pay, 5, 1, chips)
            == nchips,
        "hand chips");
    wfm_source_t         bits = { .type       = WFM_SYNTH_BITS,
                                  .snr        = 6.0 - 10.0 * log10 (4.0 * 2.0),
                                  .snr_mode   = 1, /* fs */
                                  .seed       = 7,
                                  .sps        = 2,
                                  .pn_length  = 7,
                                  .bits       = chips,
                                  .n_bits     = nchips,
                                  .modulation = 1 };
    wfm_segment_t        gb   = { .sources     = &bits,
                                  .n_sources   = 1,
                                  .fs          = 1e6,
                                  .num_samples = on,
                                  .off_samples = 10 };
    wfm_compose_state_t *cb   = wfm_compose_create (&gb, 1, 0, 0);
    DP_REQUIRE_MSG (cb, "bits create");
    static float complex ball[1024];
    size_t               bt = 0;
    while ((n = wfm_compose_execute (cb, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          ball[bt + i] = buf[i];
        bt += n;
      }
    wfm_compose_destroy (cb);
    DP_REQUIRE_MSG (bt == dt, "dsss vs bits length");
    DP_REQUIRE_MSG (
        memcmp (dall, ball, dt * sizeof (float complex)) == 0,
        "dsss segment byte-identical to pre-spread bits at converted snr");

    /* JSON round-trip: geometry keys emitted, parse back, same bytes, and
     * the recorded num_samples is the resolved intrinsic on-time. */
    char *js = wfm_spec_to_json (gg, 1, 0, 0, 0.0);
    wfm_compose_destroy (c); /* json built; the borrow ends here */
    DP_REQUIRE_MSG (js && strstr (js, "\"dsss\""), "dsss type name");
    DP_REQUIRE_MSG (strstr (js, "acq_code") && strstr (js, "data_code")
                        && strstr (js, "\"payload\"")
                        && strstr (js, "\"crc\""),
                    "dsss geometry keys");
    wfm_compose_state_t *jc2 = wfm_compose_from_json (js);
    DP_REQUIRE_MSG (jc2, "dsss from_json");
    static float complex j2[1024];
    size_t               jt = 0;
    while ((n = wfm_compose_execute (jc2, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          j2[jt + i] = buf[i];
        jt += n;
      }
    DP_REQUIRE_MSG (jt == dt
                        && memcmp (dall, j2, dt * sizeof (float complex)) == 0,
                    "dsss json round-trip byte-identical");
    wfm_compose_destroy (jc2);

    /* "pattern" is accepted as an alias for "payload" on parse: rename the
     * emitted key in place (same length) and re-parse — same bytes. */
    char *pat = strstr (js, "\"payload\"");
    DP_REQUIRE_MSG (pat, "payload key present");
    memcpy (pat, "\"pattern\"", 9);
    wfm_compose_state_t *jp = wfm_compose_from_json (js);
    DP_REQUIRE_MSG (jp, "pattern alias parses");
    static float complex jp2[1024];
    size_t               pt = 0;
    while ((n = wfm_compose_execute (jp, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          jp2[pt + i] = buf[i];
        pt += n;
      }
    DP_REQUIRE_MSG (
        pt == dt && memcmp (dall, jp2, dt * sizeof (float complex)) == 0,
        "pattern alias byte-identical to payload");
    wfm_compose_destroy (jp);
    free (js);

    /* ebno on a dsss burst is esno (BPSK payload, 1 bit/symbol): same
     * bytes as the esno render above. */
    wfm_source_t  eb        = dsss;
    wfm_segment_t ge        = g;
    ge.sources              = &eb;
    eb.snr_mode             = 2; /* ebno */
    wfm_compose_state_t *ce = wfm_compose_create (&ge, 1, 0, 0);
    DP_REQUIRE_MSG (ce, "ebno dsss create");
    static float complex eall[1024];
    size_t               et = 0;
    while ((n = wfm_compose_execute (ce, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          eall[et + i] = buf[i];
        et += n;
      }
    wfm_compose_destroy (ce);
    DP_REQUIRE_MSG (
        et == dt && memcmp (dall, eall, dt * sizeof (float complex)) == 0,
        "dsss ebno == esno (BPSK payload)");

    /* an absent optional code is simply not emitted (no empty "" keys) */
    wfm_source_t  nos       = dsss;
    wfm_segment_t gnos      = g;
    gnos.sources            = &nos;
    nos.sync                = NULL;
    nos.n_sync              = 0;
    wfm_compose_state_t *cn = wfm_compose_create (&gnos, 1, 0, 0);
    DP_REQUIRE_MSG (cn, "no-sync dsss create");
    size_t               nn  = 0;
    const wfm_segment_t *ggn = wfm_compose_segments (cn, &nn, NULL, NULL);
    char                *jn  = wfm_spec_to_json (ggn, 1, 0, 0, 0.0);
    wfm_compose_destroy (cn);
    DP_REQUIRE_MSG (jn && !strstr (jn, "\"sync\""),
                    "absent sync key not emitted");
    free (jn);

    /* invalid geometry (payload but no data code) fails the segment into a
     * silent gap rather than wedging the stream. */
    wfm_source_t bad = dsss;
    bad.data_code    = NULL;
    bad.n_data_code  = 0;
    wfm_segment_t gbad
        = { .sources = &bad, .n_sources = 1, .fs = 1e6, .off_samples = 4 };
    wfm_compose_state_t *cbad = wfm_compose_create (&gbad, 1, 0, 0);
    DP_REQUIRE_MSG (cbad, "bad dsss still creates");
    size_t got = wfm_compose_execute (cbad, buf, 64);
    DP_REQUIRE_MSG (got == 4, "bad dsss segment degrades to its gap");
    for (size_t i = 0; i < got; i++)
      DP_REQUIRE_MSG (buf[i] == 0.0f, "bad dsss gap is zeros");
    wfm_compose_destroy (cbad);

    /* in a multi-source sum the on-time is explicit, so the build itself
     * hits the invalid geometry (set_dsss fails) — the whole segment still
     * degrades to its silent gap rather than wedging the stream. */
    wfm_source_t  mix[2] = { { .type = WFM_SYNTH_TONE, .freq = 0.1 }, bad };
    wfm_segment_t gsum   = { .sources     = mix,
                             .n_sources   = 2,
                             .fs          = 1e6,
                             .num_samples = 16,
                             .off_samples = 4 };
    wfm_compose_state_t *csum = wfm_compose_create (&gsum, 1, 0, 0);
    DP_REQUIRE_MSG (csum, "sum with bad dsss still creates");
    got = wfm_compose_execute (csum, buf, 64);
    DP_REQUIRE_MSG (got == 4, "sum with bad dsss degrades to its gap");
    for (size_t i = 0; i < got; i++)
      DP_REQUIRE_MSG (buf[i] == 0.0f, "sum bad-dsss gap is zeros");
    wfm_compose_destroy (csum);
  }

  /* ── repeats: bounded per-segment instancing — N instances back-to-back,
   * instance 0 byte-compatible, fresh AWGN + fresh ranged draws per
   * instance, fixed signal, JSON round-trip ── */
  {
    wfm_source_t bpsk = { .type      = WFM_SYNTH_BPSK,
                          .snr       = 3.0,
                          .snr_mode  = 3,
                          .seed      = 11,
                          .sps       = 2,
                          .pn_length = 7 };

    /* fixed durations: total = repeats * (on + off), and the clean signal
     * repeats byte-identically while a noisy one gets fresh AWGN. */
    wfm_segment_t g3 = { .sources     = &bpsk,
                         .n_sources   = 1,
                         .fs          = 1e6,
                         .num_samples = 100,
                         .off_samples = 20,
                         .repeats     = 3 };

    wfm_compose_state_t *c = wfm_compose_create (&g3, 1, 0, 0);
    DP_REQUIRE_MSG (c, "repeats create");
    static float complex rall[512];
    size_t               rt = 0;
    while ((n = wfm_compose_execute (c, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          rall[rt + i] = buf[i];
        rt += n;
      }
    wfm_compose_destroy (c);
    DP_REQUIRE_MSG (rt == 3 * (100 + 20), "repeats=3 total = 3*(on+off)");

    /* instance 0 == a repeats-less segment (byte-compat). */
    wfm_segment_t g1        = g3;
    g1.repeats              = 0; /* 0 and 1 both mean one instance */
    wfm_compose_state_t *c1 = wfm_compose_create (&g1, 1, 0, 0);
    DP_REQUIRE_MSG (c1, "repeats-less create");
    static float complex r1[256];
    size_t               t1 = 0;
    while ((n = wfm_compose_execute (c1, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          r1[t1 + i] = buf[i];
        t1 += n;
      }
    wfm_compose_destroy (c1);
    DP_REQUIRE_MSG (t1 == 120, "repeats-less length");
    DP_REQUIRE_MSG (memcmp (rall, r1, 120 * sizeof (float complex)) == 0,
                    "instance 0 byte-identical to a repeats-less segment");

    /* noisy instances never share an AWGN realization ... */
    DP_REQUIRE_MSG (memcmp (rall, rall + 120, 100 * sizeof (float complex))
                        != 0,
                    "fresh noise per instance");
    /* ... while the underlying signal is fixed: clean instances repeat
     * byte-identically. */
    wfm_source_t cb         = bpsk;
    cb.snr                  = 100.0; /* clean: no AWGN at all */
    wfm_segment_t gc        = g3;
    gc.sources              = &cb;
    wfm_compose_state_t *cc = wfm_compose_create (&gc, 1, 0, 0);
    DP_REQUIRE_MSG (cc, "clean repeats create");
    static float complex rc[512];
    size_t               tc = 0;
    while ((n = wfm_compose_execute (cc, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          rc[tc + i] = buf[i];
        tc += n;
      }
    wfm_compose_destroy (cc);
    DP_REQUIRE_MSG (
        tc == 360 && memcmp (rc, rc + 120, 120 * sizeof (float complex)) == 0
            && memcmp (rc, rc + 240, 120 * sizeof (float complex)) == 0,
        "signal fixed: clean instances byte-identical");

    /* ranged off_samples re-draws per instance (a jittered burst train)
     * and the instance-0 draw matches the repeats-less draw. */
    wfm_segment_t gr        = g3;
    gr.off_samples          = 10;
    gr.off_samples_hi       = 200;
    gr.ranged               = WFM_RANGE_OFF_SAMPLES;
    wfm_compose_state_t *cr = wfm_compose_create (&gr, 1, 0, 0);
    DP_REQUIRE_MSG (cr, "ranged repeats create");
    size_t rtot = 0;
    while ((n = wfm_compose_execute (cr, buf, 777)) > 0)
      rtot += n;
    wfm_compose_destroy (cr);
    DP_REQUIRE_MSG (rtot >= 3 * 110 && rtot <= 3 * 300,
                    "ranged gaps within bounds");
    wfm_segment_t gr1         = gr;
    gr1.repeats               = 1;
    wfm_compose_state_t *cs   = wfm_compose_create (&gr1, 1, 0, 0);
    size_t               stot = 0;
    while ((n = wfm_compose_execute (cs, buf, 777)) > 0)
      stot += n;
    wfm_compose_destroy (cs);
    DP_REQUIRE_MSG (rtot != 3 * stot,
                    "per-instance gap draws are distinct (not 3x the first)");

    /* ── gap noise (gh-409): a noisy segment's trailing gap carries its
     * noise floor — the same AWGN stream, continued — while gap_noise=off
     * and clean scenes keep exact-zero gaps ── */
    wfm_source_t         nsy = { .type = WFM_SYNTH_BPSK,
                                 .snr  = 0.0, /* esno 0 dB, sps 2 → −3 dB fs */
                                 .snr_mode  = 3,
                                 .seed      = 21,
                                 .sps       = 2,
                                 .pn_length = 7 };
    wfm_segment_t        gn  = { .sources     = &nsy,
                                 .n_sources   = 1,
                                 .fs          = 1e6,
                                 .num_samples = 200,
                                 .off_samples = 300 };
    wfm_compose_state_t *cgn = wfm_compose_create (&gn, 1, 0, 0);
    DP_REQUIRE_MSG (cgn, "gap-noise create");
    static float complex gna[512];
    size_t               gt = 0;
    while ((n = wfm_compose_execute (cgn, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          gna[gt + i] = buf[i];
        gt += n;
      }
    wfm_compose_destroy (cgn);
    DP_REQUIRE_MSG (gt == 500, "gap-noise length");
    int gap_nz = 0;
    for (size_t i = 200; i < 500; i++)
      gap_nz += (gna[i] != 0.0f);
    DP_REQUIRE_MSG (gap_nz > 250, "noisy gap carries noise");
    double gp = 0;
    for (size_t i = 200; i < 500; i++)
      gp += creal (gna[i]) * creal (gna[i]) + cimag (gna[i]) * cimag (gna[i]);
    gp /= 300.0;
    double floor_p = pow (10.0, -(0.0 - 10.0 * log10 (2.0)) / 10.0);
    DP_REQUIRE_MSG (fabs (gp - floor_p) / floor_p < 0.35,
                    "gap noise power is the resolved floor");
    /* continuity: the gap is the seamless continuation of the on-time
     * stream — byte-identical to hand-driving the same synth. */
    wfm_synth_state_t *ref = wfm_compose_build_synth (
        &nsy, 1e6, 200, nsy.freq, nsy.snr, nsy.f_end, 0, 0, 0);
    DP_REQUIRE_MSG (ref, "reference synth");
    static float complex rr[512];
    wfm_synth_steps (ref, rr, 200);
    wfm_synth_noise_steps (ref, rr + 200, 300);
    wfm_synth_destroy (ref);
    DP_REQUIRE_MSG (memcmp (gna, rr, 500 * sizeof (float complex)) == 0,
                    "gap is the byte-exact continuation of the on-time noise");
    /* the escape hatch restores hard zeros */
    wfm_segment_t goff       = gn;
    goff.gap_noise           = 1;
    wfm_compose_state_t *cof = wfm_compose_create (&goff, 1, 0, 0);
    DP_REQUIRE_MSG (cof, "gap-noise off create");
    size_t ot = 0, zeros = 1;
    while ((n = wfm_compose_execute (cof, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          if (ot + i >= 200 && buf[i] != 0.0f)
            zeros = 0;
        ot += n;
      }
    wfm_compose_destroy (cof);
    DP_REQUIRE_MSG (ot == 500 && zeros, "gap_noise=off gap is exact zeros");

    /* ── delay_samples: a leading gap — clean prefix is zeros and shifts
     * the burst; a ranged delay re-draws per instance; the span replayer
     * reports the rendered timeline exactly ── */
    wfm_source_t  cln = { .type = WFM_SYNTH_TONE, .freq = 1e5, .snr = 100.0 };
    wfm_segment_t gd  = { .sources       = &cln,
                          .n_sources     = 1,
                          .fs            = 1e6,
                          .num_samples   = 100,
                          .off_samples   = 40,
                          .delay_samples = 60 };
    wfm_compose_state_t *cd = wfm_compose_create (&gd, 1, 0, 0);
    DP_REQUIRE_MSG (cd, "delay create");
    static float complex da[256];
    size_t               dt2 = 0;
    while ((n = wfm_compose_execute (cd, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          da[dt2 + i] = buf[i];
        dt2 += n;
      }
    wfm_compose_destroy (cd);
    DP_REQUIRE_MSG (dt2 == 200, "delay + on + off length");
    for (size_t i = 0; i < 60; i++)
      DP_REQUIRE_MSG (da[i] == 0.0f, "clean delay is zeros");
    DP_REQUIRE_MSG (da[60] != 0.0f && da[159] != 0.0f,
                    "burst placed after delay");
    for (size_t i = 160; i < 200; i++)
      DP_REQUIRE_MSG (da[i] == 0.0f, "clean trailing gap is zeros");
    /* delay=0 byte-compat: same segment without delay == da shifted */
    wfm_segment_t g0        = gd;
    g0.delay_samples        = 0;
    wfm_compose_state_t *c0 = wfm_compose_create (&g0, 1, 0, 0);
    static float complex z0[256];
    size_t               zt = 0;
    while ((n = wfm_compose_execute (c0, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          z0[zt + i] = buf[i];
        zt += n;
      }
    wfm_compose_destroy (c0);
    DP_REQUIRE_MSG (
        zt == 140 && memcmp (da + 60, z0, 140 * sizeof (float complex)) == 0,
        "delayed burst is the delay-less render, shifted");
    /* ranged delay × repeats: spans replay the rendered instance timeline */
    wfm_segment_t gr2    = gd;
    gr2.delay_samples    = 10;
    gr2.delay_samples_hi = 90;
    gr2.ranged           = WFM_RANGE_DELAY_SAMPLES;
    gr2.repeats          = 3;
    wfm_span_t spans[8];
    size_t     nsp = wfm_compose_spans (&gr2, 1, spans, 8);
    DP_REQUIRE_MSG (nsp == 3, "three instances replayed");
    DP_REQUIRE_MSG (spans[0].delay != spans[1].delay
                        || spans[1].delay != spans[2].delay,
                    "per-instance delay draws are distinct");
    wfm_compose_state_t *cr2      = wfm_compose_create (&gr2, 1, 0, 0);
    size_t               rtot2    = 0;
    size_t               first_on = 0, seen = 0;
    while ((n = wfm_compose_execute (cr2, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          if (!seen && buf[i] != 0.0f)
            {
              first_on = rtot2 + i;
              seen     = 1;
            }
        rtot2 += n;
      }
    wfm_compose_destroy (cr2);
    size_t expect = 0;
    for (size_t q = 0; q < 3; q++)
      expect += spans[q].delay + spans[q].on + spans[q].off;
    DP_REQUIRE_MSG (rtot2 == expect, "rendered length == replayed span total");
    DP_REQUIRE_MSG (first_on == spans[0].delay,
                    "first burst lands where the span replay says");

    /* multi-source sum: the gap accumulates every source's noise term (the
     * resolved floor source keeps running while the cleaned signal sources
     * contribute zero) — long gap so the SCRATCH_CAP chunking path runs. */
    wfm_source_t mix2[2]
        = { { .type = WFM_SYNTH_TONE, .freq = 0.05, .snr = 3.0, .seed = 5 },
            { .type  = WFM_SYNTH_TONE,
              .freq  = -0.1,
              .level = -6.0,
              .snr   = 100.0,
              .seed  = 6 } };
    wfm_segment_t        gsum2 = { .sources       = mix2,
                                   .n_sources     = 2,
                                   .fs            = 1e6,
                                   .num_samples   = 100,
                                   .off_samples   = 6000, /* > SCRATCH_CAP */
                                   .delay_samples = 50 };
    wfm_compose_state_t *cs2   = wfm_compose_create (&gsum2, 1, 0, 0);
    DP_REQUIRE_MSG (cs2, "sum gap-noise create");
    size_t st2 = 0, nz2 = 0;
    double sp2 = 0;
    while ((n = wfm_compose_execute (cs2, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          {
            size_t pos = st2 + i;
            if (pos >= 150 && pos < 6150)
              {
                nz2 += (buf[i] != 0.0f);
                sp2 += creal (buf[i]) * creal (buf[i])
                       + cimag (buf[i]) * cimag (buf[i]);
              }
          }
        st2 += n;
      }
    wfm_compose_destroy (cs2);
    DP_REQUIRE_MSG (st2 == 50 + 100 + 6000, "sum delay+on+off length");
    DP_REQUIRE_MSG (nz2 > 5000, "sum gap carries the resolved floor");
    sp2 /= 6000.0;
    /* anchor: tone at 3 dB over fs → floor power 10^(-3/10) ≈ 0.501 */
    DP_REQUIRE_MSG (fabs (sp2 - 0.501) / 0.501 < 0.35,
                    "sum gap power ≈ floor");

    /* the span replayer's ranged num_samples branch */
    wfm_segment_t gsp  = gd;
    gsp.num_samples    = 80;
    gsp.num_samples_hi = 120;
    gsp.ranged         = WFM_RANGE_NUM_SAMPLES;
    wfm_span_t sp1[1];
    DP_REQUIRE_MSG (wfm_compose_spans (&gsp, 1, sp1, 1) == 1 && sp1[0].on >= 80
                        && sp1[0].on <= 120,
                    "spans replay a ranged on-time");

    /* noise_steps guards: NULL state / zero n are no-ops */
    wfm_synth_noise_steps (NULL, buf, 4);
    wfm_synth_state_t *g1s = wfm_compose_build_synth (
        &cln, 1e6, 100, cln.freq, cln.snr, cln.f_end, 0, 0, 0);
    DP_REQUIRE_MSG (g1s, "guard synth");
    wfm_synth_noise_steps (g1s, buf, 0);
    buf[0] = 1.0f;
    wfm_synth_noise_steps (g1s, buf, 1); /* clean → writes exact zeros */
    DP_REQUIRE_MSG (buf[0] == 0.0f, "clean noise_steps writes zeros");
    wfm_synth_destroy (g1s);

    /* sum form emits delay/gap_noise keys too */
    wfm_segment_t gse        = gsum2;
    gse.gap_noise            = 1;
    wfm_compose_state_t *cse = wfm_compose_create (&gse, 1, 0, 0);
    DP_REQUIRE_MSG (cse, "sum emit create");
    size_t               nse = 0;
    const wfm_segment_t *gge = wfm_compose_segments (cse, &nse, NULL, NULL);
    char                *jse = wfm_spec_to_json (gge, 1, 0, 0, 0.0);
    wfm_compose_destroy (cse);
    DP_REQUIRE_MSG (jse && strstr (jse, "\"sum\"")
                        && strstr (jse, "\"delay_samples\"")
                        && strstr (jse, "\"gap_noise\""),
                    "sum form emits delay + gap_noise");
    wfm_compose_state_t *cre2 = wfm_compose_from_json (jse);
    free (jse);
    DP_REQUIRE_MSG (cre2, "sum delay/gap_noise json parses");
    wfm_compose_destroy (cre2);

    /* JSON: delay + gap_noise round-trip; omitted at defaults */
    wfm_segment_t gjd        = gd;
    gjd.gap_noise            = 1;
    wfm_compose_state_t *cj2 = wfm_compose_create (&gjd, 1, 0, 0);
    size_t               nj2 = 0;
    const wfm_segment_t *gg2 = wfm_compose_segments (cj2, &nj2, NULL, NULL);
    char                *jd  = wfm_spec_to_json (gg2, 1, 0, 0, 0.0);
    wfm_compose_destroy (cj2);
    DP_REQUIRE_MSG (jd && strstr (jd, "\"delay_samples\"")
                        && strstr (jd, "\"gap_noise\""),
                    "delay + gap_noise keys emitted");
    wfm_compose_state_t *jr2 = wfm_compose_from_json (jd);
    free (jd);
    DP_REQUIRE_MSG (jr2, "delay json parses");
    static float complex jda[256];
    size_t               jdt = 0;
    while ((n = wfm_compose_execute (jr2, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          jda[jdt + i] = buf[i];
        jdt += n;
      }
    wfm_compose_destroy (jr2);
    DP_REQUIRE_MSG (jdt == 200
                        && memcmp (da, jda, 200 * sizeof (float complex)) == 0,
                    "delay json round-trip byte-identical");
    wfm_compose_state_t *cjd = wfm_compose_create (&g0, 1, 0, 0);
    size_t               njd = 0;
    const wfm_segment_t *ggd = wfm_compose_segments (cjd, &njd, NULL, NULL);
    char                *j0  = wfm_spec_to_json (ggd, 1, 0, 0, 0.0);
    wfm_compose_destroy (cjd);
    DP_REQUIRE_MSG (j0 && !strstr (j0, "\"delay_samples\"")
                        && !strstr (j0, "\"gap_noise\""),
                    "delay + gap_noise omitted at defaults");
    free (j0);

    /* JSON: repeats emitted (and only when > 1), round-trip byte-identical */
    wfm_compose_state_t *cj = wfm_compose_create (&g3, 1, 0, 0);
    size_t               nj = 0;
    const wfm_segment_t *gj = wfm_compose_segments (cj, &nj, NULL, NULL);
    char                *js = wfm_spec_to_json (gj, 1, 0, 0, 0.0);
    wfm_compose_destroy (cj);
    DP_REQUIRE_MSG (js && strstr (js, "\"repeats\""), "repeats key emitted");
    wfm_compose_state_t *jr = wfm_compose_from_json (js);
    free (js);
    DP_REQUIRE_MSG (jr, "repeats from_json");
    static float complex jall2[512];
    size_t               jt2 = 0;
    while ((n = wfm_compose_execute (jr, buf, 777)) > 0)
      {
        for (size_t i = 0; i < n; i++)
          jall2[jt2 + i] = buf[i];
        jt2 += n;
      }
    wfm_compose_destroy (jr);
    DP_REQUIRE_MSG (
        jt2 == rt && memcmp (rall, jall2, rt * sizeof (float complex)) == 0,
        "repeats json round-trip byte-identical");
    wfm_compose_state_t *c1j = wfm_compose_create (&g1, 1, 0, 0);
    size_t               n1j = 0;
    const wfm_segment_t *g1j = wfm_compose_segments (c1j, &n1j, NULL, NULL);
    char                *j1  = wfm_spec_to_json (g1j, 1, 0, 0, 0.0);
    wfm_compose_destroy (c1j);
    DP_REQUIRE_MSG (j1 && !strstr (j1, "\"repeats\""),
                    "repeats omitted at 1 (old specs unchanged)");
    free (j1);
  }

  /* ── the resolved floor reproduces the bundled noise power ──────────────
   *
   * wfm_snr_over_fs() decides where a multi-source segment's shared noise
   * floor sits; wfm_synth_create() decides how much noise a single bundled
   * source makes. If those two disagree, the same requested SNR means two
   * different things depending on how many sources happen to share a segment
   * — and nothing else in the tree would say so, because each is internally
   * consistent. That claim used to be a comment ("mirrors the conversion in
   * wfm_synth_core.c"), over a second copy of the formula.
   *
   * The expected noise powers below are LITERALS, derived by hand, and that
   * is the whole point of the test. The first version of it computed the
   * expectation by calling wfm_snr_over_fs() and compared that against the
   * noise the generator made — but both now route through one shared
   * conversion, so the two sides moved together: dropping the bits-per-symbol
   * term from the Eb/No branch left the test green. A known answer cannot
   * follow the code it checks.
   */
  {
    /* Unit-power sources, so P_total - 1 IS the noise power. N = 10^(-snr_fs
       /10) with snr_fs = snr - 10log10(span) for Es/No, plus 10log10(bps)
       first for Eb/No, and snr itself over fs. At snr = 9 dB:

         tone  fs    sps=8    snr_fs = +9.0000   N = 0.125893
         bpsk  Es/No sps=8    snr_fs = -0.0309   N = 1.007140
         bpsk  Eb/No sps=8    snr_fs = -0.0309   N = 1.007140  (bps=1)
         qpsk  Es/No sps=4    snr_fs = +2.9794   N = 0.503570
         qpsk  Eb/No sps=4    snr_fs = +5.9897   N = 0.251785  (bps=2)
         bpsk  auto  sps=16   snr_fs = -3.0412   N = 2.014281  (auto->Es/No)
         tone  auto  sps=16   snr_fs = +9.0000   N = 0.125893  (auto->fs) */
    const struct
    {
      int    type, mode, sps;
      double expect;
    } cases[] = {
      { 0, 1, 8, 0.125893 },  { 3, 3, 8, 1.007140 }, { 3, 2, 8, 1.007140 },
      { 4, 3, 4, 0.503570 },  { 4, 2, 4, 0.251785 }, { 3, 0, 16, 2.014281 },
      { 0, 0, 16, 0.125893 },
    };
    const double snr_db = 9.0;
    for (size_t k = 0; k < sizeof cases / sizeof cases[0]; k++)
      {
        wfm_source_t         s  = { .type      = cases[k].type,
                                    .freq      = 0.0,
                                    .snr       = snr_db,
                                    .snr_mode  = cases[k].mode,
                                    .seed      = 11,
                                    .sps       = cases[k].sps,
                                    .pn_length = 9,
                                    .pn_poly   = 0 };
        wfm_segment_t        g  = { .sources     = &s,
                                    .n_sources   = 1,
                                    .fs          = 1e6,
                                    .num_samples = 200000,
                                    .off_samples = 0 };
        wfm_compose_state_t *cc = wfm_compose_create (&g, 1, 0, 0);
        DP_REQUIRE_MSG (cc, "floor/bundled: create");
        double        p   = 0.0;
        size_t        got = 0, nn;
        float complex b[4096];
        while ((nn = wfm_compose_execute (cc, b, 4096)) > 0)
          {
            for (size_t i = 0; i < nn; i++)
              p += (double)(crealf (b[i]) * crealf (b[i])
                            + cimagf (b[i]) * cimagf (b[i]));
            got += nn;
          }
        wfm_compose_destroy (cc);
        DP_REQUIRE_MSG (got == 200000, "floor/bundled: sample count");
        double measured = p / (double)got - 1.0; /* minus the unit signal */
        char   msg[160];
        snprintf (msg, sizeof msg,
                  "type=%d mode=%d sps=%d: carries noise %.4f, the requested "
                  "%.0f dB means %.4f",
                  cases[k].type, cases[k].mode, cases[k].sps, measured, snr_db,
                  cases[k].expect);
        /* 3% covers the statistical spread at 200k samples; the errors this
           catches are factors (a missing 10log10(sps) is 9 dB at sps=8, and a
           dropped bits-per-symbol term is 3 dB at QPSK). */
        DP_REQUIRE_MSG (
            fabs (measured - cases[k].expect) < 0.03 * cases[k].expect, msg);
      }
  }

  /* ── an UNSPREAD frame: the descriptor is the waveform ──────────────────
   *
   * These fields were accepted, stored, readable back, and applied on no
   * unspread face at all: `wfm_source_attach_dsss` returned early for every
   * non-dsss type and nothing else consumed them, so a caller who asked for a
   * framed BPSK waveform got an unframed one and no way to find out. What made
   * that survivable is that no test asserted the frame CHANGES anything — so
   * that is what these assert, at the layer both the CLI and Python cross.  */
  {
    static const uint8_t sync_bits[13]
        = { 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1 }; /* Barker-13 */
    static const uint8_t acq_bits[8] = { 1, 0, 1, 0, 1, 0, 1, 0 };
    static const uint8_t payload[16]
        = { 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 0, 1, 1, 0 };
    wfm_source_t plain  = { .type       = WFM_SYNTH_BITS,
                            .snr        = 100.0,
                            .sps        = 1,
                            .pn_length  = 7,
                            .bits       = (uint8_t *)payload,
                            .n_bits     = sizeof payload,
                            .modulation = 1 /* bpsk */ };
    wfm_source_t framed = plain;
    framed.acq_code     = (uint8_t *)acq_bits;
    framed.n_acq_code   = sizeof acq_bits;
    framed.acq_reps     = 4;
    framed.sync         = (uint8_t *)sync_bits;
    framed.n_sync       = sizeof sync_bits;
    framed.crc          = 1;

    DP_REQUIRE_MSG (!wfm_source_has_frame (&plain),
                    "a preamble-less, sync-less source is not framed");
    DP_REQUIRE_MSG (wfm_source_has_frame (&framed), "this one is");
    /* `crc` alone must NOT read as a frame: it defaults to crc16 on every
       source, so treating it as intent would append a trailer to every
       unframed pattern ever generated. */
    wfm_source_t crc_only = plain;
    crc_only.crc          = 1;
    DP_REQUIRE_MSG (!wfm_source_has_frame (&crc_only),
                    "crc alone is a default, not an intent to frame");

    /* The frame is honoured where the payload is explicit, and refused with a
       reason where it is not — never accepted and dropped. */
    DP_REQUIRE_MSG (wfm_source_frame_error (&framed) == NULL, "bits is fine");
    wfm_source_t framed_pn = framed;
    framed_pn.type         = WFM_SYNTH_BPSK; /* symbols from the PN LFSR */
    DP_REQUIRE_MSG (wfm_source_frame_error (&framed_pn) != NULL,
                    "a frame on a PN-sourced waveform has no payload length");
    wfm_source_t framed_empty = framed;
    framed_empty.bits         = NULL;
    framed_empty.n_bits       = 0;
    DP_REQUIRE_MSG (wfm_source_frame_error (&framed_empty) != NULL,
                    "a frame with no payload is refused");

    /* And the refusal REACHES the caller on both faces. Asserting only that
       the predicate returns a message would leave the two construction paths
       free to ignore it — which is precisely the shape of the bug: the fields
       were validated nowhere and dropped silently. */
    DP_REQUIRE_MSG (!wfm_source_to_synth (&framed_pn, 1.0),
                    "the standalone face refuses a frame the waveform type "
                    "cannot carry");
    wfm_segment_t bad_seg = { .sources     = &framed_pn,
                              .n_sources   = 1,
                              .fs          = 1e6,
                              .num_samples = 64,
                              .off_samples = 0 };
    DP_REQUIRE_MSG (!wfm_compose_create (&bad_seg, 1, 0, 0),
                    "and the composer refuses the same segment, before it "
                    "builds anything");

    /* THE assertion whose absence was the bug. */
    size_t nb        = 32 + 13 + 16 + 16; /* preamble + sync + payload + crc */
    float complex *a = malloc (nb * sizeof *a);
    float complex *b = malloc (nb * sizeof *b);
    DP_REQUIRE_MSG (a && b, "alloc");
    /* wfm_compose_build_synth is THE single synth-construction path (the
       standalone Synth reaches the same attach through the shared bridge —
       covered from Python, where that face actually lives). */
    wfm_synth_state_t *sp
        = wfm_compose_build_synth (&plain, 1.0, nb, 0.0, 100.0, 0.0, 0, 0, 0);
    wfm_synth_state_t *sf
        = wfm_compose_build_synth (&framed, 1.0, nb, 0.0, 100.0, 0.0, 0, 0, 0);
    DP_REQUIRE_MSG (sp && sf, "both sources build");
    wfm_synth_steps (sp, a, nb);
    wfm_synth_steps (sf, b, nb);
    DP_REQUIRE_MSG (memcmp (a, b, nb * sizeof *a) != 0,
                    "a framed source must not emit the unframed waveform");

    /* And it is not merely DIFFERENT — it is the descriptor's own bits, so
       the layout, the CRC's position and its bit order come from the one
       place the receiver reads them from too. */
    wfm_frame_t f   = { 0 };
    f.preamble.kind = WFM_SEQ_LITERAL;
    f.preamble.bits = acq_bits;
    f.preamble.len  = sizeof acq_bits;
    f.preamble_reps = 4;
    f.sync.kind     = WFM_SEQ_LITERAL;
    f.sync.bits     = sync_bits;
    f.sync.len      = sizeof sync_bits;
    f.payload.kind  = WFM_SEQ_LITERAL;
    f.payload.bits  = payload;
    f.payload.len   = sizeof payload;
    f.crc           = 1;
    DP_REQUIRE_MSG (wfm_frame_nbits (&f) == nb, "the frame is nb bits");
    uint8_t *want = malloc (nb);
    DP_REQUIRE_MSG (want && wfm_frame_bits (&f, want, nb) == nb, "frame bits");
    for (size_t i = 0; i < nb; i++)
      {
        /* bpsk: bit 0 -> +1, bit 1 -> -1 (wfm_synth's mapping, sps == 1). */
        float expect = want[i] ? -1.0f : 1.0f;
        DP_REQUIRE_MSG (fabsf (crealf (b[i]) - expect) < 1e-6f,
                        "the framed stream IS wfm_frame_bits of its own "
                        "descriptor, symbol for symbol");
      }
    /* One frame, then it CYCLES — which is what turns a one-frame description
       into a multi-frame record without a repeat count in the descriptor. */
    float complex     *c2 = malloc (2 * nb * sizeof *c2);
    wfm_synth_state_t *sc = wfm_compose_build_synth (&framed, 1.0, 2 * nb, 0.0,
                                                     100.0, 0.0, 0, 0, 0);
    DP_REQUIRE_MSG (c2 && sc, "cycle alloc");
    wfm_synth_steps (sc, c2, 2 * nb);
    DP_REQUIRE_MSG (memcmp (c2, c2 + nb, nb * sizeof *c2) == 0,
                    "the frame repeats verbatim");

    /* ── an UNBUILDABLE frame must FAIL the build, on both paths ──────────
     *
     * wfm_frame_bits() refuses a descriptor it cannot materialise rather than
     * half-writing one, and the two construction paths have to turn that into
     * a NULL synth. Without that they would fall through to an unframed
     * waveform — the very failure the rest of this section exists to pin,
     * one layer down and this time silent even to a byte comparison, because
     * there would be nothing to compare against.
     *
     * A preamble LENGTH with no preamble ARRAY is that state. No face can
     * currently spell it — the CLI and wfm_json.c both derive the length FROM
     * the array — so it is a C-level guard and it is asserted in C. Note it
     * passes wfm_source_frame_error(): the user-facing rule is about the
     * payload, and this is the layer under it. */
    wfm_source_t broken = framed;
    broken.acq_code     = NULL; /* n_acq_code and acq_reps still set */
    DP_REQUIRE_MSG (wfm_source_has_frame (&broken), "still reads as framed");
    DP_REQUIRE_MSG (
        wfm_source_frame_error (&broken) == NULL,
        "and passes the payload rule — this is the layer under it");
    DP_REQUIRE_MSG (
        !wfm_compose_build_synth (&broken, 1.0, nb, 0.0, 100.0, 0.0, 0, 0, 0),
        "the composer's build fails rather than emitting an unframed "
        "waveform");
    DP_REQUIRE_MSG (!wfm_source_to_synth (&broken, 1.0),
                    "and the standalone bridge agrees — they share the attach "
                    "for exactly this reason");

    wfm_synth_destroy (sp);
    wfm_synth_destroy (sf);
    wfm_synth_destroy (sc);
    free (a);
    free (b);
    free (c2);
    free (want);
  }

  printf ("test_wfm_compose: OK (total=%zu, json round-trip, level, sum, "
          "resolve, sum-json, headroom, seed_advance, ranged fields, "
          "dsss burst, unspread frame, repeats)\n",
          total);
  return 0;
}
