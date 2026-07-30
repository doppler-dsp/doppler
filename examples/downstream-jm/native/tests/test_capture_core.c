/**
 * @file test_capture_core.c
 * @brief C-level tests for the façade — no Python in the loop.
 *
 * The generated stub could not run: `capture_create` needs a real path, so it
 * skipped itself. It is replaced here with tests that build their captures
 * using doppler's own writer, which keeps the fixture honest (the same code
 * path a real capture came from) and needs no checked-in binary blobs.
 *
 * What is tested is this project's contribution, not doppler's: that the two
 * constructors share one core, and that provenance is reported truthfully.
 */
#include "capture/capture_core.h"

#include <complex.h>
#include <stdio.h>

#include "wfm_writer/wfm_writer_core.h"

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

#define NUM_SAMPLES 1024
#define FS 2400000.0
#define FC 1200000000.0
#define ST_CI16 3
#define ENDIAN_LE 0

/**
 * @brief Write a fixture capture with doppler's writer.
 *
 * @param path file to create.
 * @param ft   ::wfm_filetype_t — WFM_FT_RAW or WFM_FT_BLUE here.
 * @return 0 on success, non-zero otherwise.
 */
static int
write_capture (const char *path, wfm_filetype_t ft)
{
  float complex       buf[NUM_SAMPLES];
  wfm_writer_state_t *w;
  size_t              i;
  int                 rc;

  /* A deterministic ramp well inside full scale: nothing clips, so no
     quantisation argument is needed, and every sample is distinct so a stride
     error cannot hide behind repeated values. */
  for (i = 0; i < NUM_SAMPLES; i++)
    buf[i] = (float)(i % 64) / 128.0f + I * (float)(i % 32) / 128.0f;

  w = wfm_writer_create (path, (int)ft, ST_CI16, ENDIAN_LE, FS, FC, 0, 0.0);
  if (w == NULL)
    return -1;

  rc = (wfm_writer_write (w, buf, NUM_SAMPLES) == NUM_SAMPLES) ? 0 : -1;
  if (wfm_writer_close (w) != 0)
    rc = -1;
  return rc;
}

int
main (void)
{
  int              _fails = 0;
  const char      *blue   = "test_capture_fixture.blue";
  const char      *raw    = "test_capture_fixture.raw";
  capture_state_t *obj;
  float complex    out[NUM_SAMPLES];

  if (write_capture (blue, WFM_FT_BLUE) != 0
      || write_capture (raw, WFM_FT_RAW) != 0)
    {
      fprintf (stderr, "test_capture_core: could not write fixtures\n");
      return 1;
    }

  /* ── A self-describing capture: the file answers everything. ─────────── */
  obj = capture_create (blue);
  CHECK (obj != NULL);
  if (obj != NULL)
    {
      CHECK (capture_get_fs (obj) == FS);
      CHECK (capture_get_fc (obj) == FC);
      CHECK (capture_get_num_samples (obj) == NUM_SAMPLES);
      CHECK (capture_get_metadata_source (obj) == CAPTURE_META_FILE);
      CHECK (capture_read (obj, NUM_SAMPLES, out, NUM_SAMPLES) == NUM_SAMPLES);

      /* reset rewinds the cursor and leaves the metadata alone. */
      capture_reset (obj);
      CHECK (capture_read (obj, NUM_SAMPLES, out, NUM_SAMPLES) == NUM_SAMPLES);
      CHECK (capture_get_fs (obj) == FS);

      capture_destroy (obj);
    }

  /* ── The same bytes through the DEFAULT ctor: nothing to read. ───────── */
  obj = capture_create (raw);
  CHECK (obj != NULL);
  if (obj != NULL)
    {
      CHECK (capture_get_fs (obj) == 0.0);
      CHECK (capture_get_metadata_source (obj) == CAPTURE_META_NONE);
      /* Read at the cf32 fallback stride, a ci16 file decodes to half the
         samples — silently, with no error anywhere. That is the failure the
         view exists to prevent, pinned here so it cannot quietly change. */
      CHECK (capture_get_num_samples (obj) == NUM_SAMPLES / 2);
      capture_destroy (obj);
    }

  /* ── ...and through the VIEW's ctor: told the truth, it gets it right. ─ */
  obj = capture_open_raw (raw, ST_CI16, ENDIAN_LE, FS, FC);
  CHECK (obj != NULL);
  if (obj != NULL)
    {
      CHECK (capture_get_fs (obj) == FS);
      CHECK (capture_get_fc (obj) == FC);
      CHECK (capture_get_num_samples (obj) == NUM_SAMPLES);
      CHECK (capture_get_metadata_source (obj) == CAPTURE_META_SUPPLIED);
      CHECK (capture_read (obj, NUM_SAMPLES, out, NUM_SAMPLES) == NUM_SAMPLES);
      capture_destroy (obj);
    }

  /* ── A path that does not exist is refused, not papered over. ────────── */
  obj = capture_create ("no-such-capture.blue");
  CHECK (obj == NULL);

  /* destroy(NULL) is a documented no-op — it must not crash. */
  capture_destroy (NULL);

  remove (blue);
  remove (raw);

  if (_fails)
    {
      fprintf (stderr, "test_capture_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_capture_core PASSED\n");
  return 0;
}
