/*
 * test_wfm_writer.c — raw/csv/BLUE writers + SigMF meta (Phase C).
 *
 * Uses tmpfile() (seekable) so the BLUE data_size patch-on-close is exercised.
 * Host is little-endian (x86), matching the writer's assumption.
 */
#include "dp_test.h"
#include "wfm_writer/wfm_writer_core.h"

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* read the whole tmpfile into buf, return byte length */
static size_t
slurp (FILE *fp, uint8_t *buf, size_t cap)
{
  fflush (fp);
  fseek (fp, 0, SEEK_SET);
  return fread (buf, 1, cap, fp);
}

/* close() must report a failed final flush, not discard it. fclose() is where
   buffered data actually reaches the disk, so its result IS the answer to "did
   the capture land"; ignoring it reported success for a file that never made
   it. Uses a read-only FILE so every write fails: the HCB write at open fails,
   the data writes fail, and the flush at close fails. */
static int
test_close_reports_a_failed_flush (void)
{
  const char *path = "dp_wr_ro.blue";
  FILE       *mk   = fopen (path, "wb"); /* create it, then reopen read-only */
  DP_REQUIRE_MSG (mk != NULL, "create");
  fclose (mk);

  FILE *ro = fopen (path, "rb"); /* writes on this stream cannot succeed */
  DP_REQUIRE_MSG (ro != NULL, "reopen read-only");
  wfm_writer_state_t *w
      = wfm_writer_open (ro, WFM_FT_RAW, 0, 0, 1e6, 0.0, 0, 0.0);
  if (w)
    {
      float _Complex x[16];
      for (size_t i = 0; i < 16; i++)
        x[i] = 0.5f + 0.25f * (float _Complex)I;
      wfm_writer_write (w, x, 16);
      /* The caller owns `ro`, so close() fflush()es rather than fclose()ing --
         either way a stream that cannot be written must not report success. */
      DP_REQUIRE_MSG (wfm_writer_close (w) != 0,
                      "close reports the failed flush");
    }
  fclose (ro);
  return 0;
}

/* Read a whole file into @p out (NUL-terminated). Returns 0 if it opened. */
static int
slurp_path (const char *path, char *out, size_t cap)
{
  FILE *f = fopen (path, "rb");
  if (!f)
    return -1;
  size_t n = fread (out, 1, cap - 1, f);
  out[n]   = 0;
  fclose (f);
  return 0;
}

static int
file_exists (const char *path)
{
  FILE *f = fopen (path, "rb");
  if (f)
    fclose (f);
  return f != NULL;
}

/* Clear every path a block will assert about, BEFORE it writes any of them.
   Half these checks are "this file must NOT exist", and a CHECK that fails
   leaves its block's remove() calls unreached -- so without this, one genuine
   failure poisons every later run in the same directory into failing for a
   reason that has nothing to do with the code. (Found by mutation-testing:
   a deliberately broken name derivation failed, and the NEXT mutation then
   reported that same stale failure instead of its own.) */
static void
clear (const char *const *paths, size_t n)
{
  for (size_t i = 0; i < n; i++)
    remove (paths[i]);
}

/* Raw and CSV take fs/fc/t0 and have nowhere to put them, so they used to
   discard the caller's own metadata and hand back a file nobody could
   interpret. A path-opened writer now keeps them in a sidecar. */
static int
test_raw_csv_sidecar (void)
{
  float _Complex xs[2] = { 0.25f + 0.5f * I, -0.25f - 0.5f * I };

  /* raw: everything stated, everything recorded. */
  {
    static const char *const mine[]
        = { "dp_wr_side.raw", "dp_wr_side.raw.sigmf-meta",
            "dp_wr_side.sigmf-meta" };
    clear (mine, 3);
    const char         *path = "dp_wr_side.raw";
    wfm_writer_state_t *w    = wfm_writer_create (
        path, 2.4e6, WFM_FT_RAW, 0, 0, 1.2e9, 2, 0.0, 1785903330.0, true);
    DP_REQUIRE_MSG (w, "raw create");
    DP_REQUIRE_MSG (wfm_writer_write (w, xs, 2) == 2, "raw write");
    DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "raw close");

    /* APPENDED, not swapped -- see wfm_meta_path. */
    char json[4096];
    DP_REQUIRE_MSG (slurp_path ("dp_wr_side.raw.sigmf-meta", json, sizeof json)
                        == 0,
                    "the sidecar is <path>.sigmf-meta, beside the capture");
    DP_REQUIRE_MSG (!file_exists ("dp_wr_side.sigmf-meta"),
                    "and NOT <base>.sigmf-meta, which a real pair would own");
    DP_REQUIRE_MSG (strstr (json, "\"core:sample_rate\":2400000"),
                    "fs recorded");
    DP_REQUIRE_MSG (strstr (json, "\"core:frequency\":1200000000"),
                    "fc recorded");
    DP_REQUIRE_MSG (
        strstr (json, "\"core:datetime\":\"2026-08-05T04:15:30.000000Z\""),
        "t0 recorded");
    DP_REQUIRE_MSG (strstr (json, "\"core:datatype\":\"cf32_le\""),
                    "datatype recorded");
    remove (path);
    remove ("dp_wr_side.raw.sigmf-meta");
  }

  /* csv: same sidecar. The datatype names the value domain the samples were
     quantised to, not a byte layout -- which is one reason this is
     SigMF-shaped rather than a SigMF capture. */
  {
    static const char *const mine[]
        = { "dp_wr_side.csv", "dp_wr_side.csv.sigmf-meta" };
    clear (mine, 2);
    const char         *path = "dp_wr_side.csv";
    wfm_writer_state_t *w    = wfm_writer_create (path, 1e6, WFM_FT_CSV, 3, 0,
                                                  0.0, 2, 0.0, 0.0, true);
    DP_REQUIRE_MSG (w, "csv create");
    DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "csv close");
    char json[4096];
    DP_REQUIRE_MSG (slurp_path ("dp_wr_side.csv.sigmf-meta", json, sizeof json)
                        == 0,
                    "csv gets a sidecar too");
    DP_REQUIRE_MSG (strstr (json, "\"core:sample_rate\":1000000"),
                    "csv fs recorded");
    /* Nothing was said about the centre frequency or the capture time, so
       nothing is claimed about them -- the omit rule reaches the sidecar. */
    DP_REQUIRE_MSG (strstr (json, "\"core:frequency\"") == NULL,
                    "an unstated fc is omitted, not written as DC");
    DP_REQUIRE_MSG (strstr (json, "\"core:datetime\"") == NULL,
                    "an unstated t0 is omitted, not written as 1970");
    remove (path);
    remove ("dp_wr_side.csv.sigmf-meta");
  }

  /* opt-out, and the two file types that never take part. */
  {
    static const char *const mine[]
        = { "dp_wr_off.raw", "dp_wr_off.raw.sigmf-meta", "dp_wr_side.blue",
            "dp_wr_side.blue.sigmf-meta" };
    clear (mine, 4);
    wfm_writer_state_t *w = wfm_writer_create (
        "dp_wr_off.raw", 1e6, WFM_FT_RAW, 0, 0, 1e9, 2, 0.0, 0.0, false);
    DP_REQUIRE_MSG (w && wfm_writer_close (w) == 0, "raw, sidecar off");
    DP_REQUIRE_MSG (!file_exists ("dp_wr_off.raw.sigmf-meta"),
                    "sidecar=false writes no sidecar");
    remove ("dp_wr_off.raw");

    w = wfm_writer_create ("dp_wr_side.blue", 1e6, WFM_FT_BLUE, 0, 0, 1e9, 2,
                           0.0, 0.0, true);
    DP_REQUIRE_MSG (w && wfm_writer_close (w) == 0, "blue create/close");
    DP_REQUIRE_MSG (
        !file_exists ("dp_wr_side.blue.sigmf-meta"),
        "BLUE never gets one -- its header already carries fs/fc/t0");
    remove ("dp_wr_side.blue");
  }

  /* The reason the name is appended. A raw capture and a real SigMF capture
     sharing a base name must not share a sidecar: swapping the extension
     would have `cap.raw` overwrite `cap.sigmf-data`'s metadata, silently
     retyping someone else's capture. */
  {
    static const char *const mine[]
        = { "dp_wr_clash.sigmf-data", "dp_wr_clash.sigmf-meta",
            "dp_wr_clash.raw", "dp_wr_clash.raw.sigmf-meta" };
    clear (mine, 4);
    wfm_writer_state_t *s
        = wfm_writer_create ("dp_wr_clash.sigmf-data", 5e6, WFM_FT_SIGMF, 3, 0,
                             0.0, 2, 0.0, 0.0, true);
    DP_REQUIRE_MSG (s && wfm_writer_close (s) == 0, "sigmf half of the clash");
    wfm_writer_state_t *r = wfm_writer_create (
        "dp_wr_clash.raw", 7e6, WFM_FT_RAW, 0, 0, 0.0, 2, 0.0, 0.0, true);
    DP_REQUIRE_MSG (r && wfm_writer_close (r) == 0, "raw half of the clash");

    char sj[4096], rj[4096];
    DP_REQUIRE_MSG (slurp_path ("dp_wr_clash.sigmf-meta", sj, sizeof sj) == 0,
                    "the SigMF pair keeps its own <base>.sigmf-meta");
    DP_REQUIRE_MSG (slurp_path ("dp_wr_clash.raw.sigmf-meta", rj, sizeof rj)
                        == 0,
                    "the raw capture gets a separate one");
    DP_REQUIRE_MSG (strstr (sj, "\"core:sample_rate\":5000000")
                        && strstr (sj, "\"core:datatype\":\"ci16_le\""),
                    "the SigMF capture's metadata survived intact");
    DP_REQUIRE_MSG (strstr (rj, "\"core:sample_rate\":7000000"),
                    "and the raw capture's is its own");
    remove ("dp_wr_clash.sigmf-data");
    remove ("dp_wr_clash.sigmf-meta");
    remove ("dp_wr_clash.raw");
    remove ("dp_wr_clash.raw.sigmf-meta");
  }
  return 0;
}

int
main (void)
{
  uint8_t bytes[1024];

  /* ── raw cf32 LE: interleaved float I/Q, host order ── */
  {
    float _Complex s[2]    = { 1.0f + 2.0f * I, -1.0f - 2.0f * I };
    FILE               *fp = tmpfile ();
    wfm_writer_state_t *w
        = wfm_writer_open (fp, WFM_FT_RAW, 0, 0, 1e6, 0, 2, 0.0);
    DP_REQUIRE_MSG (w, "raw open");
    DP_REQUIRE_MSG (wfm_writer_write (w, s, 2) == 2, "raw write");
    DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "raw close");
    size_t nb = slurp (fp, bytes, sizeof bytes);
    DP_REQUIRE_MSG (nb == 16, "raw cf32 byte count");
    float f[4];
    memcpy (f, bytes, 16);
    DP_REQUIRE_MSG (f[0] == 1.0f && f[1] == 2.0f && f[2] == -1.0f
                        && f[3] == -2.0f,
                    "raw cf32 interleaved values");
    fclose (fp);
  }

  /* ── endian: ci16 BE is the byte-reverse of ci16 LE ── */
  {
    float _Complex s[1] = { 0.5f - 0.5f * I };
    uint8_t             le[4], be[4];
    FILE               *fl = tmpfile (), *fb = tmpfile ();
    wfm_writer_state_t *wl
        = wfm_writer_open (fl, WFM_FT_RAW, 3, 0, 1e6, 0, 1, 0.0);
    wfm_writer_state_t *wb
        = wfm_writer_open (fb, WFM_FT_RAW, 3, 1, 1e6, 0, 1, 0.0);
    wfm_writer_write (wl, s, 1);
    wfm_writer_write (wb, s, 1);
    wfm_writer_close (wl);
    wfm_writer_close (wb);
    DP_REQUIRE_MSG (slurp (fl, le, 4) == 4 && slurp (fb, be, 4) == 4,
                    "ci16 sizes");
    /* two 2-byte elements, each reversed */
    DP_REQUIRE_MSG (be[0] == le[1] && be[1] == le[0], "ci16 BE I swapped");
    DP_REQUIRE_MSG (be[2] == le[3] && be[3] == le[2], "ci16 BE Q swapped");
    fclose (fl);
    fclose (fb);
  }

  /* ── csv cf32: one "%0.9f,%0.9f" line per sample ── */
  {
    float _Complex s[1]    = { 0.25f + (-0.5f) * I };
    FILE               *fp = tmpfile ();
    wfm_writer_state_t *w
        = wfm_writer_open (fp, WFM_FT_CSV, 0, 0, 1e6, 0, 1, 0.0);
    DP_REQUIRE_MSG (wfm_writer_write (w, s, 1) == 1, "csv write");
    wfm_writer_close (w);
    size_t nb = slurp (fp, bytes, sizeof bytes - 1);
    bytes[nb] = 0;
    DP_REQUIRE_MSG (strcmp ((char *)bytes, "0.250000000,-0.500000000\n") == 0,
                    "csv cf32 line");
    fclose (fp);
  }

  /* ── BLUE type-1000 header + data_size patch on close ── */
  {
    float _Complex s[2] = { 1.0f + 0.0f * I, 0.0f + 1.0f * I };
    FILE *fp            = tmpfile ();
    /* total unknown at open (0) → close must patch it */
    wfm_writer_state_t *w
        = wfm_writer_open (fp, WFM_FT_BLUE, 0, 0, 1e6, 0, 0, 0.0);
    DP_REQUIRE_MSG (w, "blue open");
    wfm_writer_write (w, s, 2);
    DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "blue close");
    size_t nb = slurp (fp, bytes, sizeof bytes);
    DP_REQUIRE_MSG (nb == 512 + 16, "blue header+data size");
    DP_REQUIRE_MSG (memcmp (bytes, "BLUE", 4) == 0, "blue magic");
    DP_REQUIRE_MSG (memcmp (bytes + 8, "EEEI", 4) == 0, "blue data_rep LE");
    int32_t type;
    memcpy (&type, bytes + 48, 4);
    DP_REQUIRE_MSG (type == 1000, "blue type 1000");
    DP_REQUIRE_MSG (bytes[52] == 'C' && bytes[53] == 'F', "blue format CF");
    double xdelta;
    memcpy (&xdelta, bytes + 264, 8);
    DP_REQUIRE_MSG (xdelta == 1e-6, "blue xdelta = 1/fs");
    double data_size;
    memcpy (&data_size, bytes + 40, 8);
    DP_REQUIRE_MSG (data_size == 16.0, "blue data_size patched (2*cf32=16)");
    int32_t det;
    memcpy (&det, bytes + 12, 4);
    DP_REQUIRE_MSG (det == 0, "blue attached: detached = 0");
    double dstart;
    memcpy (&dstart, bytes + 32, 8);
    DP_REQUIRE_MSG (dstart == 512.0, "blue attached: data_start = 512");
    fclose (fp);
  }

  /* ── BLUE detached HCB: 512-byte header only, detached=1, data_start=0 ── */
  {
    FILE *fp = tmpfile ();
    /* ci16, 100 samples, detached */
    DP_REQUIRE_MSG (wfm_blue_write_hcb (fp, 3, 0, 1e6, 0, 0.0, 100, 1, 0.0)
                        == 0,
                    "detached hcb write");
    size_t nb = slurp (fp, bytes, sizeof bytes);
    DP_REQUIRE_MSG (nb == 512, "detached hcb is header-only (no data)");
    DP_REQUIRE_MSG (memcmp (bytes, "BLUE", 4) == 0, "detached magic");
    int32_t det;
    memcpy (&det, bytes + 12, 4);
    DP_REQUIRE_MSG (det == 1, "detached flag set");
    double dstart, dsize;
    memcpy (&dstart, bytes + 32, 8);
    memcpy (&dsize, bytes + 40, 8);
    DP_REQUIRE_MSG (dstart == 0.0, "detached data_start = 0");
    DP_REQUIRE_MSG (dsize == 400.0, "detached data_size (100 * ci16 = 400)");
    DP_REQUIRE_MSG (bytes[52] == 'C' && bytes[53] == 'I',
                    "detached format CI");
    fclose (fp);
  }

  /* ── SigMF meta: datatype + one annotation per segment ── */
  {
    wfm_source_t  s0      = { .type      = 0,
                              .freq      = 1e5,
                              .snr       = 100.0,
                              .snr_mode  = 1,
                              .seed      = 1,
                              .sps       = 8,
                              .pn_length = 7 };
    wfm_source_t  s1      = { .type      = 4,
                              .freq      = 0,
                              .snr       = 9.0,
                              .snr_mode  = 3,
                              .seed      = 1,
                              .sps       = 8,
                              .pn_length = 7 };
    wfm_segment_t segs[2] = {
      { .sources     = &s0,
        .n_sources   = 1,
        .fs          = 1e6,
        .num_samples = 1000,
        .off_samples = 500 },
      { .sources     = &s1,
        .n_sources   = 1,
        .fs          = 1e6,
        .num_samples = 4096,
        .off_samples = 0 },
    };
    char *j = wfm_sigmf_meta_json (3, 0, 1e6, 2.4e9, 0.0, segs, 2);
    DP_REQUIRE_MSG (j, "sigmf meta");
    DP_REQUIRE_MSG (strstr (j, "\"core:datatype\":\"ci16_le\""),
                    "sigmf datatype");
    DP_REQUIRE_MSG (strstr (j, "\"core:sample_rate\":1000000"), "sigmf rate");
    DP_REQUIRE_MSG (strstr (j, "\"core:label\":\"tone\"")
                        && strstr (j, "\"core:label\":\"qpsk\""),
                    "sigmf per-segment labels");
    DP_REQUIRE_MSG (strstr (j, "\"core:sample_start\":1500"),
                    "sigmf 2nd seg start");
    DP_REQUIRE_MSG (strstr (j, "\"wfmgen:snr\":9"), "sigmf custom snr");
    free (j);
  }

  /* ── SigMF meta: continuous DSSS carries wfmgen:symbol_rate (the "dsss"
     core:label alone can't distinguish burst from continuous). Code-only adds
     wfmgen:data=none; a burst dsss omits both. ── */
  {
    uint8_t      code[7] = { 1, 1, 1, 0, 0, 1, 0 };
    wfm_source_t prbs    = { .type        = WFM_SYNTH_DSSS,
                             .sps         = 2,
                             .data_code   = code,
                             .n_data_code = 7,
                             .symbol_rate = 2700.0 };
    wfm_source_t none    = { .type           = WFM_SYNTH_DSSS,
                             .sps            = 2,
                             .data_code      = code,
                             .n_data_code    = 7,
                             .symbol_rate    = 2700.0,
                             .dsss_code_only = 1 };
    wfm_source_t burst   = {
      .type = WFM_SYNTH_DSSS, .sps = 2, .data_code = code, .n_data_code = 7
    }; /* symbol_rate 0 → burst */

    wfm_segment_t sp = {
      .sources = &prbs, .n_sources = 1, .fs = 6.138e6, .num_samples = 4096
    };
    char *jp = wfm_sigmf_meta_json (0, 0, 6.138e6, 0, 0.0, &sp, 1);
    DP_REQUIRE_MSG (jp && strstr (jp, "\"wfmgen:symbol_rate\":2700"),
                    "sigmf continuous prbs symbol_rate");
    DP_REQUIRE_MSG (jp && !strstr (jp, "\"wfmgen:data\""),
                    "sigmf continuous prbs omits data key");
    free (jp);

    wfm_segment_t sn = {
      .sources = &none, .n_sources = 1, .fs = 6.138e6, .num_samples = 4096
    };
    char *jn = wfm_sigmf_meta_json (0, 0, 6.138e6, 0, 0.0, &sn, 1);
    DP_REQUIRE_MSG (jn && strstr (jn, "\"wfmgen:symbol_rate\":2700")
                        && strstr (jn, "\"wfmgen:data\":\"none\""),
                    "sigmf code-only symbol_rate + data=none");
    free (jn);

    wfm_segment_t sb = {
      .sources = &burst, .n_sources = 1, .fs = 6.138e6, .num_samples = 4096
    };
    char *jb = wfm_sigmf_meta_json (0, 0, 6.138e6, 0, 0.0, &sb, 1);
    DP_REQUIRE_MSG (jb && !strstr (jb, "\"wfmgen:symbol_rate\""),
                    "sigmf burst dsss omits symbol_rate");
    free (jb);
  }

  /* ── clip detection: peak (always) + opt-in fraction ── */
  {
    /* s0: |re|=1.5 clips, |im|=0.5 ok; s1: |re|=0.5 ok, |im|=2.0 clips.
       peak = 2.0; 2 of 4 components saturate → fraction 0.5 (ci16). */
    float _Complex s[2]    = { 1.5f + 0.5f * I, -0.5f - 2.0f * I };
    FILE               *fp = tmpfile ();
    wfm_writer_state_t *w
        = wfm_writer_open (fp, WFM_FT_RAW, 3, 0, 1e6, 0, 2, 0.0);
    DP_REQUIRE_MSG (w, "clip open");
    wfm_writer_track_clipping (w, 1);
    DP_REQUIRE_MSG (wfm_writer_write (w, s, 2) == 2, "clip write");
    DP_REQUIRE_MSG (wfm_writer_peak (w) == 2.0, "clip peak == 2.0");
    double f = wfm_writer_clip_fraction (w);
    DP_REQUIRE_MSG (f > 0.49 && f < 0.51, "clip fraction == 0.5");
    DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "clip close");
    fclose (fp);
  }

  /* ── float never clips: peak tracked, fraction stays 0 ── */
  {
    float _Complex s[1]    = { 3.0f + 0.0f * I };
    FILE               *fp = tmpfile ();
    wfm_writer_state_t *w
        = wfm_writer_open (fp, WFM_FT_RAW, 0, 0, 1e6, 0, 1, 0.0);
    wfm_writer_track_clipping (w, 1);
    wfm_writer_write (w, s, 1);
    DP_REQUIRE_MSG (wfm_writer_peak (w) == 3.0, "float peak tracked");
    DP_REQUIRE_MSG (wfm_writer_clip_fraction (w) == 0.0, "float never clips");
    wfm_writer_close (w);
    fclose (fp);
  }

  /* ── clean at full-scale: peak == 1.0, no clip; fraction 0 without opt-in ──
   */
  {
    float _Complex s[2]    = { 1.0f + 1.0f * I, -1.0f - 1.0f * I };
    FILE               *fp = tmpfile ();
    wfm_writer_state_t *w
        = wfm_writer_open (fp, WFM_FT_RAW, 3, 0, 1e6, 0, 2, 0.0);
    wfm_writer_write (w, s, 2); /* no track_clipping → fraction stays 0 */
    DP_REQUIRE_MSG (wfm_writer_peak (w) == 1.0, "clean peak == 1.0 (no clip)");
    DP_REQUIRE_MSG (wfm_writer_clip_fraction (w) == 0.0,
                    "no opt-in → fraction 0");
    wfm_writer_close (w);
    fclose (fp);
  }

  /* ── headroom: gain 1.0 is a bit-exact no-op (byte-identical) ── */
  {
    float _Complex s[1] = { 0.8f - 0.3f * I };
    uint8_t             a[4], b[4];
    FILE               *fa = tmpfile (), *fb = tmpfile ();
    wfm_writer_state_t *wa
        = wfm_writer_open (fa, WFM_FT_RAW, 3, 0, 1e6, 0, 1, 0.0);
    wfm_writer_state_t *wb
        = wfm_writer_open (fb, WFM_FT_RAW, 3, 0, 1e6, 0, 1, 0.0);
    wfm_writer_set_gain (wa, 1.0); /* explicit 1.0 == default (no gain) */
    wfm_writer_write (wa, s, 1);
    wfm_writer_write (wb, s, 1);
    wfm_writer_close (wa);
    wfm_writer_close (wb);
    DP_REQUIRE_MSG (slurp (fa, a, 4) == 4 && slurp (fb, b, 4) == 4,
                    "headroom sizes");
    DP_REQUIRE_MSG (memcmp (a, b, 4) == 0, "gain 1.0 byte-identical");
    fclose (fa);
    fclose (fb);
  }

  /* ── headroom backs the signal off: gain 0.5 clears a clip ── */
  {
    float _Complex s[1]    = { 1.5f + 0.0f * I }; /* clips at unity gain */
    FILE               *fp = tmpfile ();
    wfm_writer_state_t *w
        = wfm_writer_open (fp, WFM_FT_RAW, 3, 0, 1e6, 0, 1, 0.0);
    wfm_writer_set_gain (w, 0.5); /* 1.5 * 0.5 = 0.75, fits full-scale */
    wfm_writer_track_clipping (w, 1);
    wfm_writer_write (w, s, 1);
    DP_REQUIRE_MSG (wfm_writer_peak (w) == 0.75,
                    "gain 0.5: peak 0.75 (no clip)");
    DP_REQUIRE_MSG (wfm_writer_clip_fraction (w) == 0.0,
                    "headroom cleared the clip");
    wfm_writer_close (w);
    fclose (fp);
  }

  /* ── SigMF: an UNSTATED rate is omitted, not fabricated. ───────────────
     core:sample_rate is optional in SigMF 1.0.0 (only core:datatype and
     core:version are required in `global`), so fs == 0.0 must produce a
     document with no rate key at all. The alternative -- writing whatever
     the ctor happened to default to -- is what put a confident 1000000 into
     captures nobody had given a rate, in a file that outlives the process.
     An absent key says "not stated"; a present one is a number a downstream
     tool will act on. ── */
  {
    char *j = wfm_sigmf_meta_json (3, 0, 0.0, 2.4e9, 0.0, NULL, 0);
    DP_REQUIRE_MSG (j, "sigmf meta with no rate");
    DP_REQUIRE_MSG (strstr (j, "\"core:sample_rate\"") == NULL,
                    "an unstated rate is OMITTED, never defaulted");
    /* The required members are still there -- omitting the rate must not
       produce a document that fails validation for a different reason. */
    DP_REQUIRE_MSG (strstr (j, "\"core:datatype\":\"ci16_le\""),
                    "datatype still set");
    DP_REQUIRE_MSG (strstr (j, "\"core:version\":\"1.0.0\""),
                    "version still set");
    free (j);
    /* The control: the SAME call WITH a rate must emit it, or the check
       above would also pass on a builder that had stopped writing it. */
    j = wfm_sigmf_meta_json (3, 0, 2.5e6, 2.4e9, 0.0, NULL, 0);
    DP_REQUIRE_MSG (j, "sigmf meta with a rate");
    DP_REQUIRE_MSG (strstr (j, "\"core:sample_rate\":2500000"),
                    "a stated rate IS emitted");
    free (j);
  }

  /* ── SigMF: core:datetime follows the same omit-when-unset rule, and is
     the EXTENDED ISO 8601 spelling the spec requires -- not the
     filename-safe basic form doppler names files with. ── */
  {
    char *j = wfm_sigmf_meta_json (3, 0, 1e6, 0.0, 0.0, NULL, 0);
    DP_REQUIRE_MSG (j, "sigmf meta, unset t0");
    DP_REQUIRE_MSG (strstr (j, "\"core:datetime\"") == NULL,
                    "an unset t0 is OMITTED, never rendered as 1970");
    free (j);
    /* 2026-08-05T04:15:30Z */
    j = wfm_sigmf_meta_json (3, 0, 1e6, 0.0, 1785903330.0, NULL, 0);
    DP_REQUIRE_MSG (j, "sigmf meta, set t0");
    DP_REQUIRE_MSG (
        strstr (j, "\"core:datetime\":\"2026-08-05T04:15:30.000000Z\""),
        "t0 rendered as extended ISO 8601, separators and all");
    free (j);
  }

  /* ── an unstated rate is DERIVED from the segments, which already carry
     one — the annotation edges in the same document are computed from it ── */
  {
    wfm_source_t  q      = { .type = 4, .sps = 8, .seed = 1 };
    wfm_segment_t two[2] = {
      { .sources = &q, .n_sources = 1, .fs = 6.138e6, .num_samples = 4096 },
      { .sources = &q, .n_sources = 1, .fs = 6.138e6, .num_samples = 4096 },
    };
    char *j = wfm_sigmf_meta_json (0, 0, 0.0, 0.0, 0.0, two, 2);
    DP_REQUIRE_MSG (j, "meta with segments and no stated rate");
    DP_REQUIRE_MSG (strstr (j, "\"core:sample_rate\":6138000"),
                    "the segments' agreed rate is stated, not withheld");
    free (j);

    /* An explicit rate always wins — rendering at a resampled rate describes
       the FILE, and the file is what the document annotates. */
    j = wfm_sigmf_meta_json (0, 0, 1e6, 0.0, 0.0, two, 2);
    DP_REQUIRE_MSG (j && strstr (j, "\"core:sample_rate\":1000000"),
                    "a stated rate overrides the segments'");
    free (j);

    /* Segments that disagree: no single core:sample_rate is true of the
       stream, so it stays unstated rather than picking one. */
    two[1].fs = 2e6;
    j         = wfm_sigmf_meta_json (0, 0, 0.0, 0.0, 0.0, two, 2);
    DP_REQUIRE_MSG (j && strstr (j, "\"core:sample_rate\"") == NULL,
                    "disagreeing segments leave the rate unstated");
    free (j);

    /* No segments, nothing to derive from -- the Writer sidecar's case. */
    j = wfm_sigmf_meta_json (0, 0, 0.0, 0.0, 0.0, NULL, 0);
    DP_REQUIRE_MSG (j && strstr (j, "\"core:sample_rate\"") == NULL,
                    "no segments leaves the rate unstated");
    free (j);
  }

  if (test_close_reports_a_failed_flush ())
    return 1;
  if (test_raw_csv_sidecar ())
    return 1;
  printf (
      "test_wfm_writer: OK (raw/endian/csv/blue + sigmf + sidecar + clip + "
      "headroom)\n");
  return 0;
}
