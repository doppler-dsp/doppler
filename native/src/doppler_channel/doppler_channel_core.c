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
  doppler_channel_state_t *obj = dp_xcalloc (1, sizeof (*obj));
  obj->fs                      = fs;
  obj->carrier_hz              = carrier_hz;
  obj->doppler_ppm             = doppler_ppm;
  obj->doppler_rate_ppm_s      = doppler_rate_ppm_s;

  /* A scale of zero or less would mean time stopping or running backwards --
   * a reachable invalid configuration (a doppler_ppm at or past -1e6). */
  if (doppler_channel_scale (obj, 0.0) <= 0.0)
    {
      free (obj);
      return NULL;
    }

  obj->rs       = dp_xnn (resamp_create (doppler_channel_ratio (obj, 0.0)));
  obj->ctrl     = dp_xmalloc (DOPPLER_CHANNEL_MAX_BLOCK * sizeof (*obj->ctrl));
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
  /* The profile accumulator is running state, so it resets with the clocks.
     Leaving it would resume a fresh stream on the previous one's carrier. */
  state->excess_s = 0.0;
  state->prof_d   = 0.0;
  state->profiled = 0u;
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
          double t       = (double)(state->n_in + i) / state->fs;
          state->ctrl[i] = doppler_channel_ratio (state, t) - base;
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

/* Smallest scale a profile sample may imply. At or below zero, time has
   stopped or reversed -- create() already refuses the scalar equivalent, and
   the array form must not be the way in. */
static int
doppler_channel_profile_ok (const double *ppm, size_t n, double *d_min)
{
  if (!ppm || n == 0)
    return 0;
  double lo = ppm[0];
  for (size_t i = 0; i < n; i++)
    {
      if (!(1.0 + ppm[i] * 1e-6 > 0.0)) /* also catches NaN */
        return 0;
      if (ppm[i] < lo)
        lo = ppm[i];
    }
  if (d_min)
    *d_min = lo;
  return 1;
}

size_t
doppler_channel_profile_max_out (const double *ppm, size_t n)
{
  double lo = 0.0;
  if (!doppler_channel_profile_ok (ppm, n, &lo))
    return 0;
  /* +2 for the resampler's carried fractional accumulator, matching the slack
     doppler_channel_execute_max_out() allows for the same reason. */
  return (size_t)((double)n / (1.0 + lo * 1e-6)) + 2u;
}

size_t
doppler_channel_execute_profile_max_out (doppler_channel_state_t *state,
                                         size_t                   n)
{
  (void)state; /* jm's signature: the binding sizes before it sees ppm[] */
  /* +2 for the resampler's carried fractional accumulator, the same slack
     doppler_channel_execute_max_out() allows for the same reason. */
  return 2u * n + 2u;
}

size_t
doppler_channel_execute_profile (doppler_channel_state_t *state,
                                 const float complex *x, size_t x_len,
                                 const double *ppm, size_t ppm_len,
                                 float complex *out, size_t max_out)
{
  if (!state || !x || !out)
    return 0;
  /* The length contract, enforced rather than documented: one Doppler value
     per waveform sample. jm gives each array its own length, so a caller
     that pairs a profile with the wrong stream is a rejected call instead of
     a silent read past the end of the shorter one. */
  if (ppm_len != x_len)
    return 0;
  /* Validated over the WHOLE profile first: a check folded into the chunk
     loop would emit a valid prefix and then stop, which reads as a short
     read rather than a rejected argument. */
  if (!doppler_channel_profile_ok (ppm, x_len, NULL))
    return 0;

  size_t n_out = 0;
  double base  = doppler_channel_ratio (state, 0.0);

  for (size_t off = 0; off < x_len && n_out < max_out;)
    {
      size_t m = x_len - off;
      if (m > state->ctrl_cap)
        m = state->ctrl_cap;

      /* The profile IS the deviation. resamp's rate is `base + ctrl`, so
         subtracting the same `base` the resampler was built with makes
         ppm[] absolute and cancels the create-time scalar exactly. */
      for (size_t i = 0; i < m; i++)
        state->ctrl[i] = 1.0 / (1.0 + ppm[off + i] * 1e-6) - base;

      size_t got = resamp_execute_ctrl (state->rs, x + off, state->ctrl, m,
                                        out + n_out, max_out - n_out);

      /* Carrier for exactly the samples this chunk produced, while the
         chunk's own profile slice is in hand. Output sample j came from
         around input j*m/got; the two clocks differ by the dilation itself
         (~1e-5 relative), the same approximation the scalar path takes.

         The integral advances even with no carrier to apply, or a later
         carrier-bearing call would resume from a stale excess -- so the
         multiply is what is conditional here, not the accumulation.

         ORDER IS LOAD-BEARING: `excess_s` is read as the excess AT this
         sample and advanced afterwards, which is what makes it a left sum
         starting at zero and therefore equal to the closed form's `d*t` at
         `t = k/fs`. Advancing first instead gives every sample the NEXT
         sample's phase -- one increment is `fc*d/fs`, 2.9 degrees at 20 ppm
         on a 2.5 GHz carrier at 6.138 Msps, which is 0.05 of amplitude and
         exactly what the flat-profile equivalence test caught. */
      int turn = (state->carrier_hz != 0.0);
      for (size_t j = 0; j < got; j++)
        {
          size_t src = (size_t)((double)j * (double)m / (double)got);
          if (src >= m)
            src = m - 1;
          if (turn)
            {
              double ph = state->carrier_hz * state->excess_s;
              /* One turn before the float cast: the phase is absolute and
                 cexpf would lose the fraction that actually matters. */
              ph -= floor (ph);
              out[n_out + j] *= cexpf ((float)(2.0 * M_PI * ph) * I);
            }
          state->prof_d = ppm[off + src] * 1e-6;
          state->excess_s += state->prof_d / state->fs;
        }

      state->n_in += m;
      n_out += got;
      off += m;
    }

  state->profiled = 1u;
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
  /* A stream a profile has driven is no longer described by the create-time
     ramp, so reporting the closed form would name a frequency the capture
     does not have. */
  if (state->profiled)
    return state->carrier_hz * state->prof_d;
  double t = doppler_channel_get_elapsed_s (state);
  return state->carrier_hz
         * (state->doppler_ppm + state->doppler_rate_ppm_s * t) * 1e-6;
}

/* ---- state serialization ------------------------------------------------ */

/* Running state only: the two sample clocks, the profile accumulator, plus
   the resampler's own blob (delay line + fractional accumulator).
   Configuration is restored by create(), so none of fs/carrier_hz/doppler_*
   is packed here.

   The profile accumulator has to be here for the same reason the clocks do:
   it IS the carrier phase in profile mode, so a blob without it resumes a
   curved pass at zero excess -- a phase step at the seam, in the one mode
   whose whole point is that the carrier has no closed form to recover it
   from. Adding it is why the layout version is 2. */

size_t
doppler_channel_state_bytes (const doppler_channel_state_t *state)
{
  return sizeof (dp_state_hdr_t) + 2u * sizeof (uint64_t)
         + 2u * sizeof (double) + sizeof (uint64_t)
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
  dp_w_f64 (&w, state->excess_s);
  dp_w_f64 (&w, state->prof_d);
  dp_w_u64 (&w, (uint64_t)state->profiled);
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
  state->excess_s   = dp_r_f64 (&r);
  state->prof_d     = dp_r_f64 (&r);
  state->profiled   = (uint8_t)(dp_r_u64 (&r) != 0u);
  const void *child = dp_r_reserve (&r, resamp_state_bytes (state->rs));
  if (!child)
    return DP_ERR_INVALID;
  /* The child blob is self-validating — a wrong resampler payload is rejected
     by resamp_set_state's own envelope check, not silently reinterpreted. */
  return resamp_set_state (state->rs, child);
}
