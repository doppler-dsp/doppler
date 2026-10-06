#include "doppler/doppler_channel/doppler_channel_core.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* The output/input resampling ratio at receive time t. Config-derived, so it
   is recomputed rather than cached: create() owns the configuration and
   set_state() restores only running state, per the state-serialization rule.
 */
static inline double
doppler_channel_ratio (const dp_doppler_channel_state_t *s, double t)
{
  return 1.0 / doppler_channel_scale (s, t);
}

dp_doppler_channel_state_t *
dp_doppler_channel_create (double fs, double carrier_hz, double doppler_ppm,
                           double doppler_rate_ppm_s)
{
  if (!(fs > 0.0))
    return NULL;
  dp_doppler_channel_state_t *obj = dp_xcalloc (1, sizeof (*obj));
  obj->fs                         = fs;
  obj->carrier_hz                 = carrier_hz;
  obj->doppler_ppm                = doppler_ppm;
  obj->doppler_rate_ppm_s         = doppler_rate_ppm_s;

  /* A scale of zero or less would mean time stopping or running backwards --
   * a reachable invalid configuration (a doppler_ppm at or past -1e6). */
  if (doppler_channel_scale (obj, 0.0) <= 0.0)
    {
      free (obj);
      return NULL;
    }

  obj->rs       = dp_xnn (dp_resamp_create (doppler_channel_ratio (obj, 0.0)));
  obj->ctrl     = dp_xmalloc (DOPPLER_CHANNEL_MAX_BLOCK * sizeof (*obj->ctrl));
  obj->ctrl_cap = DOPPLER_CHANNEL_MAX_BLOCK;
  return obj;
}

void
dp_doppler_channel_destroy (dp_doppler_channel_state_t *state)
{
  if (!state)
    return;
  dp_resamp_destroy (state->rs);
  free (state->ctrl);
  free (state->pos);
  free (state);
}

void
dp_doppler_channel_reset (dp_doppler_channel_state_t *state)
{
  dp_resamp_reset (state->rs);
  state->n_in  = 0;
  state->n_out = 0;
  /* The profile's last d is running state, so it resets with the clocks;
     leaving it would report a fresh stream at the previous one's offset. */
  state->prof_d   = 0.0;
  state->profiled = 0u;
}

size_t
dp_doppler_channel_execute_max_out (dp_doppler_channel_state_t *state)
{
  /* The binding sizes its buffer from this alone — it never sees the input
     length — so the bound assumes a full DOPPLER_CHANNEL_MAX_BLOCK input, the
     same convention dp_RateConverter_execute_max_out uses.

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
dp_doppler_channel_execute (dp_doppler_channel_state_t *state,
                            const float _Complex *x, size_t x_len,
                            float _Complex *out, size_t max_out)
{
  size_t n_out = 0;
  /* Chip away at the input in ctrl-buffer-sized pieces. dp_resamp_execute_ctrl
     is input-driven and its accumulator carries across calls, so chunking here
     is invisible in the output. */
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
          double t       = (double)(state->n_in + i) / state->fs;
          state->ctrl[i] = doppler_channel_ratio (state, t) - base;
        }

      size_t got = dp_resamp_execute_ctrl (state->rs, x + off, state->ctrl, m,
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

size_t
dp_doppler_channel_execute_profile_max_out (dp_doppler_channel_state_t *state,
                                            size_t                      n)
{
  (void)state; /* jm's signature: the binding sizes before it sees ppm[] */
  /* The profile is unseen, so scale the known length by a floor on the scale
     (a 2x expansion) plus the resampler's carried-accumulator slack, the same
     slack dp_doppler_channel_execute_max_out() allows. */
  return 2u * n + 2u;
}

/* Reject a profile sample at or below -1e6 ppm: a scale of zero or less
   means time has stopped or reversed -- create() already refuses the scalar
   equivalent, and the array form must not be the way in. Also catches NaN. */
static int
profile_ok (const double *ppm, size_t n)
{
  for (size_t i = 0; i < n; i++)
    if (!(1.0 + ppm[i] * 1e-6 > 0.0))
      return 0;
  return 1;
}

size_t
dp_doppler_channel_execute_profile (dp_doppler_channel_state_t *state,
                                    const float _Complex *x, size_t x_len,
                                    const double *ppm, size_t ppm_len,
                                    float _Complex *out, size_t max_out)
{
  if (!state || !x || !ppm || !out)
    return 0;
  /* The length contract, enforced rather than documented: one Doppler value
     per waveform sample. */
  if (ppm_len != x_len)
    return 0;
  /* Validated over the WHOLE profile first: a check folded into the chunk
     loop would emit a valid prefix and then stop, which reads as a short
     read rather than a rejected argument. */
  if (!profile_ok (ppm, x_len))
    return 0;

  /* Positions come back parallel to the outputs, so the scratch is as long
     as the output buffer can be asked to fill. Grown on demand and never
     serialized: it is a work area, not state. */
  if (state->pos_cap < max_out)
    {
      state->pos     = dp_xrealloc (state->pos, max_out * sizeof (double));
      state->pos_cap = max_out;
    }

  size_t n_out = 0;
  double base  = doppler_channel_ratio (state, 0.0);
  int    turn  = (state->carrier_hz != 0.0);

  for (size_t off = 0; off < x_len && n_out < max_out;)
    {
      size_t m = x_len - off;
      if (m > state->ctrl_cap)
        m = state->ctrl_cap;

      /* The profile IS the deviation. The resampler's rate is `base + ctrl`,
         so subtracting the same `base` it was built with makes ppm[]
         absolute and cancels the create-time scalar exactly. */
      for (size_t i = 0; i < m; i++)
        state->ctrl[i] = 1.0 / (1.0 + ppm[off + i] * 1e-6) - base;

      size_t got = dp_resamp_execute_ctrl_pos (state->rs, x + off, state->ctrl,
                                               m, out + n_out, state->pos,
                                               max_out - n_out);

      /* Carrier, from where the resampler says each output sits. The excess
         delay at absolute output k is (p_k - k + 1) samples, p_k being its
         position on the input timeline: n_in inputs came before this chunk,
         so p_k = n_in + pos[j], and k = n_out + j. The integer part is
         formed in int64 -- exact -- and only the sub-sample fraction is a
         double, so a long capture loses nothing to cancellation.

         The +1 is the pipeline convention (the first tick of a fresh stream
         is emitted before any input is loaded, at -1); without it a profile
         of zeros would still carry a constant fc/fs-cycle phase. */
      if (turn)
        for (size_t j = 0; j < got; j++)
          {
            double  rel            = state->pos[j];
            double  fl             = floor (rel);
            int64_t whole          = (int64_t)state->n_in
                                     - (int64_t)(state->n_out + n_out + j) + 1
                                     + (int64_t)fl;
            double  excess_samples = (double)whole + (rel - fl);
            /* Cycles, reduced to one turn before the float cast: cexpf would
               lose the fraction that actually matters on a ~5e7-cycle
               absolute phase. */
            double ph = state->carrier_hz * excess_samples / state->fs;
            ph -= floor (ph);
            if (ph != 0.0) /* an all-zero profile pays no multiply */
              out[n_out + j] *= cexpf ((float)(2.0 * M_PI * ph) * I);
          }

      state->n_in += m;
      n_out += got;
      off += m;
    }

  if (x_len)
    state->prof_d = ppm[x_len - 1] * 1e-6;
  state->profiled = 1u;
  state->n_out += n_out;
  return n_out;
}

double
dp_doppler_channel_get_elapsed_s (const dp_doppler_channel_state_t *state)
{
  return (double)state->n_out / state->fs;
}

double
dp_doppler_channel_get_offset_hz (const dp_doppler_channel_state_t *state)
{
  /* A stream a profile has driven is no longer described by the create-time
     ramp, so reporting the closed form would name a frequency the capture
     does not have. */
  if (state->profiled)
    return state->carrier_hz * state->prof_d;
  double t = dp_doppler_channel_get_elapsed_s (state);
  return state->carrier_hz
         * (state->doppler_ppm + state->doppler_rate_ppm_s * t) * 1e-6;
}

double
dp_doppler_channel_get_delay_samples (const dp_doppler_channel_state_t *state)
{
  return dp_resamp_get_delay (state->rs);
}

/* ---- state serialization ------------------------------------------------ */

/* Running state only: the two sample clocks, the profile's last d, plus the
   resampler's own blob (delay line + fractional accumulator). Configuration
   is restored by create(), so none of fs/carrier_hz/doppler_* is packed here.

   The profile carrier needs nothing of its own: it is read off the
   resampler's position, which is the resampler blob plus the two clocks.
   `prof_d` is only what offset_hz reports, and it is here because a resumed
   stream that forgot a profile had driven it would report the closed form
   again. Adding it is why the layout version is 2. */

size_t
dp_doppler_channel_state_bytes (const dp_doppler_channel_state_t *state)
{
  return sizeof (dp_state_hdr_t) + 3u * sizeof (uint64_t) + sizeof (double)
         + dp_resamp_state_bytes (state->rs);
}

void
dp_doppler_channel_get_state (const dp_doppler_channel_state_t *state,
                              void                             *blob)
{
  size_t      total = dp_doppler_channel_state_bytes (state);
  dp_writer_t w     = dp_writer_init (blob, total);
  dp_w_hdr (&w, DOPPLER_CHANNEL_STATE_MAGIC, DOPPLER_CHANNEL_STATE_VERSION,
            total);
  dp_w_u64 (&w, state->n_in);
  dp_w_u64 (&w, state->n_out);
  dp_w_f64 (&w, state->prof_d);
  dp_w_u64 (&w, state->profiled);
  void *child = dp_w_reserve (&w, dp_resamp_state_bytes (state->rs));
  if (child)
    dp_resamp_get_state (state->rs, child);
}

int
dp_doppler_channel_set_state (dp_doppler_channel_state_t *state,
                              const void                 *blob)
{
  size_t total = dp_doppler_channel_state_bytes (state);
  int    rc    = dp_state_validate (blob, total, DOPPLER_CHANNEL_STATE_MAGIC,
                                    DOPPLER_CHANNEL_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, total);
  (void)dp_r_reserve (&r, sizeof (dp_state_hdr_t)); /* skip the envelope */
  state->n_in       = dp_r_u64 (&r);
  state->n_out      = dp_r_u64 (&r);
  state->prof_d     = dp_r_f64 (&r);
  state->profiled   = (uint8_t)(dp_r_u64 (&r) != 0u);
  const void *child = dp_r_reserve (&r, dp_resamp_state_bytes (state->rs));
  if (!child)
    return DP_ERR_INVALID;
  /* The child blob is self-validating — a wrong resampler payload is rejected
     by dp_resamp_set_state's own envelope check, not silently reinterpreted.
   */
  return dp_resamp_set_state (state->rs, child);
}
