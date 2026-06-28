#include "dp_state_test.h"
#include "specan/specan_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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
_almost_eq (double a, double b, double tol)
{
  return fabs (a - b) <= tol;
}
#define ALMOST_EQ(a, b, tol)                                                  \
  _almost_eq ((double)(a), (double)(b), (double)(tol))

static const double PI = 3.14159265358979323846;

/* Fill buf with a unit-rate complex exponential of normalised frequency fn
 * (cycles/sample) and amplitude amp. */
static void
gen_tone (float complex *buf, size_t len, double fn, double amp)
{
  for (size_t k = 0; k < len; k++)
    buf[k] = (float)(amp * cos (2.0 * PI * fn * (double)k))
             + (float)(amp * sin (2.0 * PI * fn * (double)k)) * I;
}

/* Drive obj with `total` input samples in `chunk`-sized blocks; return the
 * first emitted frame in out (capacity cap), or 0 if none appeared. */
static size_t
drive_first_frame (specan_state_t *obj, const float complex *tone,
                   size_t total, size_t chunk, float *out, size_t cap)
{
  for (size_t i = 0; i < total; i += chunk)
    {
      size_t m  = (i + chunk <= total) ? chunk : total - i;
      size_t no = specan_execute (obj, tone + i, m, out, cap);
      if (no)
        return no;
    }
  return 0;
}

static size_t
argmax (const float *a, size_t n)
{
  size_t mi = 0;
  for (size_t i = 1; i < n; i++)
    if (a[i] > a[mi])
      mi = i;
  return mi;
}

int
main (void)
{
  int            _fails = 0;
  const double   fs = 1.0e6, span = 200e3, rbw = 1500.0;
  const size_t   NTONE = 1u << 14; /* 16384 input samples per drive */
  float complex *tone  = malloc (NTONE * sizeof *tone);
  float         *out   = malloc (2048 * sizeof *out);
  CHECK (tone && out);
  if (!tone || !out)
    return 1;

  /* 1. Invalid arguments are rejected (no opaque NULL surprises). */
  CHECK (specan_create (0.0, span, rbw, 0, 0, 0, 1.0, 0, 1, 1)
         == NULL); /* fs   */
  CHECK (specan_create (fs, 0.0, rbw, 0, 0, 0, 1.0, 0, 1, 1)
         == NULL); /* span */
  CHECK (specan_create (fs, span, 0.0, 0, 0, 0, 1.0, 0, 1, 1)
         == NULL); /* rbw  */
  CHECK (specan_create (fs, span, rbw, 0, 0, 0, 1.0, 0, 1, 0)
         == NULL); /* navg */

  /* 2. The natural params derive a sane DSP grid. */
  specan_state_t *sa = specan_create (fs, span, rbw, 0, 0, 0, 1.0, 0, 1, 1);
  CHECK (sa != NULL);
  if (!sa)
    return 1;
  CHECK (ALMOST_EQ (sa->fs_out, span * 1.28, 1.0)); /* span -> decim rate */
  CHECK (sa->nfft == 2 * sa->n);                    /* pad = 2, n is pow2  */
  CHECK (sa->disp_n % 2 == 1);                      /* odd, DC-centred     */
  CHECK (sa->disp_lo + sa->disp_n <= sa->nfft);
  double realized_rbw = sa->psd->enbw * sa->fs_out / (double)sa->n;
  CHECK (ALMOST_EQ (realized_rbw, rbw, rbw * 0.05)); /* RBW met within 5%   */
  CHECK (sa->beta > 0.0);                            /* Kaiser actually used*/

  /* 3. A unit tone at +30 kHz lands at +30 kHz in the display, near 0 dB. */
  const double f_off = 30e3;
  gen_tone (tone, NTONE, f_off / fs, 1.0);
  size_t nfr = drive_first_frame (sa, tone, NTONE, 4096, out, 2048);
  CHECK (nfr == sa->disp_n);
  size_t pk     = argmax (out, nfr);
  size_t dc_bin = sa->disp_n / 2; /* odd length -> exact DC-centred index */
  double bin_hz = sa->fs_out / (double)sa->nfft;
  double pk_hz  = ((double)pk - (double)dc_bin) * bin_hz;
  CHECK (ALMOST_EQ (pk_hz, f_off, bin_hz)); /* within one display bin  */
  CHECK (out[pk] > -3.0);                   /* ~0 dBFS for amplitude 1 */
  CHECK (out[pk] - out[5] > 30.0);          /* tone clears far bins     */

  /* 4. Retuning to the tone moves it to DC (cheap LO retune, no rebuild). */
  specan_retune (sa, f_off);
  size_t nfr2 = drive_first_frame (sa, tone, NTONE, 4096, out, 2048);
  CHECK (nfr2 == sa->disp_n);
  size_t pk2 = argmax (out, nfr2);
  CHECK (llabs ((long long)pk2 - (long long)dc_bin) <= 2);
  specan_destroy (sa);

  /* 5. navg buffers a full averaging window before emitting a frame. */
  specan_state_t *sb = specan_create (fs, span, rbw, 0, 0, 0, 1.0, 0, 1, 2);
  CHECK (sb != NULL);
  if (sb)
    {
      /* One window length of input is far short of n*navg decimated. */
      CHECK (specan_execute (sb, tone, sb->n, out, 2048) == 0);
      size_t nfr3 = drive_first_frame (sb, tone, NTONE, 4096, out, 2048);
      CHECK (nfr3 == sb->disp_n);
      specan_destroy (sb);
    }

  free (tone);
  free (out);
  if (_fails)
    {
      fprintf (stderr, "test_specan_core FAILED (%d)\n", _fails);
      return 1;
    }
  /* serializable state — ddc + psd children + pending samples resume. */
  {
    float complex in[4096];
    float         out[2048];
    for (int i = 0; i < 4096; i++)
      in[i] = (float)(i % 7) - 3.0f + 0.2f * I;
    specan_state_t *a
        = specan_create (1e6, 1e5, 1e3, 0.0, 0.0, 0.0, 1.0, 0, 1, 2);
    specan_state_t *b
        = specan_create (1e6, 1e5, 1e3, 0.0, 0.0, 0.0, 1.0, 0, 1, 2);
    CHECK (a != NULL && b != NULL);
    (void)specan_execute (a, in, 4096, out, 2048);
    DP_STATE_ROUNDTRIP_TEST (specan, a, b);
    CHECK (b->pend_len == a->pend_len);
    specan_destroy (a);
    specan_destroy (b);
  }

  printf ("test_specan_core PASSED\n");
  return 0;
}
