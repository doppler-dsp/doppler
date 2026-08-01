/**
 * @file ddc_core.c
 * @brief Digital Down-Converter implementation.
 *
 * Ddc: lo_steps → element-wise multiply → RateConverter_execute.  The
 * real-input twin lives in ddcr/ddcr_core.c.
 *
 * execute() allocates a temporary mix buffer per call.
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
#include "lo/lo_core.h"

#include <complex.h>
#include <stdlib.h>
#include <string.h>

/* The plain and matched constructors share everything but the terminal
   stage's bank, so they share a body; the two public entry points exist
   because they are different objects to a caller (and different Python
   flavors: DDC and MatchedDDC). */
static ddc_state_t *
_ddc_new (double norm_freq, double rate, int pulse, double beta, size_t span,
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
  /* With a pulse, compensate = 1 unconditionally: on a CIC plan the droop
     fold is worth 28 dB for six taps per arm, folded into a bank the cascade
     already evaluates, so no matched-filter operating point wants it off. */
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
  /* The rectangle is exactly one symbol wide, so its matched filter is a
     boxcar of pulse_sps taps: at 2 output samples per symbol that is a
     two-tap sum, which barely opens the eye (measured on the timing loop
     this feeds: lock statistic -0.34 at 2 against +0.95 at 4). The RRC
     spans many symbols and is unaffected. */
  s->narrow_pulse = (pulse == RC_PULSE_IANDD && pulse_sps < 4.0);
  return s;
}

ddc_state_t *
ddc_create (double norm_freq, double rate)
{
  return _ddc_new (norm_freq, rate, RC_PULSE_NONE, 0.0, 0, 0.0, 0);
}

ddc_state_t *
ddc_create_matched (double norm_freq, double rate, int pulse, double beta,
                    size_t span, double pulse_sps, size_t num_phases)
{
  if (pulse == RC_PULSE_NONE) /* use ddc_create() for a plain conversion */
    return NULL;
  return _ddc_new (norm_freq, rate, pulse, beta, span, pulse_sps, num_phases);
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
ddc_execute_max_out (ddc_state_t *s, size_t x_len)
{
  /* gh-607: the binding sizes the output buffer to this per-call bound and
     resizes down to the actual count. A DDC decimates (or passes at unity),
     so the output never exceeds the input length — x_len is a safe upper
     bound, matching the old n_in fallback exactly. */
  (void)s;
  return x_len;
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
  lo_steps (s->lo, n_in, mix, n_in);
  for (size_t i = 0; i < n_in; i++)
    mix[i] = in[i] * mix[i];

  size_t nout = RateConverter_execute (s->rc, mix, n_in, out, max_out);
  free (mix);
  return nout;
}

size_t
ddc_execute_ctrl_max_out (ddc_state_t *s, size_t x_len)
{
  /* Output never exceeds the input length (see ddc_execute_max_out). */
  (void)s;
  return x_len;
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
ddc_execute_ctrl_push_tap (ddc_state_t *s, float _Complex x, double rate_ctrl,
                           double freq_ctrl, float _Complex *out,
                           size_t max_out, float _Complex *lo_out, int *n_lo)
{
  float _Complex z = x * lo_step_ctrl (s->lo, freq_ctrl);
  if (lo_out)
    *lo_out = z;
  if (n_lo)
    *n_lo = 1; /* a complex front end mixes every input it is given */
  return RateConverter_execute_ctrl_push (s->rc, z, rate_ctrl, out, max_out);
}

size_t
ddc_execute_ctrl_push (ddc_state_t *s, float _Complex x, double rate_ctrl,
                       double freq_ctrl, float _Complex *out, size_t max_out)
{
  return ddc_execute_ctrl_push_tap (s, x, rate_ctrl, freq_ctrl, out, max_out,
                                    NULL, NULL);
}

bool
ddc_get_narrow_pulse (const ddc_state_t *s)
{
  return s->narrow_pulse;
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
