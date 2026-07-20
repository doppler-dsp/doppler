#include "doppler_channel/doppler_channel_core.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* The output/input resampling ratio at receive time t. Config-derived, so it
   is recomputed rather than cached: create() owns the configuration and
   set_state() restores only running state, per the state-serialization rule.
 */
static inline double
doppler_channel_ratio (const doppler_channel_state_t *s, double t)
{
  return 1.0 / doppler_channel_scale (s, t);
}

doppler_channel_state_t *
doppler_channel_create (double fs, double carrier_hz, double doppler_ppm,
                        double doppler_rate_ppm_s)
{
  if (!(fs > 0.0))
    return NULL;
  doppler_channel_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  obj->fs                 = fs;
  obj->carrier_hz         = carrier_hz;
  obj->doppler_ppm        = doppler_ppm;
  obj->doppler_rate_ppm_s = doppler_rate_ppm_s;

  /* A scale of zero or less would mean time stopping or running backwards. */
  if (doppler_channel_scale (obj, 0.0) <= 0.0)
    {
      free (obj);
      return NULL;
    }

  obj->rs = resamp_create (doppler_channel_ratio (obj, 0.0));
  if (!obj->rs)
    {
      free (obj);
      return NULL;
    }
  obj->ctrl = malloc (DOPPLER_CHANNEL_MAX_BLOCK * sizeof (*obj->ctrl));
  if (!obj->ctrl)
    {
      resamp_destroy (obj->rs);
      free (obj);
      return NULL;
    }
  obj->ctrl_cap = DOPPLER_CHANNEL_MAX_BLOCK;
  return obj;
}

void
doppler_channel_destroy (doppler_channel_state_t *state)
{
  if (!state)
    return;
  resamp_destroy (state->rs);
  free (state->ctrl);
  free (state);
}

void
doppler_channel_reset (doppler_channel_state_t *state)
{
  resamp_reset (state->rs);
  state->n_in  = 0;
  state->n_out = 0;
}

size_t
doppler_channel_execute_max_out (doppler_channel_state_t *state)
{
  /* The binding sizes its buffer from this alone — it never sees the input
     length — so the bound assumes a full DOPPLER_CHANNEL_MAX_BLOCK input, the
     same convention RateConverter_execute_max_out uses.

     Output count is input/(1+d), maximised where d is smallest, so evaluate
     the scale at both ends of the block the next call could span and take the
     smaller. With a ramp this bound tracks the stream instead of going stale;
     the binding re-queries it every call. */
  double t0 = (double)state->n_in / state->fs;
  double t1 = t0 + (double)DOPPLER_CHANNEL_MAX_BLOCK / state->fs;
  double a  = doppler_channel_scale (state, t0);
  double b  = doppler_channel_scale (state, t1);
  double lo = (a < b) ? a : b;
  if (lo < 1e-6)
    lo = 1e-6; /* absurd configuration: bound the allocation anyway */
  return (size_t)((double)DOPPLER_CHANNEL_MAX_BLOCK / lo) + 2u;
}

size_t
doppler_channel_execute (doppler_channel_state_t *state,
                         const float complex *x, size_t x_len,
                         float complex *out, size_t max_out)
{
  size_t n_out = 0;
  /* Chip away at the input in ctrl-buffer-sized pieces. resamp_execute_ctrl is
     input-driven and its accumulator carries across calls, so chunking here is
     invisible in the output. */
  for (size_t off = 0; off < x_len && n_out < max_out;)
    {
      size_t m = x_len - off;
      if (m > state->ctrl_cap)
        m = state->ctrl_cap;

      /* Per-sample rate deviation about the base ratio the resampler was
         built with. The deviation is what tracks the ramp exactly; with
         doppler_rate_ppm_s == 0 every entry is 0 and this is a plain
         fixed-ratio resample.

         t here is the receive time of an INPUT sample, taken as n_in/fs. The
         exact input->receive mapping differs by the dilation itself (~1e-5
         relative), and it enters only as the argument of the ramp, so the
         induced error in d is ~1e-5 * d_dot * t — far below the ppm the
         parameter is quoted in. */
      double base = doppler_channel_ratio (state, 0.0);
      for (size_t i = 0; i < m; i++)
        {
          double t = (double)(state->n_in + i) / state->fs;
          state->ctrl[i]
              = (float complex) (doppler_channel_ratio (state, t) - base);
        }

      size_t got = resamp_execute_ctrl (state->rs, x + off, state->ctrl, m,
                                        out + n_out, max_out - n_out);
      state->n_in += m;
      n_out += got;
      off += m;
    }

  /* Carrier, on the OUTPUT clock. Skipping the loop when there is nothing to
     apply keeps a pure time-dilation configuration (carrier_hz = 0, used to
     isolate a code loop under test) free of a per-sample complex multiply. */
  if (state->carrier_hz != 0.0
      && (state->doppler_ppm != 0.0 || state->doppler_rate_ppm_s != 0.0))
    {
      for (size_t k = 0; k < n_out; k++)
        {
          double t  = (double)(state->n_out + k) / state->fs;
          double ph = doppler_channel_phase (state, t);
          /* Reduce to one turn before the float cast: the phase is absolute
             (~5e7 cycles over a long capture) and cexpf would lose the
             fraction that actually matters. */
          ph -= floor (ph);
          out[k] *= cexpf ((float)(2.0 * M_PI * ph) * I);
        }
    }
  state->n_out += n_out;
  return n_out;
}

double
doppler_channel_get_elapsed_s (const doppler_channel_state_t *state)
{
  return (double)state->n_out / state->fs;
}

double
doppler_channel_get_offset_hz (const doppler_channel_state_t *state)
{
  double t = doppler_channel_get_elapsed_s (state);
  return state->carrier_hz
         * (state->doppler_ppm + state->doppler_rate_ppm_s * t) * 1e-6;
}

/* ---- state serialization ------------------------------------------------ */

/* Running state only: the two sample clocks plus the resampler's own blob
   (delay line + fractional accumulator). Configuration is restored by
   create(), so none of fs/carrier_hz/doppler_* is packed here. */

size_t
doppler_channel_state_bytes (const doppler_channel_state_t *state)
{
  return sizeof (dp_state_hdr_t) + 2u * sizeof (uint64_t)
         + resamp_state_bytes (state->rs);
}

void
doppler_channel_get_state (const doppler_channel_state_t *state, void *blob)
{
  size_t      total = doppler_channel_state_bytes (state);
  dp_writer_t w     = dp_writer_init (blob, total);
  dp_w_hdr (&w, DOPPLER_CHANNEL_STATE_MAGIC, DOPPLER_CHANNEL_STATE_VERSION,
            total);
  dp_w_u64 (&w, state->n_in);
  dp_w_u64 (&w, state->n_out);
  void *child = dp_w_reserve (&w, resamp_state_bytes (state->rs));
  if (child)
    resamp_get_state (state->rs, child);
}

int
doppler_channel_set_state (doppler_channel_state_t *state, const void *blob)
{
  size_t total = doppler_channel_state_bytes (state);
  int    rc    = dp_state_validate (blob, total, DOPPLER_CHANNEL_STATE_MAGIC,
                                    DOPPLER_CHANNEL_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, total);
  (void)dp_r_reserve (&r, sizeof (dp_state_hdr_t)); /* skip the envelope */
  state->n_in       = dp_r_u64 (&r);
  state->n_out      = dp_r_u64 (&r);
  const void *child = dp_r_reserve (&r, resamp_state_bytes (state->rs));
  if (!child)
    return DP_ERR_INVALID;
  /* The child blob is self-validating — a wrong resampler payload is rejected
     by resamp_set_state's own envelope check, not silently reinterpreted. */
  return resamp_set_state (state->rs, child);
}
