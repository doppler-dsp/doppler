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
 *
 * The control-port variants replace lo_steps with a per-sample lo_step_ctrl
 * and RateConverter_execute with RateConverter_execute_ctrl; the push forms
 * do the same one sample at a time.  Nothing else differs — the two ports are
 * the LO's and the terminal stage's own accumulators, steered in place.
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
ddc_create (double norm_freq, double rate, int pulse, double beta, size_t span,
            double pulse_sps, size_t num_phases)
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
  /* RC_PULSE_NONE is a plain down-conversion: the cascade keeps its Kaiser
     anti-alias bank and the remaining arguments are unused.  With a pulse,
     compensate = 1 unconditionally -- on a CIC plan the droop fold is worth
     28 dB for six taps per arm, folded into a bank the cascade already
     evaluates, so no matched-filter operating point wants it off. */
  s->rc = (pulse == RC_PULSE_NONE)
              ? RateConverter_create (rate, 0)
              : RateConverter_create_matched (rate, 1, pulse, beta, span,
                                              pulse_sps, num_phases);
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

size_t
ddc_execute_ctrl_max_out (ddc_state_t *s)
{
  (void)s;
  return 0; /* 0 -> the binding sizes the buffer from the input block */
}

size_t
ddc_execute_ctrl_push_max_out (ddc_state_t *s)
{
  /* A single input completes at most ceil(rate) + 1 output periods, and the
     binding has no input block to size its buffer from here — unlike the
     block forms, whose 0 means "size it from the input". */
  double rate = ddc_get_rate (s);
  return (size_t)(rate > 1.0 ? rate : 1.0) + 2;
}

size_t
ddc_execute_ctrl (ddc_state_t *s, const float _Complex *in, size_t n_in,
                  double rate_ctrl, double freq_ctrl, float _Complex *out,
                  size_t max_out)
{
  if (n_in == 0)
    return 0;

  float _Complex *mix = malloc (n_in * sizeof (float _Complex));
  if (!mix)
    return 0;

  /* One LO step per input, carrying the frequency control.  lo_step_ctrl with
     ctrl == 0 is bit-identical to the vectorised lo_steps() path (both index
     the same LUT off the same integer accumulator), so no fast path is worth
     the branch here. */
  for (size_t i = 0; i < n_in; i++)
    mix[i] = in[i] * lo_step_ctrl (s->lo, freq_ctrl);

  size_t nout
      = RateConverter_execute_ctrl (s->rc, mix, n_in, rate_ctrl, out, max_out);
  free (mix);
  return nout;
}

size_t
ddc_execute_ctrl_push (ddc_state_t *s, float _Complex x, double rate_ctrl,
                       double freq_ctrl, float _Complex *out, size_t max_out)
{
  float _Complex z = x * lo_step_ctrl (s->lo, freq_ctrl);
  return RateConverter_execute_ctrl_push (s->rc, z, rate_ctrl, out, max_out);
}

bool
ddc_get_clipped (const ddc_state_t *s)
{
  return RateConverter_get_clipped (s->rc) != 0;
}

/* ── Serializable state — standard envelope + LO + RateConverter ─────────────
 * Layout: [dp_state_hdr_t][ddc_extra_t][lo][rc]; see dp_state.h. */

size_t
ddc_state_bytes (const ddc_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (ddc_extra_t)
         + lo_state_bytes (s->lo) + RateConverter_state_bytes (s->rc);
}

void
ddc_get_state (const ddc_state_t *s, void *blob)
{
  DP_GET_OPEN (DDC_STATE_MAGIC, DDC_STATE_VERSION, ddc_state_bytes (s));
  dp_w_f64 (&_w, s->rc->rate); /* ddc_extra_t */
  DP_W_CHILD (&_w, lo, s->lo);
  DP_W_CHILD (&_w, RateConverter, s->rc);
}

int
ddc_set_state (ddc_state_t *s, const void *blob)
{
  DP_SET_OPEN (DDC_STATE_MAGIC, DDC_STATE_VERSION, ddc_state_bytes (s));
  if (dp_r_f64 (&_r) != s->rc->rate) /* ddc_extra_t.rate is the layout key */
    return DP_ERR_INVALID;
  DP_R_CHILD (&_r, lo, s->lo);
  DP_R_CHILD (&_r, RateConverter, s->rc);
  return DP_OK;
}

DP_DEFINE_RUN (ddc, ddc_state_t, float _Complex, float _Complex)

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
ddcr_create (double norm_freq, double rate, int pulse, double beta,
             size_t span, double pulse_sps, size_t num_phases)
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
   * The halfband decimates by 2, so the cascade sees fs_in/2.  To achieve
   * total rate = fs_out/fs_in it must run at 2*rate.  See ddc_create() for
   * the pulse / compensate contract, which is identical here.
   */
  s->rc = (pulse == RC_PULSE_NONE)
              ? RateConverter_create (2.0 * rate, 0)
              : RateConverter_create_matched (2.0 * rate, 1, pulse, beta, span,
                                              pulse_sps, num_phases);
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

/* ── Serializable state — standard envelope + the chain's leaf serializers ───
 * Layout: [dp_state_hdr_t][ddcr_extra_t][r2c][lo][rc], children in
 * signal-chain order; see dp_state.h. */

size_t
ddcr_state_bytes (const ddcr_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (ddcr_extra_t)
         + hbdecim_r2c_state_bytes (s->r2c) + lo_state_bytes (s->lo)
         + RateConverter_state_bytes (s->rc);
}

void
ddcr_get_state (const ddcr_state_t *s, void *blob)
{
  DP_GET_OPEN (DDCR_STATE_MAGIC, DDCR_STATE_VERSION, ddcr_state_bytes (s));
  dp_w_f64 (&_w, s->rate); /* ddcr_extra_t */
  DP_W_CHILD (&_w, hbdecim_r2c, s->r2c);
  DP_W_CHILD (&_w, lo, s->lo);
  DP_W_CHILD (&_w, RateConverter, s->rc);
}

int
ddcr_set_state (ddcr_state_t *s, const void *blob)
{
  DP_SET_OPEN (DDCR_STATE_MAGIC, DDCR_STATE_VERSION, ddcr_state_bytes (s));
  if (dp_r_f64 (&_r) != s->rate) /* ddcr_extra_t.rate is the layout key */
    return DP_ERR_INVALID;
  DP_R_CHILD (&_r, hbdecim_r2c, s->r2c);
  DP_R_CHILD (&_r, lo, s->lo);
  DP_R_CHILD (&_r, RateConverter, s->rc);
  return DP_OK;
}

DP_DEFINE_RUN (ddcr, ddcr_state_t, float, float _Complex)

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

size_t
ddcr_execute_ctrl (ddcr_state_t *s, const float *in, size_t n_in,
                   double rate_ctrl, double freq_ctrl, float _Complex *out,
                   size_t max_out)
{
  if (n_in == 0)
    return 0;

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

  /* The LO runs at the intermediate rate, so the frequency control is applied
     once per halfband output — not once per ADC sample. */
  for (size_t i = 0; i < n_hb; i++)
    hb_buf[i] = hb_buf[i] * lo_step_ctrl (s->lo, freq_ctrl);

  size_t nout = RateConverter_execute_ctrl (s->rc, hb_buf, n_hb, rate_ctrl,
                                            out, max_out);
  free (hb_buf);
  return nout;
}

size_t
ddcr_execute_ctrl_push (ddcr_state_t *s, float x, double rate_ctrl,
                        double freq_ctrl, float _Complex *out, size_t max_out)
{
  /* The 2:1 halfband is the block API's own state machine — one sample in,
     0 or 1 intermediate samples out.  Half the pushes end here. */
  float _Complex z;
  if (hbdecim_r2c_execute (s->r2c, &x, 1, &z, 1) == 0)
    return 0;

  z = z * lo_step_ctrl (s->lo, freq_ctrl);
  return RateConverter_execute_ctrl_push (s->rc, z, rate_ctrl, out, max_out);
}

bool
ddcr_get_clipped (const ddcr_state_t *s)
{
  return RateConverter_get_clipped (s->rc) != 0;
}
