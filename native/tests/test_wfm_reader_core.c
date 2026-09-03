/*
 * test_wfm_reader.c — round-trip wfm_writer → wfm_reader across every
 * file type, plus file type auto-detection and the BLUE-magic gate.
 */
#include "dp_test.h"
#include "wfm/wfm_keywords.h"
#include "wfm_reader/wfm_reader_core.h"
#include "wfm_writer/wfm_writer_core.h"

#include <complex.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 1000
/* A deterministic unit-scale test signal. */
static void
make_signal (float _Complex *x, size_t n)
{
  for (size_t i = 0; i < n; i++)
    x[i] = (float)(0.9 * sin (0.1 * i)) + (float)(0.8 * cos (0.07 * i)) * I;
}

/* Write x through a writer of the given file type, then read it back and check
   the samples match within tol and the metadata is recovered. */
static int
roundtrip (const char *path, int ft, int stype, double fs, double tol)
{
  float _Complex x[N], y[N];
  make_signal (x, N);

  FILE *fp = fopen (path, "wb");
  DP_REQUIRE_MSG (fp, "open for write");
  wfm_writer_state_t *w = wfm_writer_open (fp, ft, stype, 0, fs, 0.0, N, 0.0);
  DP_REQUIRE_MSG (w, "writer open");
  DP_REQUIRE_MSG (wfm_writer_write (w, x, N) == N, "writer wrote N");
  wfm_writer_close (w);
  fclose (fp);

  /* SigMF needs its .sigmf-meta sidecar written separately. */
  if (ft == WFM_FT_SIGMF)
    {
      char *meta = wfm_sigmf_meta_json (stype, 0, fs, 0.0, 0.0, NULL, 0);
      DP_REQUIRE_MSG (meta, "sigmf meta json");
      char mpath[1024];
      snprintf (mpath, sizeof mpath, "%.*s.sigmf-meta",
                (int)(strlen (path) - 11), path); /* strip .sigmf-data */
      FILE *mf = fopen (mpath, "w");
      DP_REQUIRE_MSG (mf, "open meta");
      fputs (meta, mf);
      fclose (mf);
      free (meta);
    }

  wfm_reader_state_t *r = wfm_reader_create (path, stype, 0);
  DP_REQUIRE_MSG (r, "reader open");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  DP_REQUIRE_MSG (info.sample_type == stype, "sample_type recovered");
  if (ft == WFM_FT_BLUE || ft == WFM_FT_SIGMF)
    DP_REQUIRE_MSG (fabs (info.fs - fs) < 1.0, "fs recovered from metadata");
  /* Exact for every file type now: CSV used to be the one that could
     only say 0, which reads as an empty capture. */
  DP_REQUIRE_MSG (info.num_samples == N, "num_samples");

  size_t total = 0, n;
  while ((n = wfm_reader_read (r, N - total, y + total, N - total)) > 0)
    total += n;
  wfm_reader_destroy (r);
  DP_REQUIRE_MSG (total == N, "read back N samples");

  double maxerr = 0.0;
  for (size_t i = 0; i < N; i++)
    {
      double e = cabs ((double _Complex)x[i] - (double _Complex)y[i]);
      if (e > maxerr)
        maxerr = e;
    }
  DP_REQUIRE_MSG (maxerr < tol, "samples round-trip within tol");
  return 0;
}

/* A raw file must NOT be mis-detected as BLUE, and a .det without a valid
   .hdr (no BLUE magic) must fail to open. */
static int
test_blue_gate (void)
{
  /* raw cf32 file that happens to start with non-"BLUE" bytes → raw. */
  const char *raw = "dp_reader_raw.cf32";
  float _Complex x[8];
  make_signal (x, 8);
  FILE *fp = fopen (raw, "wb");
  DP_REQUIRE_MSG (fp, "open raw");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_RAW, 0, 0, 1e6, 0.0, 8, 0.0);
  wfm_writer_write (w, x, 8);
  wfm_writer_close (w);
  fclose (fp);
  wfm_reader_state_t *r = wfm_reader_create (raw, 0, 0);
  DP_REQUIRE_MSG (r, "raw opens");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  DP_REQUIRE_MSG (info.file_type == WFM_FT_RAW,
                  "raw not mis-detected as BLUE");
  wfm_reader_destroy (r);

  /* a .det whose .hdr lacks the BLUE magic must be rejected. */
  FILE *hf = fopen ("dp_reader_bad.hdr", "wb");
  DP_REQUIRE_MSG (hf, "open bad hdr");
  char junk[512] = "NOTBLUE";
  fwrite (junk, 1, 512, hf);
  fclose (hf);
  FILE *df = fopen ("dp_reader_bad.det", "wb");
  fwrite (x, sizeof x, 1, df);
  fclose (df);
  DP_REQUIRE_MSG (wfm_reader_create ("dp_reader_bad.det", 0, 0) == NULL,
                  "detached without BLUE magic is rejected");
  return 0;
}

/* A DETACHED BLUE capture opened by its HEADER must return the payload.
   Regression: the reader used to infer "detached" from the .det extension and
   never read the HCB `detached` field (offset 12), so a header file parsed its
   HCB then seeked to data_start -- 0 when detached -- and handed back the
   512-byte HCB itself as IQ (64 cf32 "samples", the first being the ASCII
   "BLUEEEEI" magic as two floats). Silent: no error, right file_type and fs.
   Per BLUE 3.1.1.4 the header is <base>.tmp / <base>.prm (doppler writes
   <base>.hdr) and the payload is the collocated <base>.det, so the extension
   must not decide -- `detached` does. Checks every header spelling. */
static int
test_detached_header_entry (void)
{
  static const char *const HDR[]
      = { "dp_det.hdr", "dp_det.prm", "dp_det.tmp" };
  float _Complex x[N], y[N];
  make_signal (x, N);

  /* payload: raw cf32 from byte 0 of the .det */
  FILE *df = fopen ("dp_det.det", "wb");
  DP_REQUIRE_MSG (df != NULL, "open .det");
  DP_REQUIRE_MSG (fwrite (x, sizeof x[0], N, df) == N, "write .det");
  fclose (df);

  for (size_t i = 0; i < sizeof HDR / sizeof *HDR; i++)
    {
      FILE *hf = fopen (HDR[i], "wb");
      DP_REQUIRE_MSG (hf != NULL, "open detached header");
      /* data_start = 0, detached = 1 -> payload is the collocated .det */
      DP_REQUIRE_MSG (wfm_blue_write_hcb (hf, 0, 0, 2.4e6, 0.0, 0.0, N, 1, 0.0)
                          == 0,
                      "write detached HCB");
      fclose (hf);

      wfm_reader_state_t *r = wfm_reader_create (HDR[i], 0, 0);
      DP_REQUIRE_MSG (r != NULL, "open detached capture by its header");
      wfm_reader_info_t info;
      wfm_reader_info (r, &info);
      DP_REQUIRE_MSG (info.file_type == WFM_FT_BLUE,
                      "detached header detects BLUE");
      DP_REQUIRE_MSG (info.num_samples == N,
                      "detached num_samples from data_size");
      size_t got = wfm_reader_read (r, N, y, N);
      wfm_reader_destroy (r);
      /* the whole payload -- NOT the 512-byte header as 64 samples */
      DP_REQUIRE_MSG (got == N, "detached header yields the full payload");
      for (size_t k = 0; k < N; k++)
        DP_REQUIRE_MSG (cabsf (y[k] - x[k]) < 1e-6f,
                        "detached payload is exact");
    }
  return 0;
}

/* Write a BLUE type-1000 capture whose HCB `format` field carries an arbitrary
   [mode][type] pair, with @p ncomp components per sample written from x's real
   (and, when ncomp == 2, imaginary) parts. wfm_blue_write_hcb always emits
   'C', so byte 52 is patched after the fact -- that is exactly the file a
   foreign Midas producer would hand us. */
static int
write_blue_mode (const char *path, char mode, size_t ncomp,
                 const float _Complex *x, size_t n)
{
  FILE *fp = fopen (path, "wb");
  DP_REQUIRE_MSG (fp != NULL, "open mode file");
  DP_REQUIRE_MSG (wfm_blue_write_hcb (fp, 0, 0, 1e6, 0.0, 512.0, n, 0, 0.0)
                      == 0,
                  "write HCB");
  /* data_size assumed complex; rewrite it for the real component count. */
  double dsz = (double)(n * ncomp * 4);
  fseek (fp, 52, SEEK_SET);
  fputc (mode, fp);
  fseek (fp, 40, SEEK_SET);
  fwrite (&dsz, sizeof dsz, 1, fp);
  fseek (fp, 512, SEEK_SET);
  for (size_t i = 0; i < n; i++)
    {
      float re = crealf (x[i]), im = cimagf (x[i]);
      DP_REQUIRE_MSG (fwrite (&re, sizeof re, 1, fp) == 1, "write I");
      if (ncomp == 2)
        DP_REQUIRE_MSG (fwrite (&im, sizeof im, 1, fp) == 1, "write Q");
    }
  fclose (fp);
  return 0;
}

/* The BLUE `format` field is [mode][type] (bytes 52..53). Only byte 53 used to
   be read, so a SCALAR ('S') capture -- one component per sample -- was walked
   at the complex stride: every other real sample became a phantom Q, the
   signal came back at half length, and num_samples under-counted 2x. Neither
   error surfaced. Now the mode is parsed: 'S' reads one component with Q == 0,
   'C' reads the interleaved pair, and every other Midas mode (V/Q/M/T, 3..10
   components) is REJECTED at open rather than misread at the wrong stride. */
static int
test_blue_format_mode (void)
{
  float _Complex x[N], y[N];
  make_signal (x, N);

  /* scalar: one component per sample, Q == 0 */
  if (write_blue_mode ("dp_mode_s.blue", 'S', 1, x, N))
    return 1;
  wfm_reader_state_t *r = wfm_reader_create ("dp_mode_s.blue", 0, 0);
  DP_REQUIRE_MSG (r != NULL, "scalar BLUE opens");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  DP_REQUIRE_MSG (info.mode == WFM_MODE_SCALAR, "mode is scalar");
  DP_REQUIRE_MSG (info.num_samples == N, "scalar num_samples is not halved");
  size_t got = wfm_reader_read (r, N, y, N);
  wfm_reader_destroy (r);
  DP_REQUIRE_MSG (got == N, "scalar yields every sample, not half");
  for (size_t k = 0; k < N; k++)
    {
      DP_REQUIRE_MSG (fabsf (crealf (y[k]) - crealf (x[k])) < 1e-6f,
                      "scalar I exact");
      DP_REQUIRE_MSG (cimagf (y[k]) == 0.0f, "scalar Q is exactly zero");
    }

  /* complex: unchanged, and reports its mode */
  if (write_blue_mode ("dp_mode_c.blue", 'C', 2, x, N))
    return 1;
  r = wfm_reader_create ("dp_mode_c.blue", 0, 0);
  DP_REQUIRE_MSG (r != NULL, "complex BLUE opens");
  wfm_reader_info (r, &info);
  DP_REQUIRE_MSG (info.mode == WFM_MODE_COMPLEX, "mode is complex");
  DP_REQUIRE_MSG (info.num_samples == N, "complex num_samples");
  got = wfm_reader_read (r, N, y, N);
  wfm_reader_destroy (r);
  DP_REQUIRE_MSG (got == N, "complex yields every sample");
  for (size_t k = 0; k < N; k++)
    DP_REQUIRE_MSG (cabsf (y[k] - x[k]) < 1e-6f, "complex round-trips");

  /* every unsupported mode designator is refused, not guessed at */
  static const char BAD[] = { 'V', 'Q', 'M', 'T', 'X', '1', 'c', 's' };
  for (size_t i = 0; i < sizeof BAD; i++)
    {
      if (write_blue_mode ("dp_mode_bad.blue", BAD[i], 2, x, 8))
        return 1;
      DP_REQUIRE_MSG (wfm_reader_create ("dp_mode_bad.blue", 0, 0) == NULL,
                      "unsupported format mode is rejected");
    }
  return 0;
}

/* Attach one keyword of every KW-legal type to @p w. Kept in one place so the
   writer test and the reader test cannot drift apart about what was written.
 */
static const char *const KW_STR  = "10 dB pad, 2026-07-21";
static const double      KW_D    = 1.2345e9;
static const float       KW_F[3] = { 1.5f, -2.5f, 3.5f };
static const int32_t     KW_L    = -70000;
static const int16_t     KW_I    = -1234;
static const int8_t      KW_B    = -7;
static const int64_t     KW_X    = 1234567890123LL;

static int
attach_keywords (wfm_writer_state_t *w)
{
  DP_REQUIRE_MSG (
      wfm_writer_add_keyword (w, "COMMENT", 'A', KW_STR, strlen (KW_STR)) == 0,
      "add A");
  DP_REQUIRE_MSG (wfm_writer_add_keyword (w, "F_C", 'D', &KW_D, 1) == 0,
                  "add D");
  DP_REQUIRE_MSG (wfm_writer_add_keyword (w, "GAINS", 'F', KW_F, 3) == 0,
                  "add F[]");
  DP_REQUIRE_MSG (wfm_writer_add_keyword (w, "OFFSET", 'L', &KW_L, 1) == 0,
                  "add L");
  DP_REQUIRE_MSG (wfm_writer_add_keyword (w, "TRIM", 'I', &KW_I, 1) == 0,
                  "add I");
  DP_REQUIRE_MSG (wfm_writer_add_keyword (w, "FLAG", 'B', &KW_B, 1) == 0,
                  "add B");
  DP_REQUIRE_MSG (wfm_writer_add_keyword (w, "TICKS", 'X', &KW_X, 1) == 0,
                  "add X");
  return 0;
}

/* Check the keywords attach_keywords() wrote all came back intact. */
static int
check_keywords (wfm_reader_state_t *r)
{
  DP_REQUIRE_MSG (wfm_reader_num_keywords (r) == 7,
                  "all seven keywords recovered");
  const wfm_keyword_t *k = wfm_reader_find_keyword (r, "COMMENT");
  DP_REQUIRE_MSG (k && k->type == 'A', "COMMENT is a string");
  DP_REQUIRE_MSG (k->count == strlen (KW_STR),
                  "string length (no NUL on the wire)");
  DP_REQUIRE_MSG (memcmp (k->value, KW_STR, k->count) == 0, "string value");

  k = wfm_reader_find_keyword (r, "F_C");
  double d;
  DP_REQUIRE_MSG (k && k->type == 'D' && k->count == 1,
                  "F_C is a scalar double");
  memcpy (&d, k->value, 8);
  DP_REQUIRE_MSG (d == KW_D, "double value");

  k = wfm_reader_find_keyword (r, "GAINS");
  float f[3];
  DP_REQUIRE_MSG (k && k->type == 'F' && k->count == 3,
                  "GAINS is a 3-element float");
  memcpy (f, k->value, sizeof f);
  DP_REQUIRE_MSG (f[0] == KW_F[0] && f[1] == KW_F[1] && f[2] == KW_F[2],
                  "array order preserved");

  k = wfm_reader_find_keyword (r, "OFFSET");
  int32_t l;
  DP_REQUIRE_MSG (k && k->count == 1, "OFFSET present");
  memcpy (&l, k->value, 4);
  DP_REQUIRE_MSG (l == KW_L, "negative int32 value");

  k = wfm_reader_find_keyword (r, "TICKS");
  int64_t x;
  DP_REQUIRE_MSG (k && k->count == 1, "TICKS present");
  memcpy (&x, k->value, 8);
  DP_REQUIRE_MSG (x == KW_X, "int64 value");

  DP_REQUIRE_MSG (wfm_reader_find_keyword (r, "NOPE") == NULL,
                  "absent tag is NULL");
  /* file order is preserved, so index 0 is the first one written */
  DP_REQUIRE_MSG (strcmp (wfm_reader_keyword (r, 0)->tag, "COMMENT") == 0,
                  "keywords come back in file order");
  DP_REQUIRE_MSG (wfm_reader_keyword (r, 7) == NULL,
                  "out-of-range index is NULL");
  return 0;
}

/* Extended-header keywords survive a full write -> read cycle, attached and
   detached, little- and big-endian, without disturbing the samples. The
   detached case is the one that can silently regress: the extended header
   lives in the HEADER file while the samples come from the .det, so a reader
   that looks for keywords in the data file finds none and reports an empty
   capture rather than an error. */
static int
test_keyword_roundtrip (void)
{
  float _Complex x[N], y[N];
  make_signal (x, N);

  for (int be = 0; be <= 1; be++)
    {
      const char *path = be ? "dp_kw_be.blue" : "dp_kw_le.blue";
      FILE       *fp   = fopen (path, "wb");
      DP_REQUIRE_MSG (fp != NULL, "open blue");
      wfm_writer_state_t *w
          = wfm_writer_open (fp, WFM_FT_BLUE, 0, be, 2.4e6, 0.0, N, 0.0);
      DP_REQUIRE_MSG (w != NULL, "writer open");
      if (attach_keywords (w))
        return 1;
      DP_REQUIRE_MSG (wfm_writer_write (w, x, N) == N, "write samples");
      DP_REQUIRE_MSG (wfm_writer_close (w) == 0,
                      "close writes the extended header");
      fclose (fp);

      wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
      DP_REQUIRE_MSG (r != NULL, "reopen");
      if (check_keywords (r))
        return 1;
      /* Draining to EOF must stop at the declared payload. The extended
         header sits AFTER the data, so a reader that just reads until fread
         runs dry would hand the caller keyword bytes as IQ -- silently, and
         only for files that carry metadata. */
      size_t total = 0, got;
      while ((got = wfm_reader_read (r, N - total, y + total, N - total)) > 0)
        total += got;
      DP_REQUIRE_MSG (total == N, "drains to exactly the declared payload");
      DP_REQUIRE_MSG (wfm_reader_read (r, N, y, N) == 0,
                      "and stays at end of data");
      for (size_t i = 0; i < N; i++)
        DP_REQUIRE_MSG (cabsf (y[i] - x[i]) < 1e-6f, "samples unaffected");
      wfm_reader_destroy (r);
    }

  /* detached: keywords are in the .hdr, samples in the .det */
  FILE *hf = fopen ("dp_kw_det.hdr", "wb");
  DP_REQUIRE_MSG (hf != NULL, "open detached header");
  DP_REQUIRE_MSG (wfm_blue_write_hcb (hf, 0, 0, 2.4e6, 0.0, 0.0, N, 1, 0.0)
                      == 0,
                  "write detached HCB");
  fclose (hf);
  /* re-open the header as a BLUE writer target purely to attach keywords:
     the payload goes to the .det, so this writer emits no samples. */
  hf = fopen ("dp_kw_det.hdr", "r+b");
  DP_REQUIRE_MSG (hf != NULL, "reopen header");
  fseek (hf, 0, SEEK_END);
  {
    uint8_t kwblob[512];
    size_t  off = 0;
    off += wfm_kw_encode (kwblob + off, sizeof kwblob - off, "COMMENT", 'A',
                          KW_STR, strlen (KW_STR), 0);
    off += wfm_kw_encode (kwblob + off, sizeof kwblob - off, "F_C", 'D', &KW_D,
                          1, 0);
    /* the extended header must start on a 512-byte boundary; the HCB is
       exactly 512 bytes, so block 1 is where it lands. */
    DP_REQUIRE_MSG (fwrite (kwblob, 1, off, hf) == off,
                    "append extended header");
    int32_t  blocks = 1, size = (int32_t)off;
    uint8_t  b[8];
    uint8_t *p = b;
    memcpy (p, &blocks, 4);
    memcpy (p + 4, &size, 4);
    fseek (hf, 24, SEEK_SET);
    DP_REQUIRE_MSG (fwrite (b, 1, 8, hf) == 8, "patch ext_start/ext_size");
    fclose (hf);
  }
  FILE *df = fopen ("dp_kw_det.det", "wb");
  DP_REQUIRE_MSG (df != NULL, "open .det");
  DP_REQUIRE_MSG (fwrite (x, sizeof x[0], N, df) == N, "write payload");
  fclose (df);

  /* both entry points must find the keywords -- they live in the header file
     either way, which is the whole point of the detached split. */
  static const char *const ENTRY[] = { "dp_kw_det.hdr", "dp_kw_det.det" };
  for (size_t i = 0; i < 2; i++)
    {
      wfm_reader_state_t *r = wfm_reader_create (ENTRY[i], 0, 0);
      DP_REQUIRE_MSG (r != NULL, "open detached capture");
      DP_REQUIRE_MSG (wfm_reader_num_keywords (r) == 2,
                      "detached keywords come from the HEADER file");
      const wfm_keyword_t *k = wfm_reader_find_keyword (r, "F_C");
      double               d;
      DP_REQUIRE_MSG (k != NULL, "F_C present");
      memcpy (&d, k->value, 8);
      DP_REQUIRE_MSG (d == KW_D, "detached keyword value");
      DP_REQUIRE_MSG (wfm_reader_read (r, N, y, N) == N,
                      "detached samples still read");
      wfm_reader_destroy (r);
    }
  return 0;
}

/* A capture with no extended header reports none -- and a BLUE file whose
   keyword region is truncated or corrupt still yields its samples. Metadata
   must never cost you the data. */
static int
test_keyword_absent_and_corrupt (void)
{
  float _Complex x[N], y[N];
  make_signal (x, N);

  FILE *fp = fopen ("dp_kw_none.blue", "wb");
  DP_REQUIRE_MSG (fp != NULL, "open");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_BLUE, 0, 0, 2.4e6, 0.0, N, 0.0);
  wfm_writer_write (w, x, N);
  wfm_writer_close (w);
  fclose (fp);
  wfm_reader_state_t *r = wfm_reader_create ("dp_kw_none.blue", 0, 0);
  DP_REQUIRE_MSG (r != NULL, "opens");
  DP_REQUIRE_MSG (wfm_reader_num_keywords (r) == 0,
                  "no extended header, no keywords");
  DP_REQUIRE_MSG (wfm_reader_keyword (r, 0) == NULL, "index 0 is NULL");
  wfm_reader_destroy (r);

  /* claim an extended header that runs off the end of the file */
  fp = fopen ("dp_kw_bad.blue", "wb");
  w  = wfm_writer_open (fp, WFM_FT_BLUE, 0, 0, 2.4e6, 0.0, N, 0.0);
  attach_keywords (w);
  wfm_writer_write (w, x, N);
  wfm_writer_close (w);
  fclose (fp);
  fp = fopen ("dp_kw_bad.blue", "r+b");
  DP_REQUIRE_MSG (fp != NULL, "reopen to corrupt");
  int32_t huge = 1 << 20; /* ext_size far past EOF */
  fseek (fp, 28, SEEK_SET);
  fwrite (&huge, 4, 1, fp);
  fclose (fp);
  r = fopen ("dp_kw_bad.blue", "rb")
          ? wfm_reader_create ("dp_kw_bad.blue", 0, 0)
          : NULL;
  DP_REQUIRE_MSG (r != NULL, "a bad keyword region does not fail the open");
  DP_REQUIRE_MSG (wfm_reader_read (r, N, y, N) == N,
                  "samples survive a bad ext header");
  wfm_reader_destroy (r);
  return 0;
}

/* reset() rewinds to the first SAMPLE, not to byte 0 of the file. Getting that
   wrong on an attached BLUE capture would replay the 512-byte HCB as IQ -- the
   same failure the detached-header bug produced -- so this checks the second
   pass is bit-identical to the first across every file type, including the
   detached split (where the payload genuinely does start at byte 0 of another
   file) and a capture carrying an extended header (where the data does not run
   to EOF). */
static int
test_reset_rewinds_to_the_first_sample (void)
{
  float _Complex x[N], a[N], b[N];
  make_signal (x, N);

  static const char *const PATHS[]
      = { "dp_rst.blue", "dp_rst.cf32", "dp_rst.csv" };
  static const int FT[] = { WFM_FT_BLUE, WFM_FT_RAW, WFM_FT_CSV };
  for (size_t i = 0; i < sizeof FT / sizeof *FT; i++)
    {
      FILE *fp = fopen (PATHS[i], "wb");
      DP_REQUIRE_MSG (fp != NULL, "open");
      wfm_writer_state_t *w
          = wfm_writer_open (fp, FT[i], 0, 0, 2.4e6, 0.0, N, 0.0);
      DP_REQUIRE_MSG (w != NULL, "writer");
      if (FT[i] == WFM_FT_BLUE && attach_keywords (w))
        return 1;
      DP_REQUIRE_MSG (wfm_writer_write (w, x, N) == N, "write");
      DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "close");
      fclose (fp);

      wfm_reader_state_t *r = wfm_reader_create (PATHS[i], 0, 0);
      DP_REQUIRE_MSG (r != NULL, "open for reset");
      DP_REQUIRE_MSG (wfm_reader_read (r, N, a, N) == N, "first pass");
      wfm_reader_reset (r);
      DP_REQUIRE_MSG (wfm_reader_read (r, N, b, N) == N,
                      "second pass reads N again");
      DP_REQUIRE_MSG (wfm_reader_read (r, 1, b + N - 1, 1) == 0,
                      "and stops at the end");
      wfm_reader_destroy (r);
      for (size_t k = 0; k + 1 < N; k++)
        DP_REQUIRE_MSG (a[k] == b[k], "reset replays the identical samples");
    }

  /* detached: payload is byte 0 of the .det, header is elsewhere */
  wfm_reader_state_t *r = wfm_reader_create ("dp_kw_det.hdr", 0, 0);
  DP_REQUIRE_MSG (r != NULL, "open detached");
  DP_REQUIRE_MSG (wfm_reader_read (r, N, a, N) == N, "detached first pass");
  wfm_reader_reset (r);
  DP_REQUIRE_MSG (wfm_reader_read (r, N, b, N) == N, "detached second pass");
  DP_REQUIRE_MSG (wfm_reader_num_keywords (r) == 2,
                  "keywords survive a reset");
  wfm_reader_destroy (r);
  for (size_t k = 0; k < N; k++)
    DP_REQUIRE_MSG (a[k] == b[k], "detached reset replays identically");
  return 0;
}

/* pass_capacity: emission stops at max_out (jm gh-138).
   wfm_reader_read_max_out(n) reports n (the read count is the per-call bound,
   gh-607), so the kernel's limit is the caller's count argument -- a caller
   with a buffer smaller than the count it asked for is rejected. */
static int
test_read_capacity (void)
{
  wfm_reader_state_t *r = wfm_reader_create ("dp_reader.blue", 0, 0);
  float _Complex y[16];
  DP_REQUIRE_MSG (r, "reopen the BLUE capture");
  for (size_t i = 0; i < 16; i++)
    y[i] = 42.0f + 42.0f * I;

  DP_REQUIRE_MSG (wfm_reader_read (r, 16, y, 3) == 3, "read stops at max_out");
  for (size_t i = 3; i < 16; i++)
    DP_REQUIRE_MSG (y[i] == 42.0f + 42.0f * I, "tail untouched");

  /* Zero capacity reads nothing. */
  DP_REQUIRE_MSG (wfm_reader_read (r, 16, y, 0) == 0,
                  "zero capacity reads nothing");
  wfm_reader_destroy (r);
  return 0;
}

/* The extended header at the END of an ATTACHED file -- the layout doppler's
   own writer produces, because a stream's length is not known until close
   (BLUE 3.3 permits it explicitly). Two things have to hold at once: the
   keywords decode, AND read() must stop at data_size rather than handing back
   the extended-header bytes as samples. The second is the one that would fail
   silently, so it is asserted on the sample values, not just the count. */
static int
test_ext_header_at_end_of_attached_file (void)
{
  size_t n  = 8;
  double sr = 2.048e6;

  FILE *fp = fopen ("dp_extend.blue", "wb");
  DP_REQUIRE_MSG (fp, "open for write");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_BLUE, 0, 0, sr, 0.0, n, 0.0);
  DP_REQUIRE_MSG (w, "writer open");
  DP_REQUIRE_MSG (wfm_writer_add_keyword (w, "SRATE", 'D', &sr, 1) == 0,
                  "typed kw");
  float _Complex xs[8];
  for (size_t i = 0; i < n; i++)
    xs[i] = (float)(i + 1) + 0.0f * I;
  DP_REQUIRE_MSG (wfm_writer_write (w, xs, n) == (int)n, "write samples");
  DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "close");
  fclose (fp);

  /* The extended header must sit AFTER the data, on a 512-byte boundary. */
  wfm_reader_state_t *r = wfm_reader_create ("dp_extend.blue", 0, 0);
  DP_REQUIRE_MSG (r, "reader open");
  const wfm_keyword_t *es = wfm_reader_find_header_field (r, "ext_start");
  const wfm_keyword_t *ds = wfm_reader_find_header_field (r, "data_start");
  DP_REQUIRE_MSG (es && ds, "ext_start/data_start present in the header dict");
  if (es && ds)
    {
      int32_t blocks;
      double  dstart;
      memcpy (&blocks, es->value, sizeof blocks);
      memcpy (&dstart, ds->value, sizeof dstart);
      DP_REQUIRE_MSG ((long)blocks * 512L > (long)dstart,
                      "ext header follows the data");
    }

  /* The keyword decodes, and its TYPE survives (a double, not a string). */
  const wfm_keyword_t *kw = wfm_reader_find_keyword (r, "SRATE");
  DP_REQUIRE_MSG (kw && kw->type == 'D' && kw->count == 1,
                  "SRATE is a 1-element D");
  if (kw)
    {
      double got;
      memcpy (&got, kw->value, sizeof got);
      DP_REQUIRE_MSG (fabs (got - sr) < 1.0, "SRATE value round-trips");
    }

  /* And the payload stops at data_size: the extended header is NOT samples. */
  float _Complex y[16];
  size_t got = wfm_reader_read (r, 16, y, 16);
  DP_REQUIRE_MSG (got == n, "read returns exactly the declared sample count");
  for (size_t i = 0; i < got; i++)
    DP_REQUIRE_MSG (crealf (y[i]) == (float)(i + 1), "sample values intact");
  DP_REQUIRE_MSG (wfm_reader_read (r, 16, y, 16) == 0,
                  "and stops at the payload end");
  wfm_reader_destroy (r);
  return 0;
}

/* The HCB's own keyword area (keylength@160, keywords@164): where X-Midas
   commonly puts short metadata, and what doppler used to drop on the floor. */
static int
test_hcb_keyword_area (void)
{
  FILE *fp = fopen ("dp_hcbkw.blue", "wb");
  DP_REQUIRE_MSG (fp, "open for write");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_BLUE, 0, 0, 1e6, 0.0, 4, 0.0);
  DP_REQUIRE_MSG (w, "writer open");
  /* Only the six standard main-header keywords (BLUE 1.1 3.4.1) go to the
     HCB area, and only as ASCII. A user keyword -- even an ASCII one -- goes
     to the extended header, because 3.4 reserves the 92-byte area and lets
     other systems delete user keywords found there. */
  DP_REQUIRE_MSG (wfm_writer_add_keyword (w, "VER", 'A', "1.1", 3) == 0,
                  "std kw");
  DP_REQUIRE_MSG (wfm_writer_add_keyword (w, "NAME", 'A', "hello", 5) == 0,
                  "user kw");
  double sr = 1e6;
  DP_REQUIRE_MSG (wfm_writer_add_keyword (w, "SRATE", 'D', &sr, 1) == 0,
                  "typed kw");
  float _Complex xs[4] = { 1, 2, 3, 4 };
  DP_REQUIRE_MSG (wfm_writer_write (w, xs, 4) == 4, "write");
  DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "close");
  fclose (fp);

  wfm_reader_state_t  *r  = wfm_reader_create ("dp_hcbkw.blue", 0, 0);
  const wfm_keyword_t *kl = wfm_reader_find_header_field (r, "keylength");
  DP_REQUIRE_MSG (r && kl, "reader open, keylength present");
  if (kl)
    {
      int32_t v;
      memcpy (&v, kl->value, sizeof v);
      DP_REQUIRE_MSG (v > 0, "keylength patched into the HCB");
    }
  /* Both sources merge: the caller cannot tell which block a key came from. */
  const wfm_keyword_t *v = wfm_reader_find_keyword (r, "VER");
  const wfm_keyword_t *a = wfm_reader_find_keyword (r, "NAME");
  const wfm_keyword_t *d = wfm_reader_find_keyword (r, "SRATE");
  DP_REQUIRE_MSG (v && v->type == 'A', "HCB-area keyword decoded");
  DP_REQUIRE_MSG (a && a->type == 'A',
                  "user ASCII keyword decoded (ext header)");
  DP_REQUIRE_MSG (d && d->type == 'D',
                  "extended-header keyword decoded, type intact");
  wfm_reader_destroy (r);
  return 0;
}

/* ── centre frequency ─────────────────────────────────────────────────────
 *
 * Type-1000 has no HCB field for it, so it travels as a keyword — and the
 * captures that carry one put it in the HCB keyword area as ASCII, which the
 * reader ignored entirely. These build such a file by hand rather than through
 * wfm_writer, because the point is reading somebody ELSE'S capture.
 */

/* A minimal attached cf32 BLUE capture whose only metadata is one ASCII
   "KEY=VALUE\0" pair in the HCB keyword area (BLUE 1.1 3.1.1.24.1). */
static int
write_hcb_only_capture (const char *path, const char *pair)
{
  uint8_t h[512] = { 0 };
  float _Complex data[4]
      = { 1.0f + 2.0f * I, 3.0f + 4.0f * I, 5.0f + 6.0f * I, 7.0f + 8.0f * I };
  memcpy (h + 0, "BLUE", 4);
  memcpy (h + 4, "EEEI", 4);
  memcpy (h + 8, "EEEI", 4);
  int32_t i32 = 1000;
  memcpy (h + 48, &i32, 4);
  double f64 = 512.0;
  memcpy (h + 32, &f64, 8);
  f64 = (double)sizeof data;
  memcpy (h + 40, &f64, 8);
  h[52]       = 'C';
  h[53]       = 'F';
  size_t klen = strlen (pair) + 1u; /* the NUL terminator counts */
  i32         = (int32_t)klen;
  memcpy (h + 160, &i32, 4);
  memcpy (h + 164, pair, klen);
  f64 = 1e-6;
  memcpy (h + 264, &f64, 8);

  FILE *fp = fopen (path, "wb");
  if (!fp)
    return -1;
  int ok = fwrite (h, 1, 512, fp) == 512
           && fwrite (data, 1, sizeof data, fp) == sizeof data;
  fclose (fp);
  return ok ? 0 : -1;
}

/* One ASCII HCB keyword in, one resolved (fc, fc_source) out. */
static int
fc_from_pair (const char *pair, double *fc, int *src)
{
  const char *path = "dp_reader_fc.blue";
  if (write_hcb_only_capture (path, pair))
    return -1;
  wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
  if (!r)
    return -1;
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  *fc  = info.fc;
  *src = info.fc_source;
  wfm_reader_destroy (r);
  return 0;
}

static int
test_fc_from_keywords (void)
{
  double fc;
  int    src;

  /* Every conventional tag, ASCII, in the HCB area. */
  DP_REQUIRE_MSG (fc_from_pair ("FREQ=2400000000", &fc, &src) == 0,
                  "FREQ capture");
  DP_REQUIRE_MSG (fc == 2.4e9 && src == WFM_FC_FREQ, "FREQ resolves");
  DP_REQUIRE_MSG (fc_from_pair ("RF_FREQ=1.5e9", &fc, &src) == 0,
                  "RF_FREQ capture");
  DP_REQUIRE_MSG (fc == 1.5e9 && src == WFM_FC_RF_FREQ, "RF_FREQ resolves");
  DP_REQUIRE_MSG (fc_from_pair ("CENTER_FREQ=70e6", &fc, &src) == 0,
                  "CENTER capture");
  DP_REQUIRE_MSG (fc == 70e6 && src == WFM_FC_CENTER_FREQ,
                  "CENTER_FREQ resolves");
  DP_REQUIRE_MSG (fc_from_pair ("F_C=-3", &fc, &src) == 0, "F_C capture");
  DP_REQUIRE_MSG (fc == -3.0 && src == WFM_FC_F_C, "F_C resolves");

  /* An explicit zero is a READING, and must not read as "not found" -- that
     distinction is the entire reason fc_source exists. */
  DP_REQUIRE_MSG (fc_from_pair ("FREQ=0", &fc, &src) == 0, "baseband capture");
  DP_REQUIRE_MSG (fc == 0.0 && src == WFM_FC_FREQ, "declared baseband");

  /* Nothing to read, and nothing invented. */
  DP_REQUIRE_MSG (fc_from_pair ("COMMENT=hi", &fc, &src) == 0,
                  "no-freq capture");
  DP_REQUIRE_MSG (fc == 0.0 && src == WFM_FC_NONE, "absent stays absent");

  /* A value we cannot interpret is left alone rather than guessed at: "2.4
     GHz" parsed as a bare number would be wrong by a factor of a billion. */
  DP_REQUIRE_MSG (fc_from_pair ("FREQ=2.4 GHz", &fc, &src) == 0,
                  "united capture");
  DP_REQUIRE_MSG (fc == 0.0 && src == WFM_FC_NONE,
                  "a units suffix is not a number");
  return 0;
}

/* fs_source / t0_source: the provenance half of the metadata, and the case
   that motivated it. A BLUE writer given no t0 leaves the timecode field
   zero, so the capture reads back t0 == 0.0 -- which converted naively
   through the J1950 offset is a confident, wrong 1950-01-01. WFM_T0_NONE is
   what makes that distinguishable from a capture that really does declare a
   start time.

   This is the UNSET half; test_t0_round_trips_through_blue is the SET half.
   Both are needed: a writer that always stamped a timecode would make
   t0_source pointless, and one that never could would make it unreachable. */
static int
test_fs_and_t0_provenance (void)
{
  const char *path     = "dp_reader_prov.blue";
  float _Complex xs[4] = { 0 };

  FILE *fp = fopen (path, "wb");
  DP_REQUIRE_MSG (fp, "open for write");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_BLUE, 0, 0, 2.5e6, 0.0, 4, 0.0);
  DP_REQUIRE_MSG (w, "writer open");
  DP_REQUIRE_MSG (wfm_writer_write (w, xs, 4) == 4, "write");
  DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "close");
  fclose (fp);

  wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
  DP_REQUIRE_MSG (r, "reader open");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);

  /* The rate IS declared, via xdelta, so it is attributable. */
  DP_REQUIRE_MSG (info.fs == 2.5e6, "fs round-trips");
  DP_REQUIRE_MSG (info.fs_source == WFM_FS_BLUE_XDELTA,
                  "fs attributed to xdelta");
  DP_REQUIRE_MSG (wfm_reader_get_fs_source (r) == WFM_FS_BLUE_XDELTA,
                  "fs accessor");

  /* The start time is NOT: doppler never writes one. It must report
     "unknown" rather than 1950. */
  DP_REQUIRE_MSG (info.t0_source == WFM_T0_NONE,
                  "t0 unset on a doppler-written BLUE");
  DP_REQUIRE_MSG (info.t0_unix_sec == 0.0, "t0 is 0.0 when unset");
  DP_REQUIRE_MSG (wfm_reader_get_t0_source (r) == WFM_T0_NONE,
                  "t0 source accessor");
  DP_REQUIRE_MSG (wfm_reader_get_t0 (r) == 0.0, "t0 accessor");
  wfm_reader_destroy (r);

  /* A raw capture carries no metadata at all, so neither is attributable --
     and fs == 0.0 must be reported as "not found", never as a rate. */
  const char *rawp = "dp_reader_prov.raw";
  FILE       *rf   = fopen (rawp, "wb");
  DP_REQUIRE_MSG (rf, "raw open");
  fwrite (xs, sizeof xs, 1, rf);
  fclose (rf);
  wfm_reader_state_t *rr = wfm_reader_create (rawp, 0, 0);
  DP_REQUIRE_MSG (rr, "raw reader open");
  wfm_reader_info (rr, &info);
  DP_REQUIRE_MSG (info.fs_source == WFM_FS_NONE, "raw declares no rate");
  DP_REQUIRE_MSG (info.t0_source == WFM_T0_NONE, "raw declares no start time");
  wfm_reader_destroy (rr);
  remove (path);
  remove (rawp);
  return 0;
}

/* The SET half: a t0 handed to the writer must survive the round trip
   through the J1950 timecode field and come back attributed.

   The epoch conversion is the part worth pinning. BLUE counts seconds from
   1950-01-01 and UNIX from 1970-01-01, so a writer that forgot the offset
   would still produce a file that reads back *some* time -- 20 years wrong,
   and only obviously wrong if you look. Comparing against the exact UNIX
   value the writer was given catches a dropped, doubled or sign-flipped
   offset, none of which a "is a timecode present" check would notice. */
static int
test_t0_round_trips_through_blue (void)
{
  const char *path = "dp_reader_t0.blue";
  /* 2026-08-05T04:15:30Z. A whole second: the field is a double of seconds,
     so an exact comparison is legitimate here and stays honest about what
     the format can carry. */
  const double t0      = 1785903330.0;
  float _Complex xs[4] = { 0 };

  FILE *fp = fopen (path, "wb");
  DP_REQUIRE_MSG (fp, "open for write");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_BLUE, 0, 0, 2.5e6, 0.0, 4, t0);
  DP_REQUIRE_MSG (w, "writer open");
  DP_REQUIRE_MSG (wfm_writer_write (w, xs, 4) == 4, "write");
  DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "close");
  fclose (fp);

  wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
  DP_REQUIRE_MSG (r, "reader open");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  DP_REQUIRE_MSG (info.t0_source == WFM_T0_BLUE_TIMECODE,
                  "t0 attributed to timecode");
  DP_REQUIRE_MSG (info.t0_unix_sec == t0,
                  "t0 round-trips as the SAME unix instant");
  DP_REQUIRE_MSG (wfm_reader_get_t0 (r) == t0, "t0 accessor agrees");
  DP_REQUIRE_MSG (wfm_reader_get_t0_source (r) == WFM_T0_BLUE_TIMECODE,
                  "t0 source accessor agrees");
  wfm_reader_destroy (r);
  remove (path);
  return 0;
}

/* The writer's two copies must never be readable as disagreeing. */
static int
test_fc_write_side (void)
{
  const char *path     = "dp_reader_fcw.blue";
  double      fc       = 1234567890.123456;
  float _Complex xs[4] = { 0 };

  FILE *fp = fopen (path, "wb");
  DP_REQUIRE_MSG (fp, "open for write");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_BLUE, 0, 0, 1e6, fc, 4, 0.0);
  DP_REQUIRE_MSG (w, "writer open");
  /* A standard HCB keyword makes close() rewrite the whole 92-byte area --
     the path on which a frequency written at open could be lost. */
  DP_REQUIRE_MSG (wfm_writer_add_keyword (w, "VER", 'A', "1.1", 3) == 0,
                  "std kw");
  DP_REQUIRE_MSG (wfm_writer_write (w, xs, 4) == 4, "write");
  DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "close");
  fclose (fp);

  wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
  DP_REQUIRE_MSG (r, "reader open");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  /* Exactly, not approximately: the typed extended-header copy is preferred
     precisely because it did not round-trip through a decimal string. */
  DP_REQUIRE_MSG (info.fc == fc, "fc survives to full precision");
  DP_REQUIRE_MSG (info.fc_source == WFM_FC_FREQ,
                  "and knows where it came from");
  DP_REQUIRE_MSG (wfm_reader_find_keyword (r, "VER") != NULL,
                  "VER survived too");
  wfm_reader_destroy (r);
  return 0;
}

/* ── the wrong-hint tell ──────────────────────────────────────────────────*/

static int
test_trailing_bytes (void)
{
  const char *path = "dp_reader_trail.raw";
  int8_t      raw[10]; /* 5 ci8 samples */
  memset (raw, 0, sizeof raw);
  FILE *fp = fopen (path, "wb");
  DP_REQUIRE_MSG (fp, "open for write");
  DP_REQUIRE_MSG (fwrite (raw, 1, sizeof raw, fp) == sizeof raw, "write raw");
  fclose (fp);

  wfm_reader_info_t info;
  /* Right hint: the file divides exactly. */
  wfm_reader_state_t *r = wfm_reader_create (path, 4, 0); /* ci8 */
  DP_REQUIRE_MSG (r, "reader open ci8");
  wfm_reader_info (r, &info);
  DP_REQUIRE_MSG (info.trailing_bytes == 0 && info.num_samples == 5,
                  "ci8 divides");
  wfm_reader_destroy (r);

  /* Wrong hint: 10 bytes is one cf32 sample and two bytes nobody can use.
     Nothing fails -- a headerless file type cannot check a hint -- so the
     remainder is the only thing that says so. */
  r = wfm_reader_create (path, 0, 0); /* cf32 */
  DP_REQUIRE_MSG (r, "reader open cf32");
  wfm_reader_info (r, &info);
  DP_REQUIRE_MSG (info.trailing_bytes == 2, "wrong stride leaves a remainder");
  DP_REQUIRE_MSG (info.num_samples == 1, "and only whole samples are counted");
  wfm_reader_destroy (r);
  return 0;
}

/* ── detection by content ─────────────────────────────────────────────────*/

static int
test_detects_by_content_not_extension (void)
{
  wfm_reader_info_t info;

  /* A CSV that got renamed is still a CSV. */
  const char *csv = "dp_reader_named.dat";
  FILE       *fp  = fopen (csv, "w");
  DP_REQUIRE_MSG (fp, "open csv");
  fputs ("1.0,2.0\n3.0,4.0\n5.0,6.0\n", fp);
  fclose (fp);
  wfm_reader_state_t *r = wfm_reader_create (csv, 0, 0);
  DP_REQUIRE_MSG (r, "reader open renamed csv");
  wfm_reader_info (r, &info);
  DP_REQUIRE_MSG (info.file_type == WFM_FT_CSV,
                  "text I,Q is CSV whatever it is named");
  DP_REQUIRE_MSG (info.num_samples == 3, "and its length is counted");
  float _Complex y[4];
  DP_REQUIRE_MSG (wfm_reader_read (r, 4, y, 4) == 3, "reads as CSV");
  DP_REQUIRE_MSG (crealf (y[0]) == 1.0f && cimagf (y[2]) == 6.0f,
                  "values intact");
  wfm_reader_destroy (r);

  /* And the reverse: the BLUE magic outranks a .csv name. */
  DP_REQUIRE_MSG (write_hcb_only_capture ("dp_reader_named.csv", "COMMENT=x")
                      == 0,
                  "write blue as .csv");
  r = wfm_reader_create ("dp_reader_named.csv", 0, 0);
  DP_REQUIRE_MSG (r, "reader open misnamed blue");
  wfm_reader_info (r, &info);
  DP_REQUIRE_MSG (info.file_type == WFM_FT_BLUE,
                  "the magic wins over the name");
  DP_REQUIRE_MSG (info.num_samples == 4,
                  "and the payload is located correctly");
  wfm_reader_destroy (r);
  return 0;
}

/* ── SigMF is a pair ──────────────────────────────────────────────────────*/

static int
test_sigmf_pair_from_create (void)
{
  const char *path     = "dp_reader_pair.sigmf-data";
  float _Complex xs[4] = { 0 };
  /* A path-opened writer owns BOTH halves; the sidecar carries the datatype,
     so emitting only the samples produces an undecodable capture. */
  wfm_writer_state_t *w = wfm_writer_create (path, 2e6, WFM_FT_SIGMF, 3, 0,
                                             1.2e9, 4, 0.0, 0.0, true);
  DP_REQUIRE_MSG (w, "writer create");
  DP_REQUIRE_MSG (wfm_writer_write (w, xs, 4) == 4, "write");
  DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "close");

  wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
  DP_REQUIRE_MSG (r, "reader open — the sidecar must exist");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  DP_REQUIRE_MSG (info.file_type == WFM_FT_SIGMF, "detected as SigMF");
  DP_REQUIRE_MSG (info.sample_type == 3, "ci16 recovered from the sidecar");
  DP_REQUIRE_MSG (info.fs == 2e6 && info.fc == 1.2e9, "fs/fc recovered");
  DP_REQUIRE_MSG (info.fc_source == WFM_FC_SIGMF,
                  "and attributed to the sidecar");
  wfm_reader_destroy (r);

  /* Both halves are found by NAME, so the name is part of the format. The
     control proves the refusal is about the extension and not about the path
     being unwritable -- otherwise this pair of checks would pass vacuously. */
  wfm_writer_state_t *ok = wfm_writer_create (
      "dp_reader_pair.bin", 2e6, WFM_FT_RAW, 3, 0, 0.0, 4, 0.0, 0.0, false);
  DP_REQUIRE_MSG (ok != NULL, "the same path is writable as raw");
  DP_REQUIRE_MSG (wfm_writer_close (ok) == 0, "control close");
  DP_REQUIRE_MSG (wfm_writer_create ("dp_reader_pair.bin", 2e6, WFM_FT_SIGMF,
                                     3, 0, 0.0, 4, 0.0, 0.0, true)
                      == NULL,
                  "a SigMF path must end in .sigmf-data");
  return 0;
}

/* ── SigMF core:datetime: the writer's stamp, read back ──────────────────
 *
 * The pair above proves the datatype/fs/fc path. This proves the one that
 * used to be missing: `core:datetime` is an ISO 8601 string, the reader had
 * no parser for one, and every SigMF capture therefore reported
 * WFM_T0_NONE -- a BLUE replay landed on its own timeline and a SigMF replay
 * landed on the machine replaying it.
 *
 * Written by the writer rather than by hand, so the check is against the
 * spelling this library actually emits, and it goes red if either face
 * changes. The refusal case IS hand-written, because the writer cannot
 * produce it.
 */
static int
test_sigmf_datetime_round_trips (void)
{
  const char *path     = "dp_reader_t0.sigmf-data";
  float _Complex xs[4] = { 0 };
  /* 2026-08-05T04:15:30Z, the same instant test_dp_isotime.c pins. */
  const double        t0 = 1785903330.0;
  wfm_writer_state_t *w  = wfm_writer_create (path, 2e6, WFM_FT_SIGMF, 0, 0,
                                              1.2e9, 4, 0.0, t0, true);
  DP_REQUIRE_MSG (w, "writer create");
  DP_REQUIRE_MSG (wfm_writer_write (w, xs, 4) == 4, "write");
  DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "close");

  wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
  DP_REQUIRE_MSG (r, "reader open");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  DP_REQUIRE_MSG (info.t0_source == WFM_T0_SIGMF,
                  "core:datetime must be attributed to the sidecar");
  /* The sidecar renders microseconds, so the round trip is exact to 1 us. */
  DP_REQUIRE_MSG (dp_near (info.t0_unix_sec, t0, 1e-6),
                  "the instant must survive the round trip");
  wfm_reader_destroy (r);

  /* A stamp with no timezone: refused, and the capture reports "not found"
     rather than a time that is wrong by however many hours the writer was
     from UTC. Everything else in the sidecar still reads. */
  const char *mpath = "dp_reader_t0.sigmf-meta";
  FILE       *mf    = fopen (mpath, "w");
  DP_REQUIRE (mf != NULL);
  fputs ("{\"global\":{\"core:datatype\":\"cf32_le\","
         "\"core:version\":\"1.0.0\"},"
         "\"captures\":[{\"core:sample_start\":0,"
         "\"core:datetime\":\"2026-08-05T04:15:30\"}],"
         "\"annotations\":[]}",
         mf);
  DP_REQUIRE (fclose (mf) == 0);
  r = wfm_reader_create (path, 0, 0);
  DP_REQUIRE_MSG (r, "reader open with the hand-written sidecar");
  wfm_reader_info (r, &info);
  DP_REQUIRE_MSG (info.t0_source == WFM_T0_NONE,
                  "a zone-less stamp must read as not-found, not as UTC");
  DP_REQUIRE (info.t0_unix_sec == 0.0);
  DP_REQUIRE_MSG (info.sample_type == 0,
                  "the rest of the sidecar still reads");
  wfm_reader_destroy (r);

  remove (path);
  remove (mpath);
  return 0;
}

/* ── Every spelling of core:datetime the parser accepts, and refuses ─────
 *
 * These belong HERE, not only in test_dp_isotime.c, and the reason is
 * measurable rather than stylistic: the coverage build attributes C lines to
 * `libdoppler.so` alone (see COV_IGNORE in the Makefile), so a `static
 * inline` in a header is measured only through the LIBRARY translation units
 * that call it. `dp_isotime_parse` has exactly one such caller —
 * wfm_reader_core.c — so its branches are covered by what a capture can say,
 * and by nothing else. The unit test proves the arithmetic; this proves the
 * shipped path reaches every branch of it.
 *
 * Each case writes one sidecar beside a real `.sigmf-data` body and opens it.
 * An accepted stamp reports WFM_T0_SIGMF and its instant; a refused one
 * reports WFM_T0_NONE and 0.0, and the rest of the sidecar still reads.
 */
static int
open_with_datetime (const char *path, const char *datetime, double *t0_out,
                    int *src_out)
{
  char  mpath[256];
  FILE *mf;
  snprintf (mpath, sizeof mpath, "%.*s.sigmf-meta",
            (int)(strlen (path) - 11), path); /* strip .sigmf-data */
  mf = fopen (mpath, "w");
  if (!mf)
    return -1;
  fprintf (mf,
           "{\"global\":{\"core:datatype\":\"cf32_le\","
           "\"core:version\":\"1.0.0\"},"
           "\"captures\":[{\"core:sample_start\":0");
  if (datetime)
    fprintf (mf, ",\"core:datetime\":\"%s\"", datetime);
  fprintf (mf, "}],\"annotations\":[]}");
  if (fclose (mf) != 0)
    return -1;

  wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
  if (!r)
    return -1;
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  *t0_out  = info.t0_unix_sec;
  *src_out = info.t0_source;
  /* The datatype is read from the same sidecar, so a case that got this far
     proves the document parsed and only the stamp was in question. */
  int ok = (info.sample_type == 0);
  wfm_reader_destroy (r);
  remove (mpath);
  return ok ? 0 : -1;
}

static int
test_sigmf_datetime_every_spelling (void)
{
  const char    *path  = "dp_reader_dt.sigmf-data";
  float _Complex xs[4] = { 0 };
  FILE          *df    = fopen (path, "wb");
  DP_REQUIRE (df != NULL);
  DP_REQUIRE (fwrite (xs, sizeof xs[0], 4, df) == 4);
  DP_REQUIRE (fclose (df) == 0);

  /* 2026-08-05T04:15:30Z, the instant test_dp_isotime.c pins. */
  const double T = 1785903330.0;
  struct
  {
    const char *stamp;
    double      want; /* the instant, when accepted */
  } ok_cases[] = {
    { "1970-01-01T00:00:00Z", 0.0 },
    { "2026-08-05T04:15:30Z", T },
    { "20260805T041530Z", T },                    /* basic spelling      */
    { "2026-08-05T04:15:30.5Z", T + 0.5 },        /* one fraction digit  */
    { "2026-08-05T04:15:30.123456Z", T + 0.123456 },
    { "2026-08-05T04:15:30.1234567891Z", T + 0.123456789 }, /* >9, cut   */
    { "2026-08-05T04:15:30,25Z", T + 0.25 },      /* comma fraction      */
    { "2026-08-05t04:15:30Z", T },                /* lowercase T         */
    { "2026-08-05 04:15:30Z", T },                /* space separator     */
    { "2026-08-05T05:15:30+01:00", T },           /* offset, applied     */
    { "2026-08-05T03:15:30-0100", T },            /* basic offset        */
    { "2026-08-05T04:15:30z", T },                /* lowercase zone      */
    { "2016-12-31T23:59:60Z", 1483228800.0 },     /* leap second         */
  };
  for (size_t i = 0; i < sizeof ok_cases / sizeof *ok_cases; i++)
    {
      double t0 = -1.0;
      int    src = -1;
      DP_REQUIRE_MSG (open_with_datetime (path, ok_cases[i].stamp, &t0, &src)
                          == 0,
                      "the sidecar did not parse");
      DP_REQUIRE_MSG (src == WFM_T0_SIGMF, ok_cases[i].stamp);
      DP_REQUIRE_MSG (dp_near (t0, ok_cases[i].want, 1e-6),
                      ok_cases[i].stamp);
    }

  /* Refused, every one reporting "not found" rather than a plausible time.
     The first is the decision rather than a syntax check: a zone-less stamp
     read as UTC is wrong by however many hours the writer was from it. */
  const char *bad_cases[] = {
    "2026-08-05T04:15:30",   /* NO ZONE — refused, never assumed          */
    "",                      /* shorter than any legal stamp              */
    "2026-08-05",            /* a date is not an instant                  */
    "2026-13-05T00:00:00Z",  /* month                                     */
    "2026-08-32T00:00:00Z",  /* day                                       */
    "2026-08-05T24:00:00Z",  /* hour                                      */
    "2026-08-05T00:60:00Z",  /* minute                                    */
    "2026-08-05T00:00:61Z",  /* second, past the leap                     */
    "2026-0805T00:00:00Z",   /* half-separated is not a spelling          */
    "20260805T00:00:00Z",    /* nor is the other half                     */
    "2026-08-0XT00:00:00Z",  /* a non-digit where a digit belongs         */
    "2026-08-05T00:00:00Zx", /* trailing junk is a different timestamp    */
    "2026-08-05T00:00:00.Z", /* a fraction point with no fraction         */
    "2026-08-05T00:00:00+99:00", /* an offset no zone has                 */
    "2026-08-05T00:00:00+0X",    /* a non-digit in the offset hour        */
    "2026-08-05T00:00:00+01:0X", /* ... and in its minute                 */
    "20X6-08-05T00:00:00Z",      /* a non-digit in the year               */
    "2026x08-05T00:00:00Z",      /* the first separator                   */
    "2026-0X-05T00:00:00Z",      /* the month                             */
    "2026-08-0XT00:00:00Z",      /* the day                               */
    "2026-08-05X00:00:00Z",      /* the date/time separator               */
    "2026-08-05TXX:00:00Z",      /* the hour                              */
    "2026-08-05T00x00:00Z",      /* the separator after it                */
    "2026-08-05T00:00x00Z",      /* and after the minute                  */
    "2026-08-05T00:00:XXZ",      /* the second                            */
  };
  for (size_t i = 0; i < sizeof bad_cases / sizeof *bad_cases; i++)
    {
      double t0 = -1.0;
      int    src = -1;
      DP_REQUIRE_MSG (open_with_datetime (path, bad_cases[i], &t0, &src) == 0,
                      "the sidecar did not parse");
      DP_REQUIRE_MSG (src == WFM_T0_NONE, bad_cases[i]);
      DP_REQUIRE_MSG (t0 == 0.0, bad_cases[i]);
    }

  /* And a sidecar with no core:datetime at all — the common case. */
  {
    double t0 = -1.0;
    int    src = -1;
    DP_REQUIRE (open_with_datetime (path, NULL, &t0, &src) == 0);
    DP_REQUIRE (src == WFM_T0_NONE);
    DP_REQUIRE (t0 == 0.0);
  }

  remove (path);
  return 0;
}

/* ── Following a capture that is still being written ──────────────────────
 * docs/design/end-of-capture.md sections 2a and 2b. Both were measured
 * defects before read_follow existed, and both are SILENT: the first
 * reports a clean end after two samples of an unbounded stream, the second
 * hands back plausible garbage at the wrong stride. Neither is caught by
 * any round-trip above, because a finished capture never grows under a
 * reader and never has a partial tail to meet. */

/* 2a: C stdio LATCHES the end-of-file indicator, so a reader that once
   caught up with the writer returns 0 forever even as the file grows.
   Sabotage: drop the clearerr() in follow_available and this goes red. */
static int
test_follow_resumes_after_catching_up (void)
{
  const char *path = "wfm_follow_grow.blue";
  FILE       *fp   = fopen (path, "wb+");
  DP_REQUIRE_MSG (fp, "open for write");
  /* total 0: an unbounded run declares no length, which is what wfmgen
     writes and what makes the declared bound a placeholder. */
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_BLUE, 3, 0, 2.4e6, 0.0, 0, 0.0);
  DP_REQUIRE_MSG (w, "writer open");
  float _Complex x[10];
  make_signal (x, 10);
  DP_REQUIRE_MSG (wfm_writer_write (w, x, 4) == 4, "wrote 4");
  DP_REQUIRE_MSG (wfm_writer_flush (w) == 0, "flush 4");

  wfm_reader_state_t *r = wfm_reader_create (path, 3, 0);
  DP_REQUIRE_MSG (r, "reader open");
  wfm_reader_set_follow_timeout_ms (r, 200);
  float _Complex y[16];
  DP_REQUIRE_MSG (wfm_reader_read_follow (r, 16, y, 16) == 4, "first 4");

  /* The reader has now hit EOF once. Grow the file underneath it. */
  DP_REQUIRE_MSG (wfm_writer_write (w, x + 4, 6) == 6, "wrote 6 more");
  DP_REQUIRE_MSG (wfm_writer_flush (w) == 0, "flush 6");
  size_t got = wfm_reader_read_follow (r, 16, y, 16);
  DP_REQUIRE_MSG (got == 6, "a reader that caught up still sees growth");
  DP_REQUIRE_MSG (wfm_reader_get_ending (r) == WFM_FOLLOW_NONE,
                  "growth is not an ending");
  wfm_reader_destroy (r);
  wfm_writer_close (w);
  fclose (fp);
  remove (path);
  return 0;
}

/* 2b, on the PLAIN read path, which is where the fix is load-bearing.
   read() asks for `max` samples and lets fread return what it can, so it
   over-reads into a partial sample and drops the remainder -- bytes fread
   has already taken. Every sample after that is one int16 out of phase,
   permanently, with no error anywhere.

   WHO WRITES 2.5 SAMPLES: not this library. Measured on a growing capture
   (3601 samples of the file size, zero non-aligned) -- stdio buffers in
   powers of two and every doppler sample size divides them, so our own
   writer's visible prefix is always whole samples. The partial tail comes
   from somewhere else: a foreign writer chunking with write(2), a short
   write on a full disk, a capture truncated mid-sample by a killed
   recorder (which is what wfm_reader_get_trailing_bytes exists to report),
   or a network filesystem. All reachable, none of them us -- so the file
   here is built by hand rather than by a Writer, deliberately.

   Sabotage-verified: removing the fseek-back in read_block turns the
   sequence below into [(1,2), (3,4)] and nothing more. The follow path
   masks this defect (follow_available clamps to whole samples), which is
   exactly why this test drives read() and not read_follow() -- two
   mechanisms covering one fault means neither is pinned. */
static int
test_read_never_consumes_a_partial_sample (void)
{
  const char   *path    = "wfm_partial.cf32";
  const int16_t head[5] = { 1, 2, 3, 4, 5 }; /* 2 samples + half of #3 */
  const int16_t tail[3] = { 6, 7, 8 };
  FILE         *fp      = fopen (path, "wb");
  DP_REQUIRE_MSG (fp, "open for write");
  DP_REQUIRE_MSG (fwrite (head, sizeof head, 1, fp) == 1, "wrote 2.5");
  fclose (fp);

  wfm_reader_state_t *r = wfm_reader_create (path, 3, 0);
  DP_REQUIRE_MSG (r, "reader open");
  float _Complex y[8];
  size_t n1 = wfm_reader_read (r, 8, y, 8);
  DP_REQUIRE_MSG (n1 == 2, "only whole samples are emitted");

  fp = fopen (path, "ab");
  DP_REQUIRE_MSG (fp, "reopen to append");
  DP_REQUIRE_MSG (fwrite (tail, sizeof tail, 1, fp) == 1, "completed #3, +#4");
  fclose (fp);

  size_t n2 = wfm_reader_read (r, 8, y + n1, 8 - n1);
  DP_REQUIRE_MSG (n2 == 2, "the completed sample and the next");
  for (int i = 0; i < 4; i++)
    {
      int re = (int)(crealf (y[i]) * 32767.0f + 0.5f);
      int im = (int)(cimagf (y[i]) * 32767.0f + 0.5f);
      DP_REQUIRE_MSG (re == 2 * i + 1 && im == 2 * i + 2,
                      "the stream never desynchronises");
    }
  wfm_reader_destroy (r);
  remove (path);
  return 0;
}

/* The ending is EXPLICIT: the writer's close patches data_size, and that
   placeholder -> real transition is what ends the wait. A file that has
   merely gone quiet is not an ending, which is what the timeout half
   pins. */
static int
test_follow_ends_on_the_marker_not_on_silence (void)
{
  const char *path = "wfm_follow_eof.blue";
  FILE       *fp   = fopen (path, "wb+");
  DP_REQUIRE_MSG (fp, "open for write");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_BLUE, 3, 0, 2.4e6, 0.0, 0, 0.0);
  DP_REQUIRE_MSG (w, "writer open");
  float _Complex x[8];
  make_signal (x, 8);
  DP_REQUIRE_MSG (wfm_writer_write (w, x, 8) == 8, "wrote 8");
  DP_REQUIRE_MSG (wfm_writer_flush (w) == 0, "flush");

  wfm_reader_state_t *r = wfm_reader_create (path, 3, 0);
  DP_REQUIRE_MSG (r, "reader open");
  wfm_reader_set_follow_timeout_ms (r, 150);
  float _Complex y[16];
  DP_REQUIRE_MSG (wfm_reader_read_follow (r, 16, y, 16) == 8, "drained 8");

  /* Quiet, but not over: a bounded wait must report TIMEOUT, never EOF. */
  DP_REQUIRE_MSG (wfm_reader_read_follow (r, 16, y, 16) == 0, "nothing yet");
  DP_REQUIRE_MSG (wfm_reader_get_ending (r) == WFM_FOLLOW_TIMEOUT,
                  "silence is not an ending");

  wfm_writer_close (w); /* patches data_size -- the marker */
  DP_REQUIRE_MSG (wfm_reader_read_follow (r, 16, y, 16) == 0, "drained");
  DP_REQUIRE_MSG (wfm_reader_get_ending (r) == WFM_FOLLOW_EOF,
                  "the marker ends it");
  wfm_reader_destroy (r);
  fclose (fp);
  remove (path);
  return 0;
}

/* The §3 shutdown rules. Testable here and now, in C, with no interrupt
   primitive anywhere: wfm_reader_set_stop_fn takes the predicate, so a test
   supplies its own. That is the whole reason the reader does not link
   dp_interrupt.c -- the policy is the caller's, and here the caller is this
   file. */
static int g_stop = 0;
static int
test_stop_requested (void)
{
  return g_stop;
}

/* DRAIN WINS. A stop does not discard what is already on disk: the reader
   hands back every whole sample it can see before it honours the request.
   Getting this wrong loses a tail by an amount that varies per run, because
   it is a race between two loops -- which is exactly the kind of defect a
   test has to pin rather than a reviewer notice.

   Sabotage: move the stop check above the follow_available() branch in
   wfm_reader_read_follow and this goes red. */
static int
test_follow_drains_before_honouring_a_stop (void)
{
  const char *path = "wfm_follow_drain.blue";
  FILE       *fp   = fopen (path, "wb+");
  DP_REQUIRE_MSG (fp, "open for write");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_BLUE, 3, 0, 2.4e6, 0.0, 0, 0.0);
  DP_REQUIRE_MSG (w, "writer open");
  float _Complex x[12];
  make_signal (x, 12);
  DP_REQUIRE_MSG (wfm_writer_write (w, x, 12) == 12, "wrote 12");
  DP_REQUIRE_MSG (wfm_writer_flush (w) == 0, "flush");

  wfm_reader_state_t *r = wfm_reader_create (path, 3, 0);
  DP_REQUIRE_MSG (r, "reader open");
  wfm_reader_set_stop_fn (r, test_stop_requested);
  wfm_reader_set_follow_grace_ms (r, 50); /* bounded, so a bug cannot hang */

  g_stop = 1; /* the stop is ALREADY requested before the first read */
  float _Complex y[16];
  size_t got = wfm_reader_read_follow (r, 16, y, 16);
  DP_REQUIRE_MSG (got == 12, "a stop does not discard what is on disk");
  DP_REQUIRE_MSG (wfm_reader_get_ending (r) == WFM_FOLLOW_NONE,
                  "draining is not an ending");

  /* Only once it is drained does the stop end the wait. */
  DP_REQUIRE_MSG (wfm_reader_read_follow (r, 16, y, 16) == 0, "drained");
  DP_REQUIRE_MSG (wfm_reader_get_ending (r) == WFM_FOLLOW_INTERRUPTED,
                  "a bounded grace expiring reports INTERRUPTED");
  g_stop = 0;
  wfm_reader_destroy (r);
  wfm_writer_close (w);
  fclose (fp);
  remove (path);
  return 0;
}

/* The two budgets are different clocks with different meanings, and the
   ending has to say which one expired: TIMEOUT means "more may come",
   INTERRUPTED means "a stop was asked for and the tail may be short". A
   reader that reported one for the other would be telling a caller its
   capture is complete when it is not.

   Sabotage: report a single ending for both and this goes red. */
static int
test_follow_distinguishes_timeout_from_interrupted (void)
{
  const char *path = "wfm_follow_clocks.blue";
  FILE       *fp   = fopen (path, "wb+");
  DP_REQUIRE_MSG (fp, "open for write");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_BLUE, 3, 0, 2.4e6, 0.0, 0, 0.0);
  DP_REQUIRE_MSG (w, "writer open");
  float _Complex x[4];
  make_signal (x, 4);
  DP_REQUIRE_MSG (wfm_writer_write (w, x, 4) == 4, "wrote 4");
  DP_REQUIRE_MSG (wfm_writer_flush (w) == 0, "flush");

  wfm_reader_state_t *r = wfm_reader_create (path, 3, 0);
  DP_REQUIRE_MSG (r, "reader open");
  wfm_reader_set_stop_fn (r, test_stop_requested);
  wfm_reader_set_follow_timeout_ms (r, 60);
  wfm_reader_set_follow_grace_ms (r, 60);
  float _Complex y[8];
  DP_REQUIRE_MSG (wfm_reader_read_follow (r, 8, y, 8) == 4, "drained 4");

  g_stop = 0; /* quiet, no stop: the WAIT budget expires */
  DP_REQUIRE_MSG (wfm_reader_read_follow (r, 8, y, 8) == 0, "nothing yet");
  DP_REQUIRE_MSG (wfm_reader_get_ending (r) == WFM_FOLLOW_TIMEOUT,
                  "no stop asked for -- TIMEOUT, and more may come");

  g_stop = 1; /* quiet, stop asked: the GRACE budget expires */
  DP_REQUIRE_MSG (wfm_reader_read_follow (r, 8, y, 8) == 0, "still nothing");
  DP_REQUIRE_MSG (wfm_reader_get_ending (r) == WFM_FOLLOW_INTERRUPTED,
                  "stop asked for -- INTERRUPTED, and the tail may be short");
  g_stop = 0;
  wfm_reader_destroy (r);
  wfm_writer_close (w);
  fclose (fp);
  remove (path);
  return 0;
}

/* flush() is what makes a written sample OBSERVABLE without ending the
   capture. Before it existed the only fflush was inside close(), so a
   follower waited on samples the producer had already produced and could
   not make appear -- measured at 3968 of 4096.

   Sabotage: drop the fflush from wfm_writer_flush and this goes red. */
static int
test_flush_makes_samples_observable (void)
{
  const char *path = "wfm_flush_vis.blue";
  FILE       *fp   = fopen (path, "wb+");
  DP_REQUIRE_MSG (fp, "open for write");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_BLUE, 3, 0, 2.4e6, 0.0, 0, 0.0);
  DP_REQUIRE_MSG (w, "writer open");
  /* The 512-byte HCB is buffered too, so land it before the reader opens --
     otherwise this measures "the reader could not parse a header that is
     not there yet", which is a different fact. */
  DP_REQUIRE_MSG (wfm_writer_flush (w) == 0, "header down");
  wfm_reader_state_t *r = wfm_reader_create (path, 3, 0);
  DP_REQUIRE_MSG (r, "reader open");

  float _Complex x[6];
  make_signal (x, 6);
  DP_REQUIRE_MSG (wfm_writer_write (w, x, 6) == 6, "wrote 6");
  /* Small enough to sit in the FILE buffer: nothing is on disk yet. */
  wfm_reader_set_follow_timeout_ms (r, 40);
  float _Complex y[8];
  DP_REQUIRE_MSG (wfm_reader_read_follow (r, 8, y, 8) == 0,
                  "buffered, unseen");

  DP_REQUIRE_MSG (wfm_writer_flush (w) == 0, "flush");
  DP_REQUIRE_MSG (wfm_reader_read_follow (r, 8, y, 8) == 6,
                  "flush makes them observable");
  wfm_reader_destroy (r);
  wfm_writer_close (w);
  fclose (fp);
  remove (path);
  return 0;
}

/* A SCALAR capture, written by our OWN writer rather than a hand-built
   fixture -- doppler#1032.
 *
 * The reader has always understood BLUE format mode 'S'; the writer emitted a
 * literal 'C' whatever it was handed, and no test wrote a real capture and
 * read it back, so the two halves of this library disagreed about what BLUE
 * is for as long as both existed. A fixture assembled here by hand would not
 * have caught it: it would have exercised the reader against bytes this
 * library never produced.
 *
 * The sample COUNT is the assertion that matters. Read as complex, a scalar
 * file yields half as many samples with every other one landing in Q --
 * plausible output from a file that said otherwise. */
static int
test_scalar_round_trips_through_our_own_writer (void)
{
  static const int    STYPES[] = { 5, 6, 7, 8, 9 }; /* f32 f64 i32 i16 i8 */
  static const double TOL[]    = { 1e-6, 1e-6, 1e-6, 1e-4, 2e-2 };
  for (unsigned k = 0; k < 5; k++)
    {
      const int stype = STYPES[k];
      float _Complex x[N], y[N];
      make_signal (x, N);

      char path[64];
      snprintf (path, sizeof path, "dp_scalar_%d.blue", stype);
      FILE *fp = fopen (path, "wb");
      DP_REQUIRE_MSG (fp, "open for write");
      wfm_writer_state_t *w
          = wfm_writer_open (fp, WFM_FT_BLUE, stype, 0, 1e6, 0.0, N, 0.0);
      DP_REQUIRE_MSG (w, "writer open");
      DP_REQUIRE_MSG (wfm_writer_write (w, x, N) == N, "wrote N");
      wfm_writer_close (w);
      fclose (fp);

      /* The mode character, read as a BYTE. Asking our own reader would let a
         reader that shared the writer's assumption agree with it. */
      fp = fopen (path, "rb");
      DP_REQUIRE_MSG (fp, "open for read");
      uint8_t hcb[512];
      DP_REQUIRE_MSG (fread (hcb, 1, 512, fp) == 512, "read hcb");
      fclose (fp);
      DP_REQUIRE_MSG (hcb[52] == 'S', "BLUE format mode is scalar");

      /* No hint: BLUE carries its own type, and if it did not this would be
         the complex default and the count below would halve. */
      wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
      DP_REQUIRE_MSG (r, "reader open");
      wfm_reader_info_t info;
      wfm_reader_info (r, &info);
      DP_REQUIRE_MSG (info.mode == WFM_MODE_SCALAR, "mode is scalar");
      DP_REQUIRE_MSG (info.sample_type == stype, "sample_type recovered");
      DP_REQUIRE_MSG (info.num_samples == N, "num_samples");

      size_t total = 0, n;
      while ((n = wfm_reader_read (r, N - total, y + total, N - total)) > 0)
        total += n;
      wfm_reader_destroy (r);
      DP_REQUIRE_MSG (total == N, "read back N samples");

      for (size_t i = 0; i < N; i++)
        {
          DP_REQUIRE_MSG (cimagf (y[i]) == 0.0f, "no Q in a scalar capture");
          DP_REQUIRE_MSG (fabs ((double)crealf (x[i]) - (double)crealf (y[i]))
                              < TOL[k],
                          "I round-trips");
        }
      remove (path);
    }
  return 0;
}

/* ── the surface the PYTHON BINDING calls, which is the one nothing tests
 *
 * Inventory against wfm_reader_core.h. Fifteen entry points had zero
 * mentions in any C test in the tree, and they are not a random fifteen:
 * they are almost exactly the set the generated binding uses.
 *
 *   metadata   the C suite reads it through wfm_reader_info(), which fills
 *              its struct from r->file_type, r->fs, r->fc, r->mode,
 *              r->endian DIRECTLY. `Reader.fs`, `.fc`, `.file_type`,
 *              `.sample_type`, `.mode`, `.endian` and `.num_samples` each
 *              go through a wfm_reader_get_* accessor instead. Two
 *              independent readers of one state, and only one was
 *              exercised: a transposed accessor would leave all 1473 lines
 *              of this file green and every Python property wrong.
 *
 *   maps       the C suite uses the find_* lookups; `Reader.keywords` and
 *              `Reader.header` ITERATE, through wfm_reader_num_keywords /
 *              _keyword_tag and _num_header_fields / _header_tag /
 *              _header_field. wfm_reader_ext_wfm_reader.c:827 and :991 are
 *              the call sites. None of the four enumerators was tested.
 *
 * So the sections below score the accessors against what was WRITTEN --
 * an external truth, not the other reader -- and then require the two
 * readers to agree, with a precondition that the values are not defaults
 * so the agreement cannot be vacuous.
 */
static int
test_the_accessor_surface (void)
{
  const char  *path = "dp_rd_acc.blue";
  const double FS = 2.4e6, FC = 1.42e9;
  float _Complex x[N];
  make_signal (x, N);

  /* ── written -> read back, through the accessors only ─────────────── */
  {
    remove (path);
    wfm_writer_state_t *w = wfm_writer_create (path, FS, WFM_FT_BLUE, 3, 0, FC,
                                               0, 0.0, 0.0, false);
    DP_REQUIRE_MSG (w, "accessors: writer");
    DP_REQUIRE_MSG (wfm_writer_write (w, x, N) == N, "accessors: write");
    DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "accessors: close");

    wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
    DP_REQUIRE_MSG (r, "accessors: reader");
    /* every one of these is a distinct field, so a transposed pair shows */
    DP_REQUIRE_MSG (wfm_reader_get_file_type (r) == WFM_FT_BLUE,
                    "get_file_type is the type detected from the content");
    DP_REQUIRE_MSG (wfm_reader_get_sample_type (r) == 3,
                    "get_sample_type is the ci16 the writer declared");
    DP_REQUIRE_MSG (wfm_reader_get_mode (r) == WFM_MODE_COMPLEX,
                    "get_mode is complex for a C-mode BLUE file");
    DP_REQUIRE_MSG (wfm_reader_get_endian (r) == 0, "get_endian is le");
    DP_REQUIRE_MSG (dp_near (wfm_reader_get_fs (r), FS, 1e-6),
                    "get_fs is the rate written, via BLUE xdelta");
    DP_REQUIRE_MSG (dp_near (wfm_reader_get_fc (r), FC, 1e-3),
                    "get_fc is the centre frequency written");
    DP_REQUIRE_MSG (wfm_reader_get_num_samples (r) == N,
                    "get_num_samples is the count written");
    /* fs and fc are DIFFERENT numbers here on purpose: 2.4e6 against
       1.42e9. A getter returning its neighbour's field is a swap this
       catches and a same-valued fixture would not. */
    DP_REQUIRE_MSG (wfm_reader_get_fs (r) != wfm_reader_get_fc (r),
                    "precondition: the two rates are distinguishable");
    wfm_reader_destroy (r);
  }

  /* ── the two readers must agree, and not vacuously ────────────────── */
  {
    wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
    DP_REQUIRE_MSG (r, "agree: reader");
    wfm_reader_info_t info;
    wfm_reader_info (r, &info);
    /* precondition first: an all-zero info would satisfy every comparison
       below against an all-zero accessor set. */
    DP_REQUIRE_MSG (info.fs != 0.0 && info.fc != 0.0 && info.num_samples != 0,
                    "precondition: info carries non-default values");
    DP_REQUIRE_MSG (info.file_type == wfm_reader_get_file_type (r),
                    "info.file_type == get_file_type");
    DP_REQUIRE_MSG (info.sample_type == wfm_reader_get_sample_type (r),
                    "info.sample_type == get_sample_type");
    DP_REQUIRE_MSG (info.mode == wfm_reader_get_mode (r),
                    "info.mode == get_mode");
    DP_REQUIRE_MSG (info.endian == wfm_reader_get_endian (r),
                    "info.endian == get_endian");
    DP_REQUIRE_MSG (info.fs == wfm_reader_get_fs (r), "info.fs == get_fs");
    DP_REQUIRE_MSG (info.fc == wfm_reader_get_fc (r), "info.fc == get_fc");
    DP_REQUIRE_MSG (info.num_samples == wfm_reader_get_num_samples (r),
                    "info.num_samples == get_num_samples");
    DP_REQUIRE_MSG (info.fc_source == wfm_reader_get_fc_source (r),
                    "info.fc_source == get_fc_source");
    DP_REQUIRE_MSG (info.fs_source == wfm_reader_get_fs_source (r),
                    "info.fs_source == get_fs_source");
    DP_REQUIRE_MSG (info.t0_source == wfm_reader_get_t0_source (r),
                    "info.t0_source == get_t0_source");
    DP_REQUIRE_MSG (info.trailing_bytes == wfm_reader_get_trailing_bytes (r),
                    "info.trailing_bytes == get_trailing_bytes");
    DP_REQUIRE_MSG (info.t0_unix_sec == wfm_reader_get_t0 (r),
                    "info.t0_unix_sec == get_t0");
    wfm_reader_destroy (r);
    remove (path);
  }

  /* ── provenance: fc 0.0 must be distinguishable from fc not found ──
   *
   * The header is explicit that this is the whole point -- "a genuine
   * baseband capture and a capture whose frequency this library failed to
   * find are otherwise indistinguishable" -- and get_fc_source had zero
   * mentions. Two captures, same fc == 0.0, different answers. */
  {
    const char *named = "dp_rd_src_named.blue";
    const char *bare  = "dp_rd_src_bare.raw";
    remove (named);
    remove (bare);

    wfm_writer_state_t *w = wfm_writer_create (named, 1e6, WFM_FT_BLUE, 3, 0,
                                               0.0, 0, 0.0, 0.0, false);
    DP_REQUIRE_MSG (w, "provenance: blue writer");
    DP_REQUIRE_MSG (wfm_writer_write (w, x, 64) == 64, "provenance: write");
    DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "provenance: close");

    /* a headerless raw file carries no metadata at all */
    wfm_writer_state_t *b = wfm_writer_create (bare, 1e6, WFM_FT_RAW, 3, 0,
                                               0.0, 0, 0.0, 0.0, false);
    DP_REQUIRE_MSG (b, "provenance: raw writer");
    DP_REQUIRE_MSG (wfm_writer_write (b, x, 64) == 64, "provenance: write2");
    DP_REQUIRE_MSG (wfm_writer_close (b) == 0, "provenance: close2");

    wfm_reader_state_t *rn = wfm_reader_create (named, 0, 0);
    wfm_reader_state_t *rb = wfm_reader_create (bare, 3, 0);
    DP_REQUIRE_MSG (rn && rb, "provenance: readers");
    /* both report 0.0, which is exactly why the SOURCE has to differ */
    DP_REQUIRE_MSG (wfm_reader_get_fc (rn) == 0.0
                        && wfm_reader_get_fc (rb) == 0.0,
                    "precondition: both centre frequencies read 0.0");
    DP_REQUIRE_MSG (wfm_reader_get_fs_source (rn) == WFM_FS_BLUE_XDELTA,
                    "a BLUE capture says its rate came from xdelta");
    DP_REQUIRE_MSG (wfm_reader_get_fs_source (rb) == WFM_FS_NONE,
                    "a headerless raw one says nothing carried a rate");
    DP_REQUIRE_MSG (wfm_reader_get_fs (rb) == 0.0,
                    "and so its rate is 0.0, not a guess");
    /* doppler's own BLUE writer leaves the timecode zero: that must read
       as NONE, or every capture it writes dates to 1950. */
    DP_REQUIRE_MSG (wfm_reader_get_t0_source (rn) == WFM_T0_NONE,
                    "an unset BLUE timecode is NONE, never 1950");
    DP_REQUIRE_MSG (wfm_reader_get_t0 (rn) == 0.0, "and t0 is 0.0");
    wfm_reader_destroy (rn);
    wfm_reader_destroy (rb);
    remove (named);
    remove (bare);
  }
  return 0;
}

/* ── the ITERATORS Reader.keywords and Reader.header are built from ──────
 *
 * The C suite reaches keywords and header fields through the find_*
 * lookups; the binding ITERATES with the index-based enumerators, and all
 * four of those were untested. The check is that enumerating recovers
 * exactly what looking up does -- every tag present, each resolving to the
 * same record -- because that equivalence is what makes the dict the
 * binding builds a faithful view rather than a plausible one.
 */
static int
test_the_enumerators (void)
{
  const char *path = "dp_rd_enum.blue";
  float _Complex x[N];
  make_signal (x, N);
  remove (path);

  wfm_writer_state_t *w = wfm_writer_create (path, 1e6, WFM_FT_BLUE, 3, 0, 0.0,
                                             0, 0.0, 0.0, false);
  DP_REQUIRE_MSG (w, "enum: writer");
  DP_REQUIRE_MSG (wfm_writer_write (w, x, N) == N, "enum: write");
  if (attach_keywords (w))
    return 1;
  DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "enum: close");

  wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
  DP_REQUIRE_MSG (r, "enum: reader");

  /* keywords: every index yields a tag, and that tag finds the SAME record
     the enumerator returned. A tag that enumerated but did not resolve is
     the failure the binding would turn into a KeyError. */
  size_t nk = wfm_reader_num_keywords (r);
  DP_REQUIRE_MSG (nk == 7, "enum: the seven keywords written are there");
  for (size_t i = 0; i < nk; i++)
    {
      const char *tag = wfm_reader_keyword_tag (r, i);
      DP_REQUIRE_MSG (tag && *tag, "keyword_tag is a non-empty tag");
      const wfm_keyword_t *by_index = wfm_reader_keyword (r, i);
      const wfm_keyword_t *by_tag   = wfm_reader_find_keyword (r, tag);
      DP_REQUIRE_MSG (by_index && by_tag,
                      "both the index and the tag resolve");
      DP_REQUIRE_MSG (by_index == by_tag,
                      "enumerating and looking up reach the same record");
    }
  /* Out of range is NULL for the RECORD accessor, which documents exactly
     that. The TAG accessor deliberately does not check: its doc comment says
     jm's dict loop only ever calls it for [0, num_keywords), so the bound is
     the caller's. Asserted as the contract reads rather than as one might
     wish it read -- calling the tag form out of range here would be the test
     performing the undefined behaviour it is meant to be describing. See F2.
   */
  DP_REQUIRE_MSG (wfm_reader_keyword (r, nk) == NULL,
                  "keyword past the end is NULL, as documented");

  /* header fields: the same three questions over the other map */
  size_t nh = wfm_reader_num_header_fields (r);
  DP_REQUIRE_MSG (nh > 0, "a BLUE capture exposes header fields");
  for (size_t i = 0; i < nh; i++)
    {
      const char *tag = wfm_reader_header_tag (r, i);
      DP_REQUIRE_MSG (tag && *tag, "header_tag is a non-empty tag");
      const wfm_keyword_t *by_index = wfm_reader_header_field (r, i);
      const wfm_keyword_t *by_tag   = wfm_reader_find_header_field (r, tag);
      DP_REQUIRE_MSG (by_index && by_tag,
                      "both the index and the tag resolve");
      DP_REQUIRE_MSG (by_index == by_tag,
                      "enumerating and looking up reach the same field");
    }
  DP_REQUIRE_MSG (wfm_reader_header_field (r, nh) == NULL,
                  "header_field past the end is NULL, as documented");

  /* the two maps are DISTINCT: a header field is not a keyword. Without
     this, one map aliased onto the other would satisfy everything above. */
  DP_REQUIRE_MSG (wfm_reader_find_keyword (r, wfm_reader_header_tag (r, 0))
                      == NULL,
                  "a header tag is not also a keyword");

  wfm_reader_destroy (r);
  remove (path);
  return 0;
}

/* ── the follow knobs, and read_follow_max_out ──────────────────────────
 *
 * get_follow_timeout_ms, get_follow_grace_ms and read_follow_max_out had
 * zero mentions; only the timeout SETTER was used. A setter tested without
 * its getter cannot tell a stored value from a discarded one.
 */
static int
test_the_follow_knobs (void)
{
  const char *path = "dp_rd_follow.blue";
  float _Complex x[N];
  make_signal (x, N);
  remove (path);
  wfm_writer_state_t *w = wfm_writer_create (path, 1e6, WFM_FT_BLUE, 3, 0, 0.0,
                                             0, 0.0, 0.0, false);
  DP_REQUIRE_MSG (w, "follow: writer");
  DP_REQUIRE_MSG (wfm_writer_write (w, x, N) == N, "follow: write");
  DP_REQUIRE_MSG (wfm_writer_close (w) == 0, "follow: close");

  wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
  DP_REQUIRE_MSG (r, "follow: reader");

  /* defaults are readable before anything is set */
  uint32_t t0 = wfm_reader_get_follow_timeout_ms (r);
  uint32_t g0 = wfm_reader_get_follow_grace_ms (r);
  wfm_reader_set_follow_timeout_ms (r, t0 + 137u);
  DP_REQUIRE_MSG (wfm_reader_get_follow_timeout_ms (r) == t0 + 137u,
                  "the follow timeout reads back what was set");
  DP_REQUIRE_MSG (wfm_reader_get_follow_grace_ms (r) == g0,
                  "and setting the timeout leaves the grace alone");
  wfm_reader_set_follow_grace_ms (r, g0 + 29u);
  DP_REQUIRE_MSG (wfm_reader_get_follow_grace_ms (r) == g0 + 29u,
                  "the grace reads back what was set");
  DP_REQUIRE_MSG (wfm_reader_get_follow_timeout_ms (r) == t0 + 137u,
                  "and the timeout is still what it was -- two knobs, "
                  "not one aliased pair");

  /* the capacity accessors are the identity, including at 0 */
  DP_REQUIRE_MSG (wfm_reader_read_max_out (r, 0) == 0,
                  "read_max_out (0) is 0");
  DP_REQUIRE_MSG (wfm_reader_read_max_out (r, 4096) == 4096,
                  "read_max_out is the identity");
  DP_REQUIRE_MSG (wfm_reader_read_follow_max_out (r, 0) == 0,
                  "read_follow_max_out (0) is 0");
  DP_REQUIRE_MSG (wfm_reader_read_follow_max_out (r, 4096) == 4096,
                  "read_follow_max_out is the identity");

  wfm_reader_destroy (r);
  remove (path);
  return 0;
}

int
main (void)
{
  if (roundtrip ("dp_reader.cf32", WFM_FT_RAW, 0, 1e6, 1e-6))
    return 1;
  if (roundtrip ("dp_reader.cf64", WFM_FT_RAW, 1, 1e6, 1e-9))
    return 1;
  if (roundtrip ("dp_reader.ci16", WFM_FT_RAW, 3, 1e6, 1e-3))
    return 1;
  if (roundtrip ("dp_reader.blue", WFM_FT_BLUE, 0, 2.4e6, 1e-6))
    return 1;
  if (roundtrip ("dp_reader.sigmf-data", WFM_FT_SIGMF, 0, 1e6, 1e-6))
    return 1;
  if (roundtrip ("dp_reader.csv", WFM_FT_CSV, 0, 1e6, 1e-6))
    return 1;
  if (roundtrip ("dp_reader_i16.csv", WFM_FT_CSV, 3, 1e6, 1e-3))
    return 1;
  if (test_blue_gate ())
    return 1;
  if (test_detached_header_entry ())
    return 1;
  if (test_blue_format_mode ())
    return 1;
  if (test_keyword_roundtrip ())
    return 1;
  if (test_keyword_absent_and_corrupt ())
    return 1;
  if (test_reset_rewinds_to_the_first_sample ())
    return 1;
  if (test_read_capacity ())
    return 1;
  if (test_ext_header_at_end_of_attached_file ())
    return 1;
  if (test_hcb_keyword_area ())
    return 1;
  if (test_fc_from_keywords ())
    return 1;
  if (test_fc_write_side ())
    return 1;
  if (test_trailing_bytes ())
    return 1;
  if (test_detects_by_content_not_extension ())
    return 1;
  if (test_sigmf_datetime_round_trips ())
    return 1;
  if (test_sigmf_datetime_every_spelling ())
    return 1;
  if (test_sigmf_pair_from_create ())
    return 1;
  if (test_t0_round_trips_through_blue ())
    return 1;
  if (test_fs_and_t0_provenance ())
    return 1;
  if (test_follow_resumes_after_catching_up ())
    return 1;
  if (test_read_never_consumes_a_partial_sample ())
    return 1;
  if (test_follow_ends_on_the_marker_not_on_silence ())
    return 1;
  if (test_follow_drains_before_honouring_a_stop ())
    return 1;
  if (test_follow_distinguishes_timeout_from_interrupted ())
    return 1;
  if (test_flush_makes_samples_observable ())
    return 1;
  if (test_scalar_round_trips_through_our_own_writer ())
    return 1;
  if (test_the_accessor_surface ())
    return 1;
  if (test_the_enumerators ())
    return 1;
  if (test_the_follow_knobs ())
    return 1;
  printf ("test_wfm_reader: all passed\n");
  return 0;
}
