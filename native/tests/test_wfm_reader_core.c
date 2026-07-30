/*
 * test_wfm_reader.c — round-trip wfm_writer → wfm_reader across every
 * file type, plus file type auto-detection and the BLUE-magic gate.
 */
#include "wfm/wfm_keywords.h"
#include "wfm_reader/wfm_reader_core.h"
#include "wfm_writer/wfm_writer_core.h"

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 1000
#define CHECK(c, m)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(c))                                                               \
        {                                                                     \
          fprintf (stderr, "FAIL: %s\n", m);                                  \
          return 1;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

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
  CHECK (fp, "open for write");
  wfm_writer_state_t *w = wfm_writer_open (fp, ft, stype, 0, fs, 0.0, N);
  CHECK (w, "writer open");
  CHECK (wfm_writer_write (w, x, N) == N, "writer wrote N");
  wfm_writer_close (w);
  fclose (fp);

  /* SigMF needs its .sigmf-meta sidecar written separately. */
  if (ft == WFM_FT_SIGMF)
    {
      char *meta = wfm_sigmf_meta_json (stype, 0, fs, 0.0, NULL, 0);
      CHECK (meta, "sigmf meta json");
      char mpath[1024];
      snprintf (mpath, sizeof mpath, "%.*s.sigmf-meta",
                (int)(strlen (path) - 11), path); /* strip .sigmf-data */
      FILE *mf = fopen (mpath, "w");
      CHECK (mf, "open meta");
      fputs (meta, mf);
      fclose (mf);
      free (meta);
    }

  wfm_reader_state_t *r = wfm_reader_create (path, stype, 0);
  CHECK (r, "reader open");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  CHECK (info.sample_type == stype, "sample_type recovered");
  if (ft == WFM_FT_BLUE || ft == WFM_FT_SIGMF)
    CHECK (fabs (info.fs - fs) < 1.0, "fs recovered from metadata");
  /* Exact for every file type now: CSV used to be the one that could
     only say 0, which reads as an empty capture. */
  CHECK (info.num_samples == N, "num_samples");

  size_t total = 0, n;
  while ((n = wfm_reader_read (r, N - total, y + total, N - total)) > 0)
    total += n;
  wfm_reader_destroy (r);
  CHECK (total == N, "read back N samples");

  double maxerr = 0.0;
  for (size_t i = 0; i < N; i++)
    {
      double e = cabs ((double _Complex)x[i] - (double _Complex)y[i]);
      if (e > maxerr)
        maxerr = e;
    }
  CHECK (maxerr < tol, "samples round-trip within tol");
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
  CHECK (fp, "open raw");
  wfm_writer_state_t *w = wfm_writer_open (fp, WFM_FT_RAW, 0, 0, 1e6, 0.0, 8);
  wfm_writer_write (w, x, 8);
  wfm_writer_close (w);
  fclose (fp);
  wfm_reader_state_t *r = wfm_reader_create (raw, 0, 0);
  CHECK (r, "raw opens");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  CHECK (info.file_type == WFM_FT_RAW, "raw not mis-detected as BLUE");
  wfm_reader_destroy (r);

  /* a .det whose .hdr lacks the BLUE magic must be rejected. */
  FILE *hf = fopen ("dp_reader_bad.hdr", "wb");
  CHECK (hf, "open bad hdr");
  char junk[512] = "NOTBLUE";
  fwrite (junk, 1, 512, hf);
  fclose (hf);
  FILE *df = fopen ("dp_reader_bad.det", "wb");
  fwrite (x, sizeof x, 1, df);
  fclose (df);
  CHECK (wfm_reader_create ("dp_reader_bad.det", 0, 0) == NULL,
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
  CHECK (df != NULL, "open .det");
  CHECK (fwrite (x, sizeof x[0], N, df) == N, "write .det");
  fclose (df);

  for (size_t i = 0; i < sizeof HDR / sizeof *HDR; i++)
    {
      FILE *hf = fopen (HDR[i], "wb");
      CHECK (hf != NULL, "open detached header");
      /* data_start = 0, detached = 1 -> payload is the collocated .det */
      CHECK (wfm_blue_write_hcb (hf, 0, 0, 2.4e6, 0.0, 0.0, N, 1) == 0,
             "write detached HCB");
      fclose (hf);

      wfm_reader_state_t *r = wfm_reader_create (HDR[i], 0, 0);
      CHECK (r != NULL, "open detached capture by its header");
      wfm_reader_info_t info;
      wfm_reader_info (r, &info);
      CHECK (info.file_type == WFM_FT_BLUE, "detached header detects BLUE");
      CHECK (info.num_samples == N, "detached num_samples from data_size");
      size_t got = wfm_reader_read (r, N, y, N);
      wfm_reader_destroy (r);
      /* the whole payload -- NOT the 512-byte header as 64 samples */
      CHECK (got == N, "detached header yields the full payload");
      for (size_t k = 0; k < N; k++)
        CHECK (cabsf (y[k] - x[k]) < 1e-6f, "detached payload is exact");
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
  CHECK (fp != NULL, "open mode file");
  CHECK (wfm_blue_write_hcb (fp, 0, 0, 1e6, 0.0, 512.0, n, 0) == 0,
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
      CHECK (fwrite (&re, sizeof re, 1, fp) == 1, "write I");
      if (ncomp == 2)
        CHECK (fwrite (&im, sizeof im, 1, fp) == 1, "write Q");
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
  CHECK (r != NULL, "scalar BLUE opens");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  CHECK (info.mode == WFM_MODE_SCALAR, "mode is scalar");
  CHECK (info.num_samples == N, "scalar num_samples is not halved");
  size_t got = wfm_reader_read (r, N, y, N);
  wfm_reader_destroy (r);
  CHECK (got == N, "scalar yields every sample, not half");
  for (size_t k = 0; k < N; k++)
    {
      CHECK (fabsf (crealf (y[k]) - crealf (x[k])) < 1e-6f, "scalar I exact");
      CHECK (cimagf (y[k]) == 0.0f, "scalar Q is exactly zero");
    }

  /* complex: unchanged, and reports its mode */
  if (write_blue_mode ("dp_mode_c.blue", 'C', 2, x, N))
    return 1;
  r = wfm_reader_create ("dp_mode_c.blue", 0, 0);
  CHECK (r != NULL, "complex BLUE opens");
  wfm_reader_info (r, &info);
  CHECK (info.mode == WFM_MODE_COMPLEX, "mode is complex");
  CHECK (info.num_samples == N, "complex num_samples");
  got = wfm_reader_read (r, N, y, N);
  wfm_reader_destroy (r);
  CHECK (got == N, "complex yields every sample");
  for (size_t k = 0; k < N; k++)
    CHECK (cabsf (y[k] - x[k]) < 1e-6f, "complex round-trips");

  /* every unsupported mode designator is refused, not guessed at */
  static const char BAD[] = { 'V', 'Q', 'M', 'T', 'X', '1', 'c', 's' };
  for (size_t i = 0; i < sizeof BAD; i++)
    {
      if (write_blue_mode ("dp_mode_bad.blue", BAD[i], 2, x, 8))
        return 1;
      CHECK (wfm_reader_create ("dp_mode_bad.blue", 0, 0) == NULL,
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
  CHECK (wfm_writer_add_keyword (w, "COMMENT", 'A', KW_STR, strlen (KW_STR))
             == 0,
         "add A");
  CHECK (wfm_writer_add_keyword (w, "F_C", 'D', &KW_D, 1) == 0, "add D");
  CHECK (wfm_writer_add_keyword (w, "GAINS", 'F', KW_F, 3) == 0, "add F[]");
  CHECK (wfm_writer_add_keyword (w, "OFFSET", 'L', &KW_L, 1) == 0, "add L");
  CHECK (wfm_writer_add_keyword (w, "TRIM", 'I', &KW_I, 1) == 0, "add I");
  CHECK (wfm_writer_add_keyword (w, "FLAG", 'B', &KW_B, 1) == 0, "add B");
  CHECK (wfm_writer_add_keyword (w, "TICKS", 'X', &KW_X, 1) == 0, "add X");
  return 0;
}

/* Check the keywords attach_keywords() wrote all came back intact. */
static int
check_keywords (wfm_reader_state_t *r)
{
  CHECK (wfm_reader_num_keywords (r) == 7, "all seven keywords recovered");
  const wfm_keyword_t *k = wfm_reader_find_keyword (r, "COMMENT");
  CHECK (k && k->type == 'A', "COMMENT is a string");
  CHECK (k->count == strlen (KW_STR), "string length (no NUL on the wire)");
  CHECK (memcmp (k->value, KW_STR, k->count) == 0, "string value");

  k = wfm_reader_find_keyword (r, "F_C");
  double d;
  CHECK (k && k->type == 'D' && k->count == 1, "F_C is a scalar double");
  memcpy (&d, k->value, 8);
  CHECK (d == KW_D, "double value");

  k = wfm_reader_find_keyword (r, "GAINS");
  float f[3];
  CHECK (k && k->type == 'F' && k->count == 3, "GAINS is a 3-element float");
  memcpy (f, k->value, sizeof f);
  CHECK (f[0] == KW_F[0] && f[1] == KW_F[1] && f[2] == KW_F[2],
         "array order preserved");

  k = wfm_reader_find_keyword (r, "OFFSET");
  int32_t l;
  CHECK (k && k->count == 1, "OFFSET present");
  memcpy (&l, k->value, 4);
  CHECK (l == KW_L, "negative int32 value");

  k = wfm_reader_find_keyword (r, "TICKS");
  int64_t x;
  CHECK (k && k->count == 1, "TICKS present");
  memcpy (&x, k->value, 8);
  CHECK (x == KW_X, "int64 value");

  CHECK (wfm_reader_find_keyword (r, "NOPE") == NULL, "absent tag is NULL");
  /* file order is preserved, so index 0 is the first one written */
  CHECK (strcmp (wfm_reader_keyword (r, 0)->tag, "COMMENT") == 0,
         "keywords come back in file order");
  CHECK (wfm_reader_keyword (r, 7) == NULL, "out-of-range index is NULL");
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
      CHECK (fp != NULL, "open blue");
      wfm_writer_state_t *w
          = wfm_writer_open (fp, WFM_FT_BLUE, 0, be, 2.4e6, 0.0, N);
      CHECK (w != NULL, "writer open");
      if (attach_keywords (w))
        return 1;
      CHECK (wfm_writer_write (w, x, N) == N, "write samples");
      CHECK (wfm_writer_close (w) == 0, "close writes the extended header");
      fclose (fp);

      wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
      CHECK (r != NULL, "reopen");
      if (check_keywords (r))
        return 1;
      /* Draining to EOF must stop at the declared payload. The extended
         header sits AFTER the data, so a reader that just reads until fread
         runs dry would hand the caller keyword bytes as IQ -- silently, and
         only for files that carry metadata. */
      size_t total = 0, got;
      while ((got = wfm_reader_read (r, N - total, y + total, N - total)) > 0)
        total += got;
      CHECK (total == N, "drains to exactly the declared payload");
      CHECK (wfm_reader_read (r, N, y, N) == 0, "and stays at end of data");
      for (size_t i = 0; i < N; i++)
        CHECK (cabsf (y[i] - x[i]) < 1e-6f, "samples unaffected");
      wfm_reader_destroy (r);
    }

  /* detached: keywords are in the .hdr, samples in the .det */
  FILE *hf = fopen ("dp_kw_det.hdr", "wb");
  CHECK (hf != NULL, "open detached header");
  CHECK (wfm_blue_write_hcb (hf, 0, 0, 2.4e6, 0.0, 0.0, N, 1) == 0,
         "write detached HCB");
  fclose (hf);
  /* re-open the header as a BLUE writer target purely to attach keywords:
     the payload goes to the .det, so this writer emits no samples. */
  hf = fopen ("dp_kw_det.hdr", "r+b");
  CHECK (hf != NULL, "reopen header");
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
    CHECK (fwrite (kwblob, 1, off, hf) == off, "append extended header");
    int32_t  blocks = 1, size = (int32_t)off;
    uint8_t  b[8];
    uint8_t *p = b;
    memcpy (p, &blocks, 4);
    memcpy (p + 4, &size, 4);
    fseek (hf, 24, SEEK_SET);
    CHECK (fwrite (b, 1, 8, hf) == 8, "patch ext_start/ext_size");
    fclose (hf);
  }
  FILE *df = fopen ("dp_kw_det.det", "wb");
  CHECK (df != NULL, "open .det");
  CHECK (fwrite (x, sizeof x[0], N, df) == N, "write payload");
  fclose (df);

  /* both entry points must find the keywords -- they live in the header file
     either way, which is the whole point of the detached split. */
  static const char *const ENTRY[] = { "dp_kw_det.hdr", "dp_kw_det.det" };
  for (size_t i = 0; i < 2; i++)
    {
      wfm_reader_state_t *r = wfm_reader_create (ENTRY[i], 0, 0);
      CHECK (r != NULL, "open detached capture");
      CHECK (wfm_reader_num_keywords (r) == 2,
             "detached keywords come from the HEADER file");
      const wfm_keyword_t *k = wfm_reader_find_keyword (r, "F_C");
      double               d;
      CHECK (k != NULL, "F_C present");
      memcpy (&d, k->value, 8);
      CHECK (d == KW_D, "detached keyword value");
      CHECK (wfm_reader_read (r, N, y, N) == N, "detached samples still read");
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
  CHECK (fp != NULL, "open");
  wfm_writer_state_t *w
      = wfm_writer_open (fp, WFM_FT_BLUE, 0, 0, 2.4e6, 0.0, N);
  wfm_writer_write (w, x, N);
  wfm_writer_close (w);
  fclose (fp);
  wfm_reader_state_t *r = wfm_reader_create ("dp_kw_none.blue", 0, 0);
  CHECK (r != NULL, "opens");
  CHECK (wfm_reader_num_keywords (r) == 0, "no extended header, no keywords");
  CHECK (wfm_reader_keyword (r, 0) == NULL, "index 0 is NULL");
  wfm_reader_destroy (r);

  /* claim an extended header that runs off the end of the file */
  fp = fopen ("dp_kw_bad.blue", "wb");
  w  = wfm_writer_open (fp, WFM_FT_BLUE, 0, 0, 2.4e6, 0.0, N);
  attach_keywords (w);
  wfm_writer_write (w, x, N);
  wfm_writer_close (w);
  fclose (fp);
  fp = fopen ("dp_kw_bad.blue", "r+b");
  CHECK (fp != NULL, "reopen to corrupt");
  int32_t huge = 1 << 20; /* ext_size far past EOF */
  fseek (fp, 28, SEEK_SET);
  fwrite (&huge, 4, 1, fp);
  fclose (fp);
  r = fopen ("dp_kw_bad.blue", "rb")
          ? wfm_reader_create ("dp_kw_bad.blue", 0, 0)
          : NULL;
  CHECK (r != NULL, "a bad keyword region does not fail the open");
  CHECK (wfm_reader_read (r, N, y, N) == N,
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
      CHECK (fp != NULL, "open");
      wfm_writer_state_t *w = wfm_writer_open (fp, FT[i], 0, 0, 2.4e6, 0.0, N);
      CHECK (w != NULL, "writer");
      if (FT[i] == WFM_FT_BLUE && attach_keywords (w))
        return 1;
      CHECK (wfm_writer_write (w, x, N) == N, "write");
      CHECK (wfm_writer_close (w) == 0, "close");
      fclose (fp);

      wfm_reader_state_t *r = wfm_reader_create (PATHS[i], 0, 0);
      CHECK (r != NULL, "open for reset");
      CHECK (wfm_reader_read (r, N, a, N) == N, "first pass");
      wfm_reader_reset (r);
      CHECK (wfm_reader_read (r, N, b, N) == N, "second pass reads N again");
      CHECK (wfm_reader_read (r, 1, b + N - 1, 1) == 0,
             "and stops at the end");
      wfm_reader_destroy (r);
      for (size_t k = 0; k + 1 < N; k++)
        CHECK (a[k] == b[k], "reset replays the identical samples");
    }

  /* detached: payload is byte 0 of the .det, header is elsewhere */
  wfm_reader_state_t *r = wfm_reader_create ("dp_kw_det.hdr", 0, 0);
  CHECK (r != NULL, "open detached");
  CHECK (wfm_reader_read (r, N, a, N) == N, "detached first pass");
  wfm_reader_reset (r);
  CHECK (wfm_reader_read (r, N, b, N) == N, "detached second pass");
  CHECK (wfm_reader_num_keywords (r) == 2, "keywords survive a reset");
  wfm_reader_destroy (r);
  for (size_t k = 0; k < N; k++)
    CHECK (a[k] == b[k], "detached reset replays identically");
  return 0;
}

/* pass_capacity: emission stops at max_out (jm gh-138).
   wfm_reader_read_max_out() returns 0 ("no fixed bound"), so before this the
   kernel's only limit was the caller's own count argument -- a caller with a
   buffer smaller than the count it asked for had no way to say so. */
static int
test_read_capacity (void)
{
  int                 _fails = 0;
  wfm_reader_state_t *r      = wfm_reader_create ("dp_reader.blue", 0, 0);
  float _Complex y[16];
  CHECK (r, "reopen the BLUE capture");
  for (size_t i = 0; i < 16; i++)
    y[i] = 42.0f + 42.0f * I;

  CHECK (wfm_reader_read (r, 16, y, 3) == 3, "read stops at max_out");
  for (size_t i = 3; i < 16; i++)
    CHECK (y[i] == 42.0f + 42.0f * I, "tail untouched");

  /* Zero capacity reads nothing. */
  CHECK (wfm_reader_read (r, 16, y, 0) == 0, "zero capacity reads nothing");
  wfm_reader_destroy (r);
  return _fails;
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
  int    _fails = 0;
  size_t n      = 8;
  double sr     = 2.048e6;

  FILE *fp = fopen ("dp_extend.blue", "wb");
  CHECK (fp, "open for write");
  wfm_writer_state_t *w = wfm_writer_open (fp, WFM_FT_BLUE, 0, 0, sr, 0.0, n);
  CHECK (w, "writer open");
  CHECK (wfm_writer_add_keyword (w, "SRATE", 'D', &sr, 1) == 0, "typed kw");
  float _Complex xs[8];
  for (size_t i = 0; i < n; i++)
    xs[i] = (float)(i + 1) + 0.0f * I;
  CHECK (wfm_writer_write (w, xs, n) == (int)n, "write samples");
  CHECK (wfm_writer_close (w) == 0, "close");
  fclose (fp);

  /* The extended header must sit AFTER the data, on a 512-byte boundary. */
  wfm_reader_state_t *r = wfm_reader_create ("dp_extend.blue", 0, 0);
  CHECK (r, "reader open");
  const wfm_keyword_t *es = wfm_reader_find_header_field (r, "ext_start");
  const wfm_keyword_t *ds = wfm_reader_find_header_field (r, "data_start");
  CHECK (es && ds, "ext_start/data_start present in the header dict");
  if (es && ds)
    {
      int32_t blocks;
      double  dstart;
      memcpy (&blocks, es->value, sizeof blocks);
      memcpy (&dstart, ds->value, sizeof dstart);
      CHECK ((long)blocks * 512L > (long)dstart,
             "ext header follows the data");
    }

  /* The keyword decodes, and its TYPE survives (a double, not a string). */
  const wfm_keyword_t *kw = wfm_reader_find_keyword (r, "SRATE");
  CHECK (kw && kw->type == 'D' && kw->count == 1, "SRATE is a 1-element D");
  if (kw)
    {
      double got;
      memcpy (&got, kw->value, sizeof got);
      CHECK (fabs (got - sr) < 1.0, "SRATE value round-trips");
    }

  /* And the payload stops at data_size: the extended header is NOT samples. */
  float _Complex y[16];
  size_t got = wfm_reader_read (r, 16, y, 16);
  CHECK (got == n, "read returns exactly the declared sample count");
  for (size_t i = 0; i < got; i++)
    CHECK (crealf (y[i]) == (float)(i + 1), "sample values intact");
  CHECK (wfm_reader_read (r, 16, y, 16) == 0, "and stops at the payload end");
  wfm_reader_destroy (r);
  return _fails;
}

/* The HCB's own keyword area (keylength@160, keywords@164): where X-Midas
   commonly puts short metadata, and what doppler used to drop on the floor. */
static int
test_hcb_keyword_area (void)
{
  int   _fails = 0;
  FILE *fp     = fopen ("dp_hcbkw.blue", "wb");
  CHECK (fp, "open for write");
  wfm_writer_state_t *w = wfm_writer_open (fp, WFM_FT_BLUE, 0, 0, 1e6, 0.0, 4);
  CHECK (w, "writer open");
  /* Only the six standard main-header keywords (BLUE 1.1 3.4.1) go to the
     HCB area, and only as ASCII. A user keyword -- even an ASCII one -- goes
     to the extended header, because 3.4 reserves the 92-byte area and lets
     other systems delete user keywords found there. */
  CHECK (wfm_writer_add_keyword (w, "VER", 'A', "1.1", 3) == 0, "std kw");
  CHECK (wfm_writer_add_keyword (w, "NAME", 'A', "hello", 5) == 0, "user kw");
  double sr = 1e6;
  CHECK (wfm_writer_add_keyword (w, "SRATE", 'D', &sr, 1) == 0, "typed kw");
  float _Complex xs[4] = { 1, 2, 3, 4 };
  CHECK (wfm_writer_write (w, xs, 4) == 4, "write");
  CHECK (wfm_writer_close (w) == 0, "close");
  fclose (fp);

  wfm_reader_state_t  *r  = wfm_reader_create ("dp_hcbkw.blue", 0, 0);
  const wfm_keyword_t *kl = wfm_reader_find_header_field (r, "keylength");
  CHECK (r && kl, "reader open, keylength present");
  if (kl)
    {
      int32_t v;
      memcpy (&v, kl->value, sizeof v);
      CHECK (v > 0, "keylength patched into the HCB");
    }
  /* Both sources merge: the caller cannot tell which block a key came from. */
  const wfm_keyword_t *v = wfm_reader_find_keyword (r, "VER");
  const wfm_keyword_t *a = wfm_reader_find_keyword (r, "NAME");
  const wfm_keyword_t *d = wfm_reader_find_keyword (r, "SRATE");
  CHECK (v && v->type == 'A', "HCB-area keyword decoded");
  CHECK (a && a->type == 'A', "user ASCII keyword decoded (ext header)");
  CHECK (d && d->type == 'D', "extended-header keyword decoded, type intact");
  wfm_reader_destroy (r);
  return _fails;
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
  CHECK (fc_from_pair ("FREQ=2400000000", &fc, &src) == 0, "FREQ capture");
  CHECK (fc == 2.4e9 && src == WFM_FC_FREQ, "FREQ resolves");
  CHECK (fc_from_pair ("RF_FREQ=1.5e9", &fc, &src) == 0, "RF_FREQ capture");
  CHECK (fc == 1.5e9 && src == WFM_FC_RF_FREQ, "RF_FREQ resolves");
  CHECK (fc_from_pair ("CENTER_FREQ=70e6", &fc, &src) == 0, "CENTER capture");
  CHECK (fc == 70e6 && src == WFM_FC_CENTER_FREQ, "CENTER_FREQ resolves");
  CHECK (fc_from_pair ("F_C=-3", &fc, &src) == 0, "F_C capture");
  CHECK (fc == -3.0 && src == WFM_FC_F_C, "F_C resolves");

  /* An explicit zero is a READING, and must not read as "not found" -- that
     distinction is the entire reason fc_source exists. */
  CHECK (fc_from_pair ("FREQ=0", &fc, &src) == 0, "baseband capture");
  CHECK (fc == 0.0 && src == WFM_FC_FREQ, "declared baseband");

  /* Nothing to read, and nothing invented. */
  CHECK (fc_from_pair ("COMMENT=hi", &fc, &src) == 0, "no-freq capture");
  CHECK (fc == 0.0 && src == WFM_FC_NONE, "absent stays absent");

  /* A value we cannot interpret is left alone rather than guessed at: "2.4
     GHz" parsed as a bare number would be wrong by a factor of a billion. */
  CHECK (fc_from_pair ("FREQ=2.4 GHz", &fc, &src) == 0, "united capture");
  CHECK (fc == 0.0 && src == WFM_FC_NONE, "a units suffix is not a number");
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
  CHECK (fp, "open for write");
  wfm_writer_state_t *w = wfm_writer_open (fp, WFM_FT_BLUE, 0, 0, 1e6, fc, 4);
  CHECK (w, "writer open");
  /* A standard HCB keyword makes close() rewrite the whole 92-byte area --
     the path on which a frequency written at open could be lost. */
  CHECK (wfm_writer_add_keyword (w, "VER", 'A', "1.1", 3) == 0, "std kw");
  CHECK (wfm_writer_write (w, xs, 4) == 4, "write");
  CHECK (wfm_writer_close (w) == 0, "close");
  fclose (fp);

  wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
  CHECK (r, "reader open");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  /* Exactly, not approximately: the typed extended-header copy is preferred
     precisely because it did not round-trip through a decimal string. */
  CHECK (info.fc == fc, "fc survives to full precision");
  CHECK (info.fc_source == WFM_FC_FREQ, "and knows where it came from");
  CHECK (wfm_reader_find_keyword (r, "VER") != NULL, "VER survived too");
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
  CHECK (fp, "open for write");
  CHECK (fwrite (raw, 1, sizeof raw, fp) == sizeof raw, "write raw");
  fclose (fp);

  wfm_reader_info_t info;
  /* Right hint: the file divides exactly. */
  wfm_reader_state_t *r = wfm_reader_create (path, 4, 0); /* ci8 */
  CHECK (r, "reader open ci8");
  wfm_reader_info (r, &info);
  CHECK (info.trailing_bytes == 0 && info.num_samples == 5, "ci8 divides");
  wfm_reader_destroy (r);

  /* Wrong hint: 10 bytes is one cf32 sample and two bytes nobody can use.
     Nothing fails -- a headerless file type cannot check a hint -- so the
     remainder is the only thing that says so. */
  r = wfm_reader_create (path, 0, 0); /* cf32 */
  CHECK (r, "reader open cf32");
  wfm_reader_info (r, &info);
  CHECK (info.trailing_bytes == 2, "wrong stride leaves a remainder");
  CHECK (info.num_samples == 1, "and only whole samples are counted");
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
  CHECK (fp, "open csv");
  fputs ("1.0,2.0\n3.0,4.0\n5.0,6.0\n", fp);
  fclose (fp);
  wfm_reader_state_t *r = wfm_reader_create (csv, 0, 0);
  CHECK (r, "reader open renamed csv");
  wfm_reader_info (r, &info);
  CHECK (info.file_type == WFM_FT_CSV, "text I,Q is CSV whatever it is named");
  CHECK (info.num_samples == 3, "and its length is counted");
  float _Complex y[4];
  CHECK (wfm_reader_read (r, 4, y, 4) == 3, "reads as CSV");
  CHECK (crealf (y[0]) == 1.0f && cimagf (y[2]) == 6.0f, "values intact");
  wfm_reader_destroy (r);

  /* And the reverse: the BLUE magic outranks a .csv name. */
  CHECK (write_hcb_only_capture ("dp_reader_named.csv", "COMMENT=x") == 0,
         "write blue as .csv");
  r = wfm_reader_create ("dp_reader_named.csv", 0, 0);
  CHECK (r, "reader open misnamed blue");
  wfm_reader_info (r, &info);
  CHECK (info.file_type == WFM_FT_BLUE, "the magic wins over the name");
  CHECK (info.num_samples == 4, "and the payload is located correctly");
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
  wfm_writer_state_t *w
      = wfm_writer_create (path, WFM_FT_SIGMF, 3, 0, 2e6, 1.2e9, 4, 0.0);
  CHECK (w, "writer create");
  CHECK (wfm_writer_write (w, xs, 4) == 4, "write");
  CHECK (wfm_writer_close (w) == 0, "close");

  wfm_reader_state_t *r = wfm_reader_create (path, 0, 0);
  CHECK (r, "reader open — the sidecar must exist");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  CHECK (info.file_type == WFM_FT_SIGMF, "detected as SigMF");
  CHECK (info.sample_type == 3, "ci16 recovered from the sidecar");
  CHECK (info.fs == 2e6 && info.fc == 1.2e9, "fs/fc recovered");
  CHECK (info.fc_source == WFM_FC_SIGMF, "and attributed to the sidecar");
  wfm_reader_destroy (r);

  /* Both halves are found by NAME, so the name is part of the format. The
     control proves the refusal is about the extension and not about the path
     being unwritable -- otherwise this pair of checks would pass vacuously. */
  wfm_writer_state_t *ok = wfm_writer_create ("dp_reader_pair.bin", WFM_FT_RAW,
                                              3, 0, 2e6, 0.0, 4, 0.0);
  CHECK (ok != NULL, "the same path is writable as raw");
  CHECK (wfm_writer_close (ok) == 0, "control close");
  CHECK (wfm_writer_create ("dp_reader_pair.bin", WFM_FT_SIGMF, 3, 0, 2e6, 0.0,
                            4, 0.0)
             == NULL,
         "a SigMF path must end in .sigmf-data");
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
  if (test_sigmf_pair_from_create ())
    return 1;
  printf ("test_wfm_reader: all passed\n");
  return 0;
}
