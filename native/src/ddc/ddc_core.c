/**
 * @file ddc_core.c
 * @brief Digital Down-Converter implementation.
 *
 * Ddc:  lo_steps → element-wise multiply → RateConverter_execute
 * DdcR: hbdecim_r2c_execute → lo_steps → element-wise multiply
 *       → RateConverter_execute
 *
 * Both implementations allocate temporary buffers per execute() call.
 * For typical SDR block sizes (1 k–64 k samples) the Python GIL and
 * NumPy overhead dominate the malloc cost.
 *
 * RateConverter selects the cheapest cascade (CIC + optional HB +
 * polyphase) at create time, matching the rate automatically.
 */
#include "ddc/ddc_core.h"
#include "RateConverter/RateConverter_core.h"
#include "hbdecim/hbdecim_r2c_core.h"
#include "lo/lo_core.h"

#include <complex.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------
 * Default halfband FIR coefficients — 19-tap Kaiser prototype.
 *
 * Computed from _halfband_bank(atten=60.0, pb=0.4, sb=0.6), FIR branch.
 * Spectral spec (normalised to intermediate rate fs_in/2):
 *   passband  ≤ 0.4,  stopband  ≥ 0.6,  rejection ≥ 60 dB
 * ------------------------------------------------------------------ */
#define DDC_HB_TAPS 19
static const float s_hb_fir[DDC_HB_TAPS] = {
  1.5790532343089580e-03f,
  -4.6757734380662441e-03f,
  1.0443178936839104e-02f,
  -2.0174624398350716e-02f,
  3.5798925906419754e-02f,
  -6.0866370797157288e-02f,
  1.0411340743303299e-01f,
  -1.9753780961036682e-01f,
  6.3160091638565063e-01f,
  6.3160091638565063e-01f,
  -1.9753780961036682e-01f,
  1.0411340743303299e-01f,
  -6.0866370797157288e-02f,
  3.5798925906419754e-02f,
  -2.0174624398350716e-02f,
  1.0443178936839104e-02f,
  -4.6757734380662441e-03f,
  1.5790532343089580e-03f,
  0.0f,
};

/* ================================================================== */
/* Ddc                                                                */
/* ================================================================== */

struct ddc_state
{
  lo_state_t            *lo;
  RateConverter_state_t *rc;
};

ddc_state_t *
ddc_create (double norm_freq, double rate)
{
  if (rate <= 0.0)
    return NULL;
  ddc_state_t *s = malloc (sizeof *s);
  if (!s)
    return NULL;
  s->lo = lo_create (norm_freq);
  if (!s->lo)
    {
      free (s);
      return NULL;
    }
  s->rc = RateConverter_create (rate, 0);
  if (!s->rc)
    {
      lo_destroy (s->lo);
      free (s);
      return NULL;
    }
  return s;
}

void
ddc_destroy (ddc_state_t *s)
{
  if (!s)
    return;
  lo_destroy (s->lo);
  RateConverter_destroy (s->rc);
  free (s);
}

void
ddc_reset (ddc_state_t *s)
{
  lo_reset (s->lo);
  RateConverter_reset (s->rc);
}

double
ddc_get_norm_freq (const ddc_state_t *s)
{
  return lo_get_norm_freq (s->lo);
}

void
ddc_set_norm_freq (ddc_state_t *s, double norm_freq)
{
  lo_set_norm_freq (s->lo, norm_freq);
}

double
ddc_get_rate (const ddc_state_t *s)
{
  return s->rc->rate;
}

size_t
ddc_execute_max_out (ddc_state_t *s)
{
  (void)s;
  return 0;
}

size_t
ddc_execute (ddc_state_t *s, const float _Complex *in, size_t n_in,
             float _Complex *out, size_t max_out)
{
  if (n_in == 0)
    return 0;

  float _Complex *mix = malloc (n_in * sizeof (float _Complex));
  if (!mix)
    return 0;

  /* Generate LO phasors and multiply with input in one pass. */
  lo_steps (s->lo, n_in, mix);
  for (size_t i = 0; i < n_in; i++)
    mix[i] = in[i] * mix[i];

  size_t nout = RateConverter_execute (s->rc, mix, n_in, out, max_out);
  free (mix);
  return nout;
}

/* ================================================================== */
/* DdcR                                                               */
/* ================================================================== */

struct ddcr_state
{
  hbdecim_r2c_state_t   *r2c;
  lo_state_t            *lo;
  RateConverter_state_t *rc;
  double                 rate; /* total fs_out / fs_in */
};

ddcr_state_t *
ddcr_create (double norm_freq, double rate)
{
  if (rate <= 0.0 || rate >= 0.5)
    return NULL;
  ddcr_state_t *s = malloc (sizeof *s);
  if (!s)
    return NULL;

  s->r2c = hbdecim_r2c_create (DDC_HB_TAPS, s_hb_fir);
  if (!s->r2c)
    {
      free (s);
      return NULL;
    }
  s->lo = lo_create (norm_freq);
  if (!s->lo)
    {
      hbdecim_r2c_destroy (s->r2c);
      free (s);
      return NULL;
    }
  /*
   * The halfband decimates by 2, so the resampler sees fs_in/2.
   * To achieve total rate = fs_out/fs_in, the resampler must run at
   * rate_resamp = fs_out / (fs_in/2) = 2 * rate.
   */
  s->rc = RateConverter_create (2.0 * rate, 0);
  if (!s->rc)
    {
      lo_destroy (s->lo);
      hbdecim_r2c_destroy (s->r2c);
      free (s);
      return NULL;
    }
  s->rate = rate;
  return s;
}

void
ddcr_destroy (ddcr_state_t *s)
{
  if (!s)
    return;
  hbdecim_r2c_destroy (s->r2c);
  lo_destroy (s->lo);
  RateConverter_destroy (s->rc);
  free (s);
}

void
ddcr_reset (ddcr_state_t *s)
{
  hbdecim_r2c_reset (s->r2c);
  lo_reset (s->lo);
  RateConverter_reset (s->rc);
}

/* ── Serializable state — compose the chain's leaf serializers ──────────────
 * Layout: [ddcr_state_hdr_t][r2c][lo][rc...], children in signal-chain order.
 */

size_t
ddcr_state_bytes (const ddcr_state_t *s)
{
  return sizeof (ddcr_state_hdr_t) + hbdecim_r2c_state_bytes (s->r2c)
         + lo_state_bytes (s->lo) + RateConverter_state_bytes (s->rc);
}

void
ddcr_get_state (const ddcr_state_t *s, void *blob)
{
  ddcr_state_hdr_t *hd = (ddcr_state_hdr_t *)blob;
  hd->magic            = DDCR_STATE_MAGIC;
  hd->version          = (uint16_t)DDCR_STATE_VERSION;
  hd->_pad             = 0;
  hd->rate             = s->rate;
  hd->bytes            = (uint64_t)ddcr_state_bytes (s);

  char *p = (char *)blob + sizeof (ddcr_state_hdr_t);
  hbdecim_r2c_get_state (s->r2c, p);
  p += hbdecim_r2c_state_bytes (s->r2c);
  lo_get_state (s->lo, p);
  p += lo_state_bytes (s->lo);
  RateConverter_get_state (s->rc, p);
}

int
ddcr_set_state (ddcr_state_t *s, const void *blob)
{
  const ddcr_state_hdr_t *hd = (const ddcr_state_hdr_t *)blob;
  if (hd->magic != DDCR_STATE_MAGIC || hd->version != DDCR_STATE_VERSION
      || hd->rate != s->rate || hd->bytes != (uint64_t)ddcr_state_bytes (s))
    return -1;

  const char *p = (const char *)blob + sizeof (ddcr_state_hdr_t);
  hbdecim_r2c_set_state (s->r2c, p);
  p += hbdecim_r2c_state_bytes (s->r2c);
  lo_set_state (s->lo, p);
  p += lo_state_bytes (s->lo);
  RateConverter_set_state (s->rc, p);
  return 0;
}

size_t
ddcr_run (ddcr_state_t *s, const void *state_in, void *state_out,
          const float *in, size_t n_in, float _Complex *out, size_t max_out)
{
  if (state_in)
    {
      if (ddcr_set_state (s, state_in) != 0)
        return 0;
    }
  size_t n_out = ddcr_execute (s, in, n_in, out, max_out);
  if (state_out)
    ddcr_get_state (s, state_out);
  return n_out;
}

double
ddcr_get_norm_freq (const ddcr_state_t *s)
{
  return lo_get_norm_freq (s->lo);
}

void
ddcr_set_norm_freq (ddcr_state_t *s, double norm_freq)
{
  lo_set_norm_freq (s->lo, norm_freq);
}

double
ddcr_get_rate (const ddcr_state_t *s)
{
  return s->rate;
}

size_t
ddcr_execute (ddcr_state_t *s, const float *in, size_t n_in,
              float _Complex *out, size_t max_out)
{
  if (n_in == 0)
    return 0;

  /* Step 1: halfband R2C decimation (2:1). */
  size_t          hb_max = n_in / 2 + 2;
  float _Complex *hb_buf = malloc (hb_max * sizeof (float _Complex));
  if (!hb_buf)
    return 0;

  size_t n_hb = hbdecim_r2c_execute (s->r2c, in, n_in, hb_buf, hb_max);

  if (n_hb == 0)
    {
      free (hb_buf);
      return 0;
    }

  /* Step 2: LO mix at intermediate rate. */
  float _Complex *mix = malloc (n_hb * sizeof (float _Complex));
  if (!mix)
    {
      free (hb_buf);
      return 0;
    }
  lo_steps (s->lo, n_hb, mix);
  for (size_t i = 0; i < n_hb; i++)
    mix[i] = hb_buf[i] * mix[i];
  free (hb_buf);

  /* Step 3: rate-convert to target output rate. */
  size_t nout = RateConverter_execute (s->rc, mix, n_hb, out, max_out);
  free (mix);
  return nout;
}
