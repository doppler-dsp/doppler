/*
 * test_iq_file_core.c — headerless IqFile reader round-trip.
 *
 * Writes a tiny interleaved-cf32 + interleaved-ci16 capture to a temp file,
 * opens each with iq_file_create(), and checks the decode matches (floats
 * exact; ci16 within one quantization step). Also exercises the headerless
 * contract: a missing file / NULL path / bad sample_type returns NULL, a read
 * past EOF returns 0 and zero-fills, and close() is idempotent.
 */
#include "iq_file/iq_file_core.h"

#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static inline int
_almost_eq (float a, float b, float tol)
{
  return fabsf (a - b) <= tol;
}

/* Write n interleaved cf32 (I,Q) pairs to path. */
static void
write_cf32 (const char *path, const float *iq, size_t n)
{
  FILE *f = fopen (path, "wb");
  fwrite (iq, sizeof (float), 2 * n, f);
  fclose (f);
}

/* Write n interleaved ci16 (I,Q) pairs to path (little-endian host). */
static void
write_ci16 (const char *path, const int16_t *iq, size_t n)
{
  FILE *f = fopen (path, "wb");
  fwrite (iq, sizeof (int16_t), 2 * n, f);
  fclose (f);
}

int
main (void)
{
  int  _fails = 0;
  char tmpl[] = "/tmp/iq_file_test_XXXXXX";
  int  tfd    = mkstemp (tmpl);
  CHECK (tfd >= 0);
  close (tfd);

  /* ── cf32: bit-exact round-trip ──────────────────────────────────────── */
  const float src[8] = { 1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.5f, 0.25f, -0.75f };
  write_cf32 (tmpl, src, 4);

  iq_file_state_t *r = iq_file_create (tmpl, 0 /*cf32*/, 0 /*le*/);
  CHECK (r != NULL);
  CHECK (iq_file_get_nsamples (r) == 4);

  float complex out[8];
  iq_file_read (r, 4, out);
  CHECK (iq_file_get_position (r) == 4);
  for (size_t i = 0; i < 4; i++)
    {
      CHECK (_almost_eq (crealf (out[i]), src[2 * i], 0.0f));
      CHECK (_almost_eq (cimagf (out[i]), src[2 * i + 1], 0.0f));
    }

  /* read past EOF: 0 samples, tail zero-filled */
  out[0] = 7.0f + 7.0f * I;
  iq_file_read (r, 2, out);
  CHECK (crealf (out[0]) == 0.0f && cimagf (out[0]) == 0.0f);

  /* close is idempotent; reads after close stay zero */
  iq_file_close (r);
  iq_file_close (r);
  out[0] = 9.0f + 9.0f * I;
  iq_file_read (r, 1, out);
  CHECK (crealf (out[0]) == 0.0f && cimagf (out[0]) == 0.0f);
  iq_file_destroy (r);

  /* ── ci16: rescale within one LSB ────────────────────────────────────── */
  const int16_t isrc[4] = { 32767, 0, -16384, 8192 };
  write_ci16 (tmpl, isrc, 2);
  iq_file_state_t *ri = iq_file_create (tmpl, 3 /*ci16*/, 0 /*le*/);
  CHECK (ri != NULL);
  CHECK (iq_file_get_nsamples (ri) == 2);
  float complex iout[2];
  iq_file_read (ri, 2, iout);
  CHECK (_almost_eq (crealf (iout[0]), 32767.0f / 32767.0f, 1.0f / 32767.0f));
  CHECK (_almost_eq (cimagf (iout[0]), 0.0f, 1.0f / 32767.0f));
  CHECK (_almost_eq (crealf (iout[1]), -16384.0f / 32767.0f, 1.0f / 32767.0f));

  /* reset rewinds to sample 0 */
  iq_file_reset (ri);
  CHECK (iq_file_get_position (ri) == 0);
  iq_file_destroy (ri);

  /* ── headerless contract: bad inputs return NULL ─────────────────────── */
  CHECK (iq_file_create (NULL, 0, 0) == NULL);               /* NULL path */
  CHECK (iq_file_create ("/no/such/file.iq", 0, 0) == NULL); /* missing */
  CHECK (iq_file_create (tmpl, 99, 0) == NULL);              /* bad type */

  unlink (tmpl);

  if (_fails)
    {
      fprintf (stderr, "test_iq_file_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_iq_file_core PASSED\n");
  return 0;
}
