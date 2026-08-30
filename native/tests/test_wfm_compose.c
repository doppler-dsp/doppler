/*
 * test_wfm_compose.c — multi-segment composer (Phase B).
 *
 * Verifies segment sequencing, gaps (noise-floor by default, zeros when
 * clean/off), delays, once-through completion, and repeat looping — all
 * over the reused Phase-A synth engine.
 */
#include "ccsds_tm/ccsds_tm.h"
#include "ccsds_tm/ccsds_tm_frame.h"
#include "dp_test.h"
#include "gold/gold_core.h"
#include "pn/pn_core.h"
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
  char *json = wfm_spec_to_json (segs, 2, 0, 0, 0, 0.0);
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
    char *js = wfm_spec_to_json (segs2, 2, 0, 0, 0, 0.0);
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
    char                *json = wfm_spec_to_json (rs, ns, rp, ct, 0, 0.0);
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
    char *j6 = wfm_spec_to_json (&seg, 1, 0, 0, 0, 6.0);
    DP_REQUIRE_MSG (j6 && strstr (j6, "\"headroom\""),
                    "headroom emitted when set");
    DP_REQUIRE_MSG (fabs (wfm_spec_headroom (j6) - 6.0) < 1e-9,
                    "headroom extracted");
    free (j6);
    char *j0 = wfm_spec_to_json (&seg, 1, 0, 0, 0, 0.0);
    DP_REQUIRE_MSG (j0 && !strstr (j0, "\"headroom\""),
                    "headroom omitted at 0 dB");
    DP_REQUIRE_MSG (wfm_spec_headroom (j0) == 0.0, "absent headroom → 0");
    free (j0);
  }

  /* ── seed_advance rides in the record too (doppler#978) ──
   *
   * Its twin above is not decoration: the key was PARSED and never emitted,
   * so a recorded run replayed with the mode reset to NONE and every loop
   * after the first came out identical. The composer is the SSOT here --
   * `--from-file` sets the mode from the spec and the flag path sets it from
   * `--seed-advance`, so a serialiser reads it back with
   * wfm_compose_seed_advance() rather than from whichever half supplied it.
   */
  {
    wfm_source_t  src = { .type = 0, .snr = 100.0, .sps = 8, .pn_length = 7 };
    wfm_segment_t seg
        = { .sources = &src, .n_sources = 1, .fs = 1e6, .num_samples = 16 };
    wfm_compose_state_t *c = wfm_compose_create (&seg, 1, /*repeat=*/1, 0);
    DP_REQUIRE_MSG (c, "seed_advance create");
    DP_REQUIRE_MSG (wfm_compose_seed_advance (c) == WFM_SEED_ADVANCE_NONE,
                    "seed_advance defaults to NONE");
    wfm_compose_set_seed_advance (c, WFM_SEED_ADVANCE_NOISE);
    DP_REQUIRE_MSG (wfm_compose_seed_advance (c) == WFM_SEED_ADVANCE_NOISE,
                    "the getter reads back what the setter wrote");
    /* Out of range is ignored by the setter — so the getter must still show
     * the last good value, not whatever was passed. */
    wfm_compose_set_seed_advance (c, 99);
    DP_REQUIRE_MSG (wfm_compose_seed_advance (c) == WFM_SEED_ADVANCE_NOISE,
                    "an out-of-range mode leaves the getter untouched");

    size_t               ns;
    int                  rp, ct;
    const wfm_segment_t *rs = wfm_compose_segments (c, &ns, &rp, &ct);
    char                *jn
        = wfm_spec_to_json (rs, ns, rp, ct, wfm_compose_seed_advance (c), 0.0);
    /* Presence only — the VALUE is checked by the round trip below, so this
     * assertion must not encode cJSON's whitespace. */
    DP_REQUIRE_MSG (jn && strstr (jn, "\"seed_advance\""),
                    "seed_advance emitted when set");
    /* The round trip that matters: parse it back and the mode survives. */
    wfm_compose_state_t *jc = wfm_compose_from_json (jn);
    DP_REQUIRE_MSG (jc, "seed_advance from_json");
    DP_REQUIRE_MSG (wfm_compose_seed_advance (jc) == WFM_SEED_ADVANCE_NOISE,
                    "seed_advance survives emit → parse");
    free (jn);
    wfm_compose_destroy (jc);
    wfm_compose_destroy (c);

    /* Omitted at the default, exactly as headroom is at 0 dB: the inline
     * form's field order is frozen for byte-identity, so an always-present
     * key would churn every capture ever recorded to say "none". */
    char *j0 = wfm_spec_to_json (&seg, 1, 0, 0, WFM_SEED_ADVANCE_NONE, 0.0);
    DP_REQUIRE_MSG (j0 && !strstr (j0, "\"seed_advance\""),
                    "seed_advance omitted at NONE");
    wfm_compose_state_t *j0c = wfm_compose_from_json (j0);
    DP_REQUIRE_MSG (
        j0c && wfm_compose_seed_advance (j0c) == WFM_SEED_ADVANCE_NONE,
        "absent seed_advance → NONE");
    free (j0);
    wfm_compose_destroy (j0c);
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
    char         *js = wfm_spec_to_json (&g, 1, 0, 0, 0, 0.0);
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
    char *js = wfm_spec_to_json (&g, 1, 0, 0, 0, 0.0);
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

    wfm_source_t dsss = { .type         = WFM_SYNTH_DSSS,
                          .snr          = 6.0,
                          .snr_mode     = 3, /* esno: outer data symbol */
                          .seed         = 7,
                          .sps          = 2,
                          .pn_length    = 7,
                          .acq_code     = { .bits = acq, .len = 8 },
                          .acq_reps     = 3,
                          .data_code    = { .bits = dcode, .len = 4 },
                          .sync         = { .bits = sync, .len = 2 },
                          .payload.bits = pay, /* payload */
                          .payload.len  = 5,
                          .crc          = 1 };
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
    wfm_source_t         bits = { .type      = WFM_SYNTH_BITS,
                                  .snr       = 6.0 - 10.0 * log10 (4.0 * 2.0),
                                  .snr_mode  = 1, /* fs */
                                  .seed      = 7,
                                  .sps       = 2,
                                  .pn_length = 7,
                                  .payload.bits = chips,
                                  .payload.len  = nchips,
                                  .modulation   = 1 };
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
    char *js = wfm_spec_to_json (gg, 1, 0, 0, 0, 0.0);
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
    nos.sync.bits           = NULL;
    nos.sync.len            = 0;
    wfm_compose_state_t *cn = wfm_compose_create (&gnos, 1, 0, 0);
    DP_REQUIRE_MSG (cn, "no-sync dsss create");
    size_t               nn  = 0;
    const wfm_segment_t *ggn = wfm_compose_segments (cn, &nn, NULL, NULL);
    char                *jn  = wfm_spec_to_json (ggn, 1, 0, 0, 0, 0.0);
    wfm_compose_destroy (cn);
    DP_REQUIRE_MSG (jn && !strstr (jn, "\"sync\""),
                    "absent sync key not emitted");
    free (jn);

    /* ── a coding stage reaches a DSSS burst ──────────────────────────
     *
     * The flags were read from the scene and then dropped: `--conv` on a
     * dsss source produced a byte-identical waveform to no `--conv` at all,
     * because the burst was assembled by a private four-field builder that
     * had never heard of a stage (doppler#1017). Now the burst is the same
     * description every other source's frame is, so a rate-1/2 inner code
     * doubles the frame -- and leaves the preamble alone, because the
     * preamble is not IN the description.
     */
    {
      wfm_source_t cv  = dsss;
      cv.convolutional = 1;
      wfm_segment_t gcv
          = { .sources = &cv, .n_sources = 1, .fs = 1e6, .off_samples = 0 };
      wfm_segment_t gpl
          = { .sources = &dsss, .n_sources = 1, .fs = 1e6, .off_samples = 0 };

      const size_t pre   = 8u * 3u; /* acq_code x acq_reps */
      const size_t frame = (2u + 5u + WFM_FRAME_CRC_BITS) * 4u;
      DP_REQUIRE_MSG (wfm_source_dsss_nchips (&dsss) == pre + frame,
                      "plain burst: preamble + spread frame");
      DP_REQUIRE_MSG (wfm_source_dsss_nchips (&cv) == pre + 2u * frame,
                      "a rate-1/2 inner code doubles the SPREAD part only");

      wfm_compose_state_t *cp = wfm_compose_create (&gpl, 1, 0, 0);
      wfm_compose_state_t *cc = wfm_compose_create (&gcv, 1, 0, 0);
      DP_REQUIRE_MSG (cp && cc, "both bursts compose");
      static float complex pbuf[4096], cbuf[4096];
      size_t               pn2 = wfm_compose_execute (cp, pbuf, 4096);
      size_t               cn2 = wfm_compose_execute (cc, cbuf, 4096);
      wfm_compose_destroy (cp);
      wfm_compose_destroy (cc);
      DP_REQUIRE_MSG (pn2 == (pre + frame) * 2u
                          && cn2 == (pre + 2u * frame) * 2u,
                      "the segment's on-time follows its own description");
      /* The preamble is the coherent pull-in target: a code over "the whole
         frame" must not touch it, or every receiver's acquisition breaks. */
      DP_REQUIRE_MSG (
          memcmp (pbuf, cbuf, pre * 2u * sizeof (float complex)) == 0,
          "the unspread preamble is identical with and without the "
          "inner code");
      /* ...and the spread part is NOT identical, or the stage did nothing. */
      DP_REQUIRE_MSG (memcmp (pbuf + pre * 2u, cbuf + pre * 2u,
                              frame * 2u * sizeof (float complex))
                          != 0,
                      "the inner code changed the frame it covers");

      /* A record that omits a stage is a capture nobody can rebuild. The
         SUM path writes its sources through a different function than the
         lone-source path (which keeps its own field order for byte
         identity), so both are checked -- the Python face covers the lone
         one, this covers the sum. */
      wfm_source_t  mix2[2] = { { .type = WFM_SYNTH_TONE, .freq = 0.1 }, cv };
      wfm_segment_t gmix    = { .sources     = mix2,
                                .n_sources   = 2,
                                .fs          = 1e6,
                                .num_samples = 64,
                                .off_samples = 0 };
      wfm_compose_state_t *cm = wfm_compose_create (&gmix, 1, 0, 0);
      DP_REQUIRE_MSG (cm, "a sum carrying a coded dsss source composes");
      size_t               nm  = 0;
      const wfm_segment_t *gm  = wfm_compose_segments (cm, &nm, NULL, NULL);
      char                *jm2 = wfm_spec_to_json (gm, 1, 0, 0, 0, 0.0);
      wfm_compose_destroy (cm);
      DP_REQUIRE_MSG (jm2, "sum spec serialises");
      DP_REQUIRE_MSG (strstr (jm2, "\"conv\"") != NULL,
                      "a summed source's record must name its inner code");
      free (jm2);
    }

    /* Invalid geometry (frame bits but no spreading code) is REFUSED at
     * create, on every face, and that is a deliberate change of answer. It
     * used to build and degrade the segment to a silent gap -- the same
     * "accepted and dropped" shape this file's own create-time check exists
     * to stop, and on the CLI it read as a zero-length capture with exit 0.
     * A DSSS burst spreads its frame, so a frame with nothing to spread it
     * by is a geometry no caller can have meant (doppler#1017). */
    wfm_source_t bad   = dsss;
    bad.data_code.bits = NULL;
    bad.data_code.len  = 0;
    wfm_segment_t gbad
        = { .sources = &bad, .n_sources = 1, .fs = 1e6, .off_samples = 4 };
    DP_REQUIRE_MSG (wfm_source_frame_error (&bad) != NULL,
                    "a spread frame with no data code is named as an error");
    DP_REQUIRE_MSG (wfm_compose_create (&gbad, 1, 0, 0) == NULL,
                    "bad dsss is refused at create, not silently gapped");

    /* Same answer inside a multi-source sum: one source that cannot be built
     * refuses the whole composition rather than summing the others and
     * quietly leaving this one out. */
    wfm_source_t  mix[2] = { { .type = WFM_SYNTH_TONE, .freq = 0.1 }, bad };
    wfm_segment_t gsum   = { .sources     = mix,
                             .n_sources   = 2,
                             .fs          = 1e6,
                             .num_samples = 16,
                             .off_samples = 4 };
    DP_REQUIRE_MSG (wfm_compose_create (&gsum, 1, 0, 0) == NULL,
                    "a sum carrying a bad dsss source is refused too");
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

    /* ── the DRAWN values, not just the timing ──────────────────────────
     *
     * wfm_compose_draws() answers "when AND what". The sidecar used to take
     * its timing from spans and its frequency/SNR from the source struct --
     * which for a ranged field holds `lo` -- so a row was exact about when
     * and wrong about what, and the exact half hid the other (doppler#1086).
     * These assert the two halves come from ONE walk: identical timing to
     * spans, one row per source, and values that actually vary. */
    {
      wfm_source_t rsrc = *gr2.sources;
      rsrc.freq         = 1e5;
      rsrc.freq_hi      = 2e5;
      rsrc.snr          = 8.0;
      rsrc.snr_hi       = 14.0;
      rsrc.ranged       = WFM_RANGE_FREQ | WFM_RANGE_SNR;
      wfm_segment_t gdw = gr2;
      gdw.sources       = &rsrc;
      gdw.n_sources     = 1;

      DP_REQUIRE_MSG (wfm_compose_draws (&gdw, 1, NULL, 0) == 3,
                      "size-then-fill: one row per source per instance");
      wfm_draw_t dr[8];
      size_t     ndr = wfm_compose_draws (&gdw, 1, dr, 8);
      DP_REQUIRE_MSG (ndr == 3, "three rows for three instances");

      wfm_span_t sp2[8];
      DP_REQUIRE_MSG (wfm_compose_spans (&gdw, 1, sp2, 8) == 3, "3 spans");
      for (size_t i = 0; i < 3; i++)
        DP_REQUIRE_MSG (
            dr[i].start == sp2[i].start && dr[i].delay == sp2[i].delay
                && dr[i].on == sp2[i].on && dr[i].off == sp2[i].off,
            "draws and spans report ONE timeline -- they walk "
            "the same draw, so they cannot disagree");

      for (size_t i = 0; i < 3; i++)
        {
          DP_REQUIRE_MSG (dr[i].seg == 0 && dr[i].instance == i
                              && dr[i].src == 0,
                          "each row names its own (segment, instance, src)");
          DP_REQUIRE_MSG (dr[i].freq >= 1e5 && dr[i].freq <= 2e5,
                          "a drawn freq lies inside its range");
          DP_REQUIRE_MSG (dr[i].snr >= 8.0 && dr[i].snr <= 14.0,
                          "a drawn snr lies inside its range");
        }
      /* The defect's signature: reading `lo` gives three identical rows
         sitting exactly on the bound. Both are refused. */
      DP_REQUIRE_MSG (dr[0].freq != dr[1].freq || dr[1].freq != dr[2].freq,
                      "per-instance freq draws are distinct -- three equal "
                      "values is what reading the range's lo looks like");
      DP_REQUIRE_MSG (dr[0].freq != 1e5 || dr[1].freq != 1e5,
                      "a drawn freq is not the range's lo");

      /* An UN-ranged field reports its scalar, so a consumer never has to
         branch on the `ranged` bitmask to know what it is looking at. */
      wfm_source_t fsrc = rsrc;
      fsrc.ranged       = 0;
      fsrc.freq         = 1234.0;
      fsrc.snr          = 5.5;
      fsrc.level        = -3.25;
      wfm_segment_t gfx = gdw;
      gfx.sources       = &fsrc;
      wfm_draw_t fx[8];
      DP_REQUIRE_MSG (wfm_compose_draws (&gfx, 1, fx, 8) == 3, "3 fixed");
      DP_REQUIRE_MSG (fx[0].freq == 1234.0 && fx[2].freq == 1234.0
                          && fx[1].snr == 5.5 && fx[1].level == -3.25,
                      "an un-ranged field reports its own scalar");
    }
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
    char                *jse = wfm_spec_to_json (gge, 1, 0, 0, 0, 0.0);
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
    char                *jd  = wfm_spec_to_json (gg2, 1, 0, 0, 0, 0.0);
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
    char                *j0  = wfm_spec_to_json (ggd, 1, 0, 0, 0, 0.0);
    wfm_compose_destroy (cjd);
    DP_REQUIRE_MSG (j0 && !strstr (j0, "\"delay_samples\"")
                        && !strstr (j0, "\"gap_noise\""),
                    "delay + gap_noise omitted at defaults");
    free (j0);

    /* JSON: repeats emitted (and only when > 1), round-trip byte-identical */
    wfm_compose_state_t *cj = wfm_compose_create (&g3, 1, 0, 0);
    size_t               nj = 0;
    const wfm_segment_t *gj = wfm_compose_segments (cj, &nj, NULL, NULL);
    char                *js = wfm_spec_to_json (gj, 1, 0, 0, 0, 0.0);
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
    char                *j1  = wfm_spec_to_json (g1j, 1, 0, 0, 0, 0.0);
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
    wfm_source_t plain   = { .type         = WFM_SYNTH_BITS,
                             .snr          = 100.0,
                             .sps          = 1,
                             .pn_length    = 7,
                             .payload.bits = (uint8_t *)payload,
                             .payload.len  = sizeof payload,
                             .modulation   = 1 /* bpsk */ };
    wfm_source_t framed  = plain;
    framed.acq_code.bits = acq_bits;
    framed.acq_code.len  = sizeof acq_bits;
    framed.acq_reps      = 4;
    framed.sync.bits     = sync_bits;
    framed.sync.len      = sizeof sync_bits;
    framed.crc           = 1;

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

    /* A PN-sourced waveform CAN be framed now, given a payload. #755 refused
       --type bpsk outright because the synth's LFSR is endless and nothing
       said where the payload stopped; a payload length is that bound, so the
       question is the one BITS always answered -- is there a payload at all
       (gh-762). */
    wfm_source_t framed_pn = framed;
    framed_pn.type         = WFM_SYNTH_BPSK; /* symbols from the PN LFSR */
    DP_REQUIRE_MSG (wfm_source_frame_error (&framed_pn) == NULL,
                    "a bounded payload is what a framed PN-sourced waveform "
                    "was ever missing");
    wfm_synth_state_t *psy = wfm_source_to_synth (&framed_pn, 1e6);
    DP_REQUIRE_MSG (psy, "and it BUILDS on the standalone face");
    wfm_synth_destroy (psy);

    /* The same source with a GENERATED payload -- the shape --payload-len
       resolves to, and what makes a 100k-bit frame six numbers in a record. */
    wfm_source_t framed_gen = framed_pn;
    framed_gen.payload
        = (wfm_seq_t){ .kind = WFM_SEQ_PN, .len = 64, .reg_bits = 7 };
    DP_REQUIRE_MSG (wfm_source_frame_error (&framed_gen) == NULL,
                    "a generated payload is a payload -- tested on LENGTH, "
                    "never on the array a generated kind does not have");
    wfm_synth_state_t *gsy = wfm_source_to_synth (&framed_gen, 1e6);
    DP_REQUIRE_MSG (gsy, "a framed waveform whose payload is GENERATED "
                         "builds -- the last place gh-762's flattening "
                         "survived was this descriptor's payload field");
    wfm_synth_destroy (gsy);

    wfm_source_t framed_empty = framed;
    framed_empty.payload.bits = NULL;
    framed_empty.payload.len  = 0;
    DP_REQUIRE_MSG (wfm_source_frame_error (&framed_empty) != NULL,
                    "a frame with no payload is refused");

    /* A type that carries no bit stream at all still cannot be framed, and
       the refusal REACHES the caller on both faces. Asserting only that the
       predicate returns a message would leave the two construction paths free
       to ignore it — which is precisely the shape of the bug: the fields were
       validated nowhere and dropped silently. */
    wfm_source_t framed_chirp = framed;
    framed_chirp.type         = WFM_SYNTH_CHIRP;
    DP_REQUIRE_MSG (wfm_source_frame_error (&framed_chirp) != NULL,
                    "a chirp has no bit stream to frame, payload or not");
    DP_REQUIRE_MSG (!wfm_source_to_synth (&framed_chirp, 1.0),
                    "the standalone face refuses a frame the waveform type "
                    "cannot carry");
    wfm_segment_t bad_seg = { .sources     = &framed_chirp,
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
    wfm_source_t broken  = framed;
    broken.acq_code.bits = NULL; /* .len and acq_reps still set */
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

  /* ── the interleaver's span INCLUDES the outer code's check symbols ──
   *
   * `ccsds_tm_frame_desc_of` gives the interleave stage the whole data group
   * -- "payload, its CRC, and the outer code's check symbols" -- and the
   * flag guard validated payload + CRC only. So the check ran against a
   * DIFFERENT span from the one the stage permutes, and refused the
   * canonical CCSDS arrangement: 223 octets under RS(255,223) interleaved 5
   * deep at unit 8. 1784 bits does not divide by 40; the 2040 the stage
   * actually covers divides exactly 51 times.
   *
   * That refusal was pinned in the flag-matrix golden as an expected exit 2,
   * which is how a guard rejecting valid input survives: the evidence for
   * the flag was the failure it caused. */
  {
    static uint8_t frame[223 * 8]; /* 223 octets, the RS(255,223) message */
    for (size_t i = 0; i < sizeof frame; i++)
      frame[i] = (uint8_t)(i & 1u);

    wfm_source_t cadu = { .type                 = WFM_SYNTH_BITS,
                          .snr                  = 100.0,
                          .sps                  = 1,
                          .pn_length            = 7,
                          .modulation           = 1,
                          .payload.bits         = frame,
                          .payload.len          = sizeof frame,
                          .crc                  = 0,
                          .rs_depth             = 1,
                          .interleave_depth     = 5,
                          .interleave_unit_bits = 8 };
    DP_REQUIRE_MSG (wfm_source_frame_error (&cadu) == NULL,
                    "223 octets + RS parity is 2040 bits, which 5 x 8 "
                    "divides 51 times -- the arrangement CCSDS specifies");

    /* The guard still bites, or removing it would have passed the case
       above just as well. It has to be provoked WITHOUT disturbing the outer
       code's own geometry: shortening the payload trips the --rs-depth guard
       three statements earlier, which returns a different sentence and
       satisfies a bare `!= NULL` while never reaching this one. So keep the
       223 octets and change the depth: 2040 divides by 5*8 and does not
       divide by 2*8, because 255 is odd. */
    wfm_source_t odd_depth     = cadu;
    odd_depth.interleave_depth = 2;
    const char *why            = wfm_source_frame_error (&odd_depth);
    DP_REQUIRE_MSG (why != NULL,
                    "a data group that is not a whole number of units is "
                    "still refused");
    /* And refused BY THIS GUARD -- asserting only that some sentence came
       back is what let the case above pass on the outer code's message. */
    DP_REQUIRE_MSG (strstr (why, "--interleave") == why,
                    "the refusal has to name --interleave, not whichever "
                    "guard happened to fire first");

    /* And without an outer code the span is payload + CRC, unchanged: 16
       payload bits and no CRC is two units of 8, so depth 2 divides it. */
    wfm_source_t         no_outer    = cadu;
    static const uint8_t sixteen[16] = { 0 };
    no_outer.payload.bits            = (uint8_t *)sixteen;
    no_outer.payload.len             = sizeof sixteen;
    no_outer.rs_depth                = 0;
    no_outer.interleave_depth        = 2;
    DP_REQUIRE_MSG (wfm_source_frame_error (&no_outer) == NULL,
                    "with no outer code the group is payload + CRC");
  }

  /* ── the bridge builds by NAME, and it must build the SAME frame ──────
   *
   * `wfm_source_describe_frame` used to fill a CCSDS-shaped spec struct and
   * hand it to `ccsds_tm_frame_desc_of`, which made a standard's vocabulary
   * the only vocabulary -- a frame doppler had never seen had to be spelled
   * in CCSDS's slots or not at all. It now builds through the general
   * by-name builder instead.
   *
   * The falsification is equivalence with the path it replaces, over the
   * shapes a source can take, asserted on the ASSEMBLED BITS rather than on
   * the two structs: a description that merely looked alike would prove
   * nothing about which bits each stage touched. Both paths run the same
   * CCSDS kernels, so any difference is the description.
   *
   * A refusal counts as agreement -- both paths must refuse the same shapes
   * -- but refusals alone would be a vacuous pass, so the number of cases
   * that actually assembled is asserted at the end.
   */
  {
    static uint8_t payload[223 * 8];
    for (size_t i = 0; i < sizeof payload; i++)
      payload[i] = (uint8_t)((i * 7u + (i >> 3)) & 1u); /* structured */
    static const uint8_t syncw[13] = { 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1 };
    static const uint8_t pre[8]    = { 1, 0, 1, 0, 1, 0, 1, 0 };

    const struct
    {
      int         asm_, crc, rs, rand, conv;
      unsigned    ilv, unit;
      size_t      n_sync, n_pre, reps;
      const char *what;
    } CASES[] = {
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "payload alone" },
      { 0, 1, 0, 0, 0, 0, 0, 13, 0, 0, "sync + CRC" },
      { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, "ASM + CRC" },
      { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, "outer code" },
      { 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, "CRC inside the outer code" },
      { 1, 0, 1, 2, 0, 0, 0, 0, 0, 0, "ASM + outer + randomiser" },
      { 1, 0, 1, 1, 0, 5, 8, 0, 0, 0, "and an interleaver" },
      { 1, 0, 1, 1, 1, 5, 8, 0, 0, 0, "and the inner code" },
      { 0, 1, 0, 0, 0, 0, 0, 13, 8, 4, "a preamble, repeated" },
    };

    size_t assembled = 0;
    for (size_t c = 0; c < sizeof CASES / sizeof CASES[0]; c++)
      {
        /* The outer code needs payload PLUS its CRC to be exactly
           223*depth octets -- virtual fill is not implemented and a short
           frame is refused rather than padded (the source's own guard says
           so). Size the payload to match, or the case tests a refusal
           instead of a frame. */
        size_t n_bits = sizeof payload;
        if (CASES[c].rs)
          n_bits = (size_t)223u * (size_t)CASES[c].rs * 8u
                   - (CASES[c].crc ? WFM_FRAME_CRC_BITS : 0u);

        wfm_source_t src = { .type                 = WFM_SYNTH_BITS,
                             .snr                  = 100.0,
                             .sps                  = 1,
                             .pn_length            = 7,
                             .modulation           = 1,
                             .payload.bits         = payload,
                             .payload.len          = n_bits,
                             .attach_asm           = CASES[c].asm_,
                             .crc                  = CASES[c].crc,
                             .rs_depth             = (unsigned)CASES[c].rs,
                             .randomise            = CASES[c].rand,
                             .convolutional        = CASES[c].conv,
                             .interleave_depth     = CASES[c].ilv,
                             .interleave_unit_bits = CASES[c].unit };
        if (CASES[c].n_sync)
          {
            src.sync.bits = syncw;
            src.sync.len  = CASES[c].n_sync;
          }
        if (CASES[c].n_pre)
          {
            src.acq_code.bits = pre;
            src.acq_code.len  = CASES[c].n_pre;
            src.acq_reps      = CASES[c].reps;
          }

        /* The new path. */
        wfm_frame_desc_t by_name;
        DP_REQUIRE_MSG (wfm_source_describe_frame (&src, &by_name) == 0,
                        CASES[c].what);

        /* The path it replaces, spelled the only way that struct allows. */
        const ccsds_tm_frame_spec_t sp = {
          .attach_asm           = src.attach_asm,
          .preamble             = src.acq_code.bits,
          .preamble_len         = src.acq_code.len,
          .preamble_reps        = src.acq_reps,
          .sync                 = src.sync.bits,
          .sync_len             = src.sync.len,
          .payload              = src.payload.bits,
          .payload_len          = src.payload.len,
          .crc                  = src.crc,
          .rs_depth             = src.rs_depth,
          .randomise            = src.randomise,
          .convolutional        = src.convolutional,
          .interleave_depth     = src.interleave_depth,
          .interleave_unit_bits = src.interleave_unit_bits,
        };
        wfm_frame_desc_t by_spec;
        DP_REQUIRE (ccsds_tm_frame_desc_of (&sp, &by_spec) == 0);

        wfm_frame_desc_layout_t la, lb;
        DP_REQUIRE (wfm_frame_desc_layout (&by_name, &la) == 0);
        DP_REQUIRE (wfm_frame_desc_layout (&by_spec, &lb) == 0);
        DP_REQUIRE_MSG (la.out_bits == lb.out_bits, CASES[c].what);

        uint8_t *a = (uint8_t *)malloc (la.out_bits);
        uint8_t *b = (uint8_t *)malloc (lb.out_bits);
        DP_REQUIRE (a != NULL && b != NULL);

        wfm_frame_ops_t ops;
        ccsds_tm_frame_ops (&ops, NULL);
        const size_t na = wfm_frame_assemble (&by_name, &ops, a, la.out_bits);
        ccsds_tm_frame_ops (&ops, NULL); /* a fresh inner-code register */
        const size_t nb = wfm_frame_assemble (&by_spec, &ops, b, lb.out_bits);

        DP_REQUIRE_MSG (na == nb, CASES[c].what);
        if (na)
          {
            DP_REQUIRE_MSG (memcmp (a, b, na) == 0, CASES[c].what);
            assembled++;
          }
        free (a);
        free (b);
      }

    /* Refusals agreeing is agreement, but it is not evidence about frames.
       Most of these have to have actually produced bits. */
    DP_REQUIRE_MSG (assembled >= 8,
                    "the equivalence cases must mostly ASSEMBLE, or the "
                    "comparison is between two refusals");
  }

  /* ── a GENERATED sync reaches the wire THROUGH THE SOURCE ─────────────
   *
   * gh-762 step 2. `wfm_seq_t` has had four kinds all along and the frame
   * layer materialises every one of them, but no caller could spell
   * anything but LITERAL: `wfm_source_describe_frame` rebuilt each field as
   * a fresh literal, so a source's kind was discarded one call before the
   * descriptor could see it. The source now carries `wfm_seq_t` (step 1) and
   * the bridge passes it through, which is the whole change.
   *
   * The truth is `pn_generate` over the same three numbers -- an EXTERNAL
   * one. A round trip through the frame would agree with itself perfectly
   * while regenerating the wrong sequence, which is exactly the shape that
   * let a Gold field sit differentially-checked and wrong.
   */
  {
    static uint8_t pay[64];
    for (size_t i = 0; i < sizeof pay; i++)
      pay[i] = (uint8_t)((i * 5u + 1u) & 1u);

    wfm_source_t src;
    memset (&src, 0, sizeof src);
    src.type          = WFM_SYNTH_BITS;
    src.sps           = 2;
    src.payload.bits  = pay;
    src.payload.len   = sizeof pay;
    src.sync.kind     = WFM_SEQ_PN;
    src.sync.len      = 31u; /* one period of a 5-bit register */
    src.sync.reg_bits = 5u;
    src.sync.seed     = 3u;

    /* A generated sequence has NO array, so a source that tested its frame
       on the pointer read this as unframed and emitted the payload bare. */
    DP_REQUIRE_MSG (wfm_source_has_frame (&src),
                    "a PN sync frames a source -- the test is on length, "
                    "not on an array a generated kind never has");
    DP_REQUIRE_MSG (wfm_source_frame_error (&src) == NULL,
                    "and it is a buildable shape");

    wfm_frame_desc_t d;
    DP_REQUIRE_MSG (wfm_source_describe_frame (&src, &d) == 0, "describe");
    const int i = wfm_frame_field_index (&d, "sync");
    DP_REQUIRE_MSG (i >= 0, "the sync field is there by name");
    DP_REQUIRE_MSG (d.field[i].seq.kind == WFM_SEQ_PN,
                    "and it is still a PN field -- the kind SURVIVED the "
                    "bridge, which is the whole of gh-762 step 2");

    wfm_frame_desc_layout_t l;
    DP_REQUIRE_MSG (wfm_frame_desc_layout (&d, &l) == 0, "layout");
    static uint8_t got[4096];
    DP_REQUIRE_MSG (wfm_frame_assemble (&d, NULL, got, sizeof got)
                        == l.out_bits,
                    "and the frame assembles");

    static uint8_t want[31];
    pn_state_t    *pn = pn_create (pn_mls_poly (5u), 3u, 5u, 0);
    DP_REQUIRE_MSG (pn != NULL, "pn_create");
    DP_REQUIRE_MSG (pn_generate (pn, 31u, want, 31u) == 31u, "pn_generate");
    pn_destroy (pn);
    DP_REQUIRE_MSG (memcmp (got + l.field_off[i], want, 31u) == 0,
                    "a PN sync declared on the SOURCE is pn_generate of its "
                    "own three numbers, at the offset the layout promised");

    /* A literal source still describes a literal, unchanged. Both
       directions: a bridge that stamped PN on everything would pass the
       assertion above and break every existing caller. */
    static const uint8_t lit[4] = { 1, 0, 0, 1 };
    wfm_source_t         plain  = src;
    memset (&plain.sync, 0, sizeof plain.sync);
    plain.sync.kind = WFM_SEQ_LITERAL;
    plain.sync.bits = lit;
    plain.sync.len  = sizeof lit;
    wfm_frame_desc_t d2;
    DP_REQUIRE_MSG (wfm_source_describe_frame (&plain, &d2) == 0, "describe");
    const int j = wfm_frame_field_index (&d2, "sync");
    DP_REQUIRE (j >= 0);
    DP_REQUIRE_MSG (d2.field[j].seq.kind == WFM_SEQ_LITERAL
                        && d2.field[j].seq.bits == lit,
                    "a literal sync is still literal, and still the caller's "
                    "own array rather than a copy");
  }

  /* ── a GENERATED sequence SURVIVES --record → --from-file ─────────────
   *
   * gh-762 step 3. `add_bit_string` writes nothing when there are no bits,
   * and a generated sequence has none by definition -- so before this a PN
   * sync VANISHED from the record and `--from-file` rebuilt an unframed
   * waveform at exit 0. That is the same silent-unframed shape
   * `add_frame_fields`'s own comment warns about for the type gate, and it
   * is worse here: the whole reason to carry (poly, seed, reg_bits) instead
   * of a million-symbol array is that the METADATA reproduces the capture.
   *
   * Asserted on the assembled BITS, not on the struct: a record that merely
   * looked alike would prove nothing about the waveform it rebuilds.
   */
  {
    static uint8_t pay[64];
    for (size_t i = 0; i < sizeof pay; i++)
      pay[i] = (uint8_t)((i * 3u + 1u) & 1u);

    wfm_source_t src;
    memset (&src, 0, sizeof src);
    src.type          = WFM_SYNTH_BITS;
    src.sps           = 2;
    src.payload.bits  = pay;
    src.payload.len   = sizeof pay;
    src.sync.kind     = WFM_SEQ_PN;
    src.sync.len      = 31u;
    src.sync.reg_bits = 5u;
    src.sync.seed     = 3u;
    src.sync.poly     = 0u; /* derive the maximal-length polynomial */

    wfm_segment_t seg
        = { .sources = &src, .n_sources = 1, .fs = 1e6, .num_samples = 256 };
    char *js = wfm_spec_to_json (&seg, 1, 0, 0, 0, 0.0);
    DP_REQUIRE_MSG (js, "to_json");
    DP_REQUIRE_MSG (strstr (js, "\"sync_gen\""),
                    "a generated sync is RECORDED -- it has no bit string, "
                    "so without its own key the field left no trace at all");
    DP_REQUIRE_MSG (!strstr (js, "\"sync\":"),
                    "and not also as a literal: one field, one source of "
                    "bits");
    DP_REQUIRE_MSG (strstr (js, "\"kind\":\"pn\"")
                        || strstr (js, "\"kind\":\t\"pn\""),
                    "the kind is named");

    wfm_compose_state_t *jc = wfm_compose_from_json (js);
    DP_REQUIRE_MSG (jc, "from_json");

    /* The bits the reloaded description assembles must equal the original's,
       and must equal pn_generate -- an EXTERNAL truth, so a record that
       round-tripped its own mistake perfectly would still fail. */
    wfm_frame_desc_t d;
    DP_REQUIRE_MSG (wfm_source_describe_frame (&src, &d) == 0, "describe");
    const int i = wfm_frame_field_index (&d, "sync");
    DP_REQUIRE (i >= 0);
    wfm_frame_desc_layout_t l;
    DP_REQUIRE (wfm_frame_desc_layout (&d, &l) == 0);
    static uint8_t got[4096];
    DP_REQUIRE (wfm_frame_assemble (&d, NULL, got, sizeof got) == l.out_bits);

    static uint8_t want[31];
    pn_state_t    *pn = pn_create (pn_mls_poly (5u), 3u, 5u, 0);
    DP_REQUIRE (pn != NULL);
    DP_REQUIRE (pn_generate (pn, 31u, want, 31u) == 31u);
    pn_destroy (pn);
    DP_REQUIRE_MSG (memcmp (got + l.field_off[i], want, 31u) == 0,
                    "the recorded PN sync is pn_generate of its own numbers");

    wfm_compose_destroy (jc);
    free (js);

    /* GOLD and DOTTED through the same round trip, and a DSSS source's
       spreading code, so every branch of the codec is driven by a test
       rather than by one kind standing in for three. The Gold taps are the
       header's own worked example, and the check is again EXTERNAL --
       gold_generate of the same five numbers. */
    {
      wfm_source_t g;
      memset (&g, 0, sizeof g);
      g.type               = WFM_SYNTH_DSSS;
      g.sps                = 2;
      g.payload.bits       = pay;
      g.payload.len        = 8;
      g.acq_reps           = 2;
      g.acq_code.kind      = WFM_SEQ_DOTTED;
      g.acq_code.len       = 8u;
      g.data_code.kind     = WFM_SEQ_PN;
      g.data_code.len      = 7u;
      g.data_code.reg_bits = 3u;
      g.data_code.seed     = 1u;
      g.sync.kind          = WFM_SEQ_GOLD;
      g.sync.len           = 16u;
      g.sync.reg_bits      = 10u;
      g.sync.taps_a        = 934u;
      g.sync.seed_a        = 350u;
      g.sync.taps_b        = 567u;
      g.sync.seed_b        = 73u;

      wfm_segment_t gseg
          = { .sources = &g, .n_sources = 1, .fs = 1e6, .num_samples = 512 };
      char *gjs = wfm_spec_to_json (&gseg, 1, 0, 0, 0, 0.0);
      DP_REQUIRE_MSG (gjs, "gold/dotted to_json");
      DP_REQUIRE_MSG (strstr (gjs, "\"sync_gen\"")
                          && strstr (gjs, "\"acq_code_gen\"")
                          && strstr (gjs, "\"data_code_gen\""),
                      "all three sequences record their generators");
      /* Checked as two pieces: cJSON separates a key from its value with a
         tab, and pinning the whitespace would make this a formatting test. */
      DP_REQUIRE_MSG (strstr (gjs, "\"taps_a\"") && strstr (gjs, "0x3a6"),
                      "a Gold tap mask is recorded as HEX -- a uint64 does "
                      "not survive a JSON number");

      wfm_compose_state_t *gc = wfm_compose_from_json (gjs);
      DP_REQUIRE_MSG (gc, "gold/dotted from_json");
      wfm_compose_destroy (gc);
      free (gjs);

      /* The Gold sync's bits, against gold_generate of the same numbers. */
      wfm_frame_desc_t gd;
      DP_REQUIRE (wfm_source_describe_frame (&g, &gd) == 0);
      const int gi = wfm_frame_field_index (&gd, "sync");
      DP_REQUIRE (gi >= 0);
      wfm_frame_desc_layout_t gl;
      DP_REQUIRE (wfm_frame_desc_layout (&gd, &gl) == 0);
      static uint8_t gbits[4096];
      DP_REQUIRE (wfm_frame_assemble (&gd, NULL, gbits, sizeof gbits)
                  == gl.out_bits);
      static uint8_t gwant[16];
      gold_state_t  *gs = gold_create (934u, 350u, 567u, 73u, 10u);
      DP_REQUIRE (gs != NULL);
      DP_REQUIRE (gold_generate (gs, 16u, gwant, 16u) == 16u);
      gold_destroy (gs);
      DP_REQUIRE_MSG (memcmp (gbits + gl.field_off[gi], gwant, 16u) == 0,
                      "a recorded Gold sync IS gold_generate of its own "
                      "five numbers");
    }

    /* Refusals. Each of these BUILDS a waveform if ignored rather than
       refused, and it is not the recorded one -- which is the one failure a
       record exists to prevent. */
    DP_REQUIRE_MSG (
        !wfm_compose_from_json (
            "{\"segments\":[{\"fs\":1e6,\"num_samples\":16,\"type\":\"bits\","
            "\"pattern\":\"0101\",\"sync_gen\":{\"kind\":\"martian\","
            "\"len\":8}}]}"),
        "an unknown kind is refused, not silently dropped -- a newer writer's "
        "record must not load as a different waveform");
    DP_REQUIRE_MSG (
        !wfm_compose_from_json (
            "{\"segments\":[{\"fs\":1e6,\"num_samples\":16,\"type\":\"bits\","
            "\"pattern\":\"0101\",\"sync\":\"0110\","
            "\"sync_gen\":{\"kind\":\"pn\",\"len\":8,\"reg_bits\":3}}]}"),
        "a field carrying BOTH a literal and a generator is refused");
    DP_REQUIRE_MSG (
        !wfm_compose_from_json (
            "{\"segments\":[{\"fs\":1e6,\"num_samples\":16,\"type\":\"bits\","
            "\"pattern\":\"0101\",\"sync_gen\":{\"kind\":\"pn\",\"len\":8,"
            "\"reg_bits\":0}}]}"),
        "a PN with no register width is refused");
    DP_REQUIRE_MSG (
        !wfm_compose_from_json (
            "{\"segments\":[{\"fs\":1e6,\"num_samples\":16,\"type\":\"bits\","
            "\"pattern\":\"0101\",\"sync_gen\":5}]}"),
        "a generator block that is not an object is refused -- a scalar there "
        "is a writer this reader does not understand");
    DP_REQUIRE_MSG (
        !wfm_compose_from_json (
            "{\"segments\":[{\"fs\":1e6,\"num_samples\":16,\"type\":\"bits\","
            "\"pattern\":\"0101\",\"sync_gen\":{\"kind\":\"pn\","
            "\"reg_bits\":5}}]}"),
        "a generator with no length is refused -- length is the one parameter "
        "no default can supply");
    DP_REQUIRE_MSG (
        !wfm_compose_from_json (
            "{\"segments\":[{\"fs\":1e6,\"num_samples\":16,\"type\":\"bits\","
            "\"pattern\":\"0101\",\"sync_gen\":{\"kind\":\"gold\",\"len\":8,"
            "\"reg_bits\":65}}]}"),
        "a Gold register wider than the 64 bits gold_create() holds is "
        "refused, not silently masked down");
    DP_REQUIRE_MSG (
        !wfm_compose_from_json (
            "{\"segments\":[{\"fs\":1e6,\"num_samples\":16,\"type\":\"dsss\","
            "\"data_code_gen\":{\"kind\":\"martian\",\"len\":8}}]}"),
        "a malformed data_code_gen is refused on the dsss source too -- the "
        "spread half reads its generators through the same gate");
  }

  /* ── the chip path MATERIALISES a generated code ─────────────────
   *
   * The DSSS chip builders take raw arrays, never a description, so a
   * generated `data_code`/`acq_code` -- whose parameters ARE the code, with
   * `bits == NULL` -- has to be expanded before it reaches them. Read through
   * as a pointer it dereferenced NULL; refused on the pointer it became "no
   * code at all". Both are asserted on the STANDALONE face, because that is
   * the one a `--from-file` record restores through. */
  {
    static const uint8_t dcode4[4] = { 0, 1, 1, 0 };
    static const uint8_t sync2[2]  = { 1, 0 };
    static uint8_t       pay5[5]   = { 1, 0, 0, 1, 1 };

    /* CONTINUOUS, spread by a generated PN. This is exactly the shape a
       recorded `data_code_gen` restores to. */
    wfm_source_t cgen
        = { .type        = WFM_SYNTH_DSSS,
            .snr         = 40.0,
            .snr_mode    = 1,
            .seed        = 7,
            .sps         = 2,
            .pn_length   = 7,
            .symbol_rate = 1e4,
            .data_code   = { .kind = WFM_SEQ_PN, .len = 31, .reg_bits = 5 } };
    wfm_synth_state_t *cs = wfm_source_to_synth (&cgen, 1e6);
    DP_REQUIRE_MSG (cs,
                    "a continuous stream spread by a GENERATED code builds -- "
                    "a generated code carries no array, so a pointer test "
                    "refused every recorded data_code_gen on this face");
    wfm_synth_destroy (cs);

    /* A burst with NO acquisition preamble. The expansion still runs over the
       absent field and must yield nothing rather than invent one. */
    wfm_source_t       nopre = { .type         = WFM_SYNTH_DSSS,
                                 .snr          = 40.0,
                                 .snr_mode     = 1,
                                 .seed         = 7,
                                 .sps          = 2,
                                 .pn_length    = 7,
                                 .data_code    = { .bits = dcode4, .len = 4 },
                                 .sync         = { .bits = sync2, .len = 2 },
                                 .payload.bits = pay5,
                                 .payload.len  = 5,
                                 .crc          = 1 };
    wfm_synth_state_t *ns    = wfm_source_to_synth (&nopre, 1e6);
    DP_REQUIRE_MSG (ns, "a burst with a sync word and no preamble is a burst");
    wfm_synth_destroy (ns);

    /* A code that is DECLARED and unbuildable -- a length with no array -- is
       refused on BOTH chip paths. Spreading whatever the fresh buffer held
       would produce a capture no receiver can be scored against. */
    wfm_source_t cbad = cgen;
    cbad.data_code    = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .len = 8 };
    DP_REQUIRE_MSG (!wfm_source_to_synth (&cbad, 1e6),
                    "a continuous spreading code with a length and no bits is "
                    "refused");

    wfm_source_t bbad = nopre;
    bbad.acq_code     = (wfm_seq_t){ .kind = WFM_SEQ_LITERAL, .len = 8 };
    bbad.acq_reps     = 3;
    DP_REQUIRE_MSG (!wfm_source_to_synth (&bbad, 1e6),
                    "a burst preamble with a length and no bits is refused");
  }

  /* ── clock Doppler (gh-942) ───────────────────────────────────────────
   *
   * The regression that matters is BLOCK SIZE, not the split point. A
   * Doppler channel is a resampler: it consumes ~n*(1+d) inputs per n
   * outputs, so the composer's "pull k, get k" only holds because the
   * renderer keeps a holdover. Get that wrong and the output depends on how
   * the caller happened to chunk its reads -- which #939 shows a
   * split-resume test does NOT catch, because both halves can fit inside
   * one internal block and never exercise the boundary at all.
   *
   * So: render the same scene through a range of block sizes, including
   * ones that are not divisors of the feed, and require every one to be
   * bit-identical to a single-call render.
   */
  {
    wfm_source_t  src = { .type       = WFM_SYNTH_TONE,
                          .freq       = 1e5,
                          .snr        = 40.0,
                          .seed       = 7,
                          .doppler    = 25.0, /* ppm */
                          .carrier_hz = 2.5e9 };
    wfm_segment_t seg
        = { .sources = &src, .n_sources = 1, .fs = 1e6, .num_samples = 20000 };

    enum
    {
      N = 20000
    };
    static float complex ref[N], got[N];

    wfm_compose_state_t *c = wfm_compose_create (&seg, 1, 0, 0);
    DP_REQUIRE_MSG (c, "a doppler source composes");
    size_t nref = 0;
    for (size_t n; (n = wfm_compose_execute (c, ref + nref, N - nref)) > 0;)
      nref += n;
    wfm_compose_destroy (c);
    DP_REQUIRE_MSG (nref == N, "doppler scene still yields its full on-time");

    /* Deliberately awkward sizes: 1 exercises the holdover every sample,
       4096 is exactly the internal feed, and 4095/4097 straddle it. */
    static const size_t blocks[] = { 1, 3, 511, 1000, 4095, 4096, 4097, 9973 };
    for (size_t bi = 0; bi < sizeof blocks / sizeof *blocks; bi++)
      {
        size_t b = blocks[bi];
        c        = wfm_compose_create (&seg, 1, 0, 0);
        DP_REQUIRE (c);
        size_t ngot = 0;
        for (size_t n; ngot < N;)
          {
            size_t want = (N - ngot < b) ? N - ngot : b;
            n           = wfm_compose_execute (c, got + ngot, want);
            if (n == 0)
              break;
            ngot += n;
          }
        wfm_compose_destroy (c);
        DP_REQUIRE_MSG (ngot == nref,
                        "a doppler render's LENGTH is block-size invariant");
        DP_REQUIRE_MSG (memcmp (ref, got, nref * sizeof *ref) == 0,
                        "a doppler render is bit-identical at every block "
                        "size -- the holdover, not the chunking, decides it");
      }

    /* The channel must actually be doing something, or the invariance above
       is the invariance of a no-op. 25 ppm over 20000 samples at 1 MHz is
       half a sample of dilation and a 62.5 kHz carrier term, so the same
       scene without Doppler must differ. */
    wfm_source_t plain = src;
    plain.doppler      = 0.0;
    plain.carrier_hz   = 0.0;
    wfm_segment_t pseg = seg;
    pseg.sources       = &plain;
    c                  = wfm_compose_create (&pseg, 1, 0, 0);
    DP_REQUIRE (c);
    size_t npl = 0;
    for (size_t n; (n = wfm_compose_execute (c, got + npl, N - npl)) > 0;)
      npl += n;
    wfm_compose_destroy (c);
    DP_REQUIRE_MSG (memcmp (ref, got, nref * sizeof *ref) != 0,
                    "doppler changes the waveform (else the block-size "
                    "invariance above proves nothing)");
  }

  /* ── a PERSIST pass keeps moving through the gap ──────────────────────
   *
   * An emitter does not stop moving because its burst ended, so the channel
   * runs over the off-time too -- on the noise floor, which is what a gap
   * carries. The observable consequence is that a LONGER gap leaves the pass
   * further along by the time the next burst starts.
   *
   * Two renders of the same two-burst scene differing ONLY in gap length,
   * compared over the second burst. If the channel were skipped over gaps
   * (or reset per instance), the second burst would be identical in both and
   * this would fail -- which is exactly what makes it a test of the
   * behaviour rather than of the plumbing.
   *
   * The source is CLEAN, so gaps are exact zeros and the synth's AWGN stream
   * cannot itself carry the gap-length difference into burst 2. The synths
   * are torn down per instance, so burst 2's signal is otherwise identical
   * between the two renders: the channel is the only thing that can differ.
   */
  {
    enum
    {
      B    = 4000, /* burst   */
      G1   = 1000, /* short gap */
      G2   = 9000, /* long gap  */
      CAP2 = 2 * (B + G2)
    };
    static float complex a[CAP2], b[CAP2];

    wfm_source_t src = { .type             = WFM_SYNTH_TONE,
                         .freq             = 5e4,
                         .snr              = WFM_SYNTH_SNR_CLEAN,
                         .seed             = 11,
                         .doppler          = 5.0,   /* ppm   */
                         .doppler_rate     = 200.0, /* ppm/s */
                         .carrier_hz       = 2.0e9,
                         .doppler_lifetime = WFM_DOPPLER_PERSIST };

    size_t         got[2];
    float complex *bufs[2] = { a, b };
    const size_t   gaps[2] = { G1, G2 };
    for (int g = 0; g < 2; g++)
      {
        wfm_segment_t        seg = { .sources     = &src,
                                     .n_sources   = 1,
                                     .fs          = 1e6,
                                     .num_samples = B,
                                     .off_samples = gaps[g],
                                     .repeats     = 2 };
        wfm_compose_state_t *c   = wfm_compose_create (&seg, 1, 0, 0);
        DP_REQUIRE_MSG (c, "a persisting-doppler scene composes");
        size_t nn = 0;
        for (size_t n;
             (n = wfm_compose_execute (c, bufs[g] + nn, CAP2 - nn)) > 0;)
          nn += n;
        wfm_compose_destroy (c);
        got[g] = nn;
      }

    /* Burst 1 starts at 0 in both and has seen no gap yet: identical. */
    DP_REQUIRE_MSG (memcmp (a, b, B * sizeof *a) == 0,
                    "the FIRST burst is unaffected by what follows it");
    /* Burst 2 starts after the gap in each render. */
    DP_REQUIRE_MSG (got[0] >= (size_t)(B + G1 + B)
                        && got[1] >= (size_t)(B + G2 + B),
                    "both renders reached their second burst");
    DP_REQUIRE_MSG (memcmp (a + B + G1, b + B + G2, B * sizeof *a) != 0,
                    "a longer gap leaves the pass further along -- the "
                    "channel runs on the noise floor while the burst is "
                    "absent, so doppler_rate is per SECOND, not per unit "
                    "of on-time");

    /* And the LIMIT of that, pinned so the header's claim stays true: the
       channel is keyed by (segment, source), because a position is the only
       source identity the composer has. Two SEGMENTS declaring the same
       parameters are therefore two passes, not one continued -- each starts
       at its own t=0, so segment 1's burst matches segment 0's rather than
       carrying on from it. Sharing across segments needs a declared source
       id that the scene format does not have (gh-942). */
    wfm_source_t two_src = src;
    two_src.doppler_rate = 0.0; /* offset only: a pass that has not moved */
    wfm_segment_t two[2] = {
      { .sources = &two_src, .n_sources = 1, .fs = 1e6, .num_samples = B },
      { .sources = &two_src, .n_sources = 1, .fs = 1e6, .num_samples = B }
    };
    wfm_compose_state_t *c2 = wfm_compose_create (two, 2, 0, 0);
    DP_REQUIRE_MSG (c2, "a two-segment persisting scene composes");
    size_t n2 = 0;
    for (size_t n; (n = wfm_compose_execute (c2, a + n2, CAP2 - n2)) > 0;)
      n2 += n;
    wfm_compose_destroy (c2);
    DP_REQUIRE_MSG (n2 == 2 * B, "both segments rendered");
    DP_REQUIRE_MSG (memcmp (a, a + B, B * sizeof *a) == 0,
                    "each SEGMENT gets its own pass: identical declarations "
                    "render identically, because the key is (segment, "
                    "source) and there is no cross-segment source id");
  }

  /* ── a RANGED doppler draws per instance, and replays ─────────────────
   *
   * `doppler`/`doppler_rate` join freq/snr/level/f_end as [lo, hi] fields,
   * which is the main testing path the capability is for: sweeping a
   * receiver over a span of geometries without writing a scene per point.
   * Two properties, and the second is the one that makes a recorded sweep
   * reproducible:
   *
   *   - instances DIFFER (the draw actually varies), and
   *   - a second identical render REPLAYS them exactly (the draw hashes its
   *     key rather than consuming RNG state).
   */
  {
    enum
    {
      B  = 2048,
      R  = 3,
      NT = R * B
    };
    static float complex r1[NT], r2[NT];

    /* BOTH ranged fields: doppler_rate draws from its own stream, and
       ranging only the offset would leave that one unexercised. */
    wfm_source_t src
        = { .type            = WFM_SYNTH_TONE,
            .freq            = 1e5,
            .snr             = WFM_SYNTH_SNR_CLEAN,
            .seed            = 3,
            .carrier_hz      = 1.0e9,
            .doppler         = -20.0,
            .doppler_hi      = 20.0,
            .doppler_rate    = -50.0,
            .doppler_rate_hi = 50.0,
            .ranged          = WFM_RANGE_DOPPLER | WFM_RANGE_DOPPLER_RATE };
    wfm_segment_t seg = { .sources     = &src,
                          .n_sources   = 1,
                          .fs          = 1e6,
                          .num_samples = B,
                          .repeats     = R };

    float complex *outs[2] = { r1, r2 };
    for (int pass = 0; pass < 2; pass++)
      {
        wfm_compose_state_t *c = wfm_compose_create (&seg, 1, 0, 0);
        DP_REQUIRE_MSG (c, "a ranged-doppler scene composes");
        size_t nn = 0;
        for (size_t n;
             (n = wfm_compose_execute (c, outs[pass] + nn, NT - nn)) > 0;)
          nn += n;
        wfm_compose_destroy (c);
        DP_REQUIRE_MSG (nn == NT, "ranged-doppler scene yields every burst");
      }

    DP_REQUIRE_MSG (memcmp (r1, r2, NT * sizeof *r1) == 0,
                    "a ranged doppler REPLAYS: the draw hashes its key, so a "
                    "recorded sweep reproduces byte-for-byte");
    DP_REQUIRE_MSG (memcmp (r1, r1 + B, B * sizeof *r1) != 0,
                    "consecutive instances draw DIFFERENT doppler (else the "
                    "replay above is the replay of a constant)");
  }

  printf ("test_wfm_compose: OK (total=%zu, json round-trip, level, sum, "
          "resolve, sum-json, headroom, seed_advance, ranged fields, "
          "dsss burst, unspread frame, repeats, doppler block-invariance, "
          "persist-through-gap, ranged doppler)\n",
          total);
  return 0;
}
