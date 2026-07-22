/*
 * test_wfm_reader.c — round-trip wfm_writer → wfm_reader across every
 * container, plus container auto-detection and the BLUE-magic gate.
 */
#include "wfm/wfm_reader.h"
#include "wfm/wfm_writer.h"

#include <complex.h>
#include <math.h>
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

/* Write x through a writer of the given container, then read it back and check
   the samples match within tol and the metadata is recovered. */
static int
roundtrip (const char *path, int ft, int stype, double fs, double tol)
{
  float _Complex x[N], y[N];
  make_signal (x, N);

  FILE *fp = fopen (path, "wb");
  CHECK (fp, "open for write");
  wfm_writer_t *w = wfm_writer_open (fp, ft, stype, 0, fs, 0.0, N);
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

  wfm_reader_t *r = wfm_reader_open (path, stype, 0);
  CHECK (r, "reader open");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  CHECK (info.sample_type == stype, "sample_type recovered");
  if (ft == WFM_FT_BLUE || ft == WFM_FT_SIGMF)
    CHECK (fabs (info.fs - fs) < 1.0, "fs recovered from metadata");
  CHECK (info.num_samples == 0 || info.num_samples == N, "num_samples");

  size_t total = 0, n;
  while ((n = wfm_reader_read (r, y + total, N - total)) > 0)
    total += n;
  wfm_reader_close (r);
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
  wfm_writer_t *w = wfm_writer_open (fp, WFM_FT_RAW, 0, 0, 1e6, 0.0, 8);
  wfm_writer_write (w, x, 8);
  wfm_writer_close (w);
  fclose (fp);
  wfm_reader_t *r = wfm_reader_open (raw, 0, 0);
  CHECK (r, "raw opens");
  wfm_reader_info_t info;
  wfm_reader_info (r, &info);
  CHECK (info.file_type == WFM_FT_RAW, "raw not mis-detected as BLUE");
  wfm_reader_close (r);

  /* a .det whose .hdr lacks the BLUE magic must be rejected. */
  FILE *hf = fopen ("dp_reader_bad.hdr", "wb");
  CHECK (hf, "open bad hdr");
  char junk[512] = "NOTBLUE";
  fwrite (junk, 1, 512, hf);
  fclose (hf);
  FILE *df = fopen ("dp_reader_bad.det", "wb");
  fwrite (x, sizeof x, 1, df);
  fclose (df);
  CHECK (wfm_reader_open ("dp_reader_bad.det", 0, 0) == NULL,
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

      wfm_reader_t *r = wfm_reader_open (HDR[i], 0, 0);
      CHECK (r != NULL, "open detached capture by its header");
      wfm_reader_info_t info;
      wfm_reader_info (r, &info);
      CHECK (info.file_type == WFM_FT_BLUE, "detached header detects BLUE");
      CHECK (info.num_samples == N, "detached num_samples from data_size");
      size_t got = wfm_reader_read (r, y, N);
      wfm_reader_close (r);
      /* the whole payload -- NOT the 512-byte header as 64 samples */
      CHECK (got == N, "detached header yields the full payload");
      for (size_t k = 0; k < N; k++)
        CHECK (cabsf (y[k] - x[k]) < 1e-6f, "detached payload is exact");
    }
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
  printf ("test_wfm_reader: all passed\n");
  return 0;
}
