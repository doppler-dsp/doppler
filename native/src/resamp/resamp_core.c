#include "resamp/resamp_core.h"
#include "nco/nco_core.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* Kaiser bank builder                                                 */
/* ------------------------------------------------------------------ */

static double
resamp_bessel_i0 (double x)
{
  double sum = 1.0, term = 1.0;
  for (int k = 1; k < 30; k++)
    {
      term *= (x / (2.0 * k)) * (x / (2.0 * k));
      sum += term;
      if (term < 1e-20 * sum)
        break;
    }
  return sum;
}

static double
resamp_kaiser_beta (double atten)
{
  if (atten > 50.0)
    return 0.1102 * (atten - 8.7);
  if (atten >= 21.0)
    return 0.5842 * pow (atten - 21.0, 0.4) + 0.07886 * (atten - 21.0);
  return 0.0;
}

static unsigned
resamp_log2_u (size_t v)
{
  unsigned r = 0;
  while ((1u << r) < v)
    r++;
  return r;
}

/*
 * Build polyphase bank [num_phases][num_taps] from a Kaiser prototype.
 * atten: stopband attenuation in dB.  pb, sb: normalized pass/stop edges.
 * Returns heap-allocated bank, or NULL on failure.
 */
static float *
resamp_build_bank (size_t num_phases, size_t num_taps, double atten, double pb,
                   double sb)
{
  double beta  = resamp_kaiser_beta (atten);
  double pb_ph = pb / (double)num_phases;
  double sb_ph = sb / (double)num_phases;
  double wc    = 2.0 * M_PI * (pb_ph + (sb_ph - pb_ph) * 0.5);

  /* prototype length */
  size_t proto = num_phases * num_taps;
  if (proto % 2 == 0)
    proto++;
  int halflen = (int)(proto / 2);

  double *g = calloc (proto, sizeof (double));
  if (!g)
    return NULL;

  double b0 = resamp_bessel_i0 (beta);
  for (size_t i = 0; i < proto; i++)
    {
      double m   = (double)i - halflen;
      double mid = (double)(proto - 1) * 0.5;
      double u   = 2.0 * ((double)i - mid) / (double)(proto - 1);
      double w   = resamp_bessel_i0 (beta * sqrt (1.0 - u * u)) / b0;
      double s   = (m == 0.0) ? 1.0 : sin (wc * m) / (wc * m);
      g[i]       = w * wc / M_PI * s * (double)num_phases;
    }

  float *bank = malloc (num_phases * num_taps * sizeof (float));
  if (!bank)
    {
      free (g);
      return NULL;
    }

  for (size_t p = 0; p < num_phases; p++)
    for (size_t t = 0; t < num_taps; t++)
      {
        size_t idx             = t * num_phases + p;
        bank[p * num_taps + t] = (idx < proto) ? (float)g[idx] : 0.0f;
      }

  free (g);
  return bank;
}

/* Compute taps-per-phase from Kaiser spec. */
static size_t
resamp_kaiser_num_taps (size_t num_phases, double atten, double pb, double sb)
{
  double pb_ph = pb / (double)num_phases;
  double sb_ph = sb / (double)num_phases;
  size_t proto
      = (size_t)(1.0 + (atten - 8.0) / 2.285 / (2.0 * M_PI * (sb_ph - pb_ph)));
  size_t halflen = proto / 2;
  size_t htaps   = 2 * halflen + 1;
  return htaps / num_phases + 1;
}

/* ------------------------------------------------------------------ */
/* Internal create from conditioned bank                               */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* The resampling step, as a 32-bit phase increment -- MODULAR         */
/*                                                                      */
/* This is a RESAMPLING rule, not a phase-accumulator one, which is why */
/* it lives here instead of borrowing an NCO conversion.  The boundary  */
/* is the difference: an NCO phase must stay inside one period, so its  */
/* conversion SATURATES (nco_phase_units).  A resampler step of one     */
/* whole period per tick is not a limit case -- it IS rate == 1 -- and  */
/* in a phase word that is 0.                                           */
/*                                                                      */
/* Under the interpolator's rule (emit every tick, load when the        */
/* accumulator fails to advance) 0 is exactly right: u never changes,   */
/* so `u(k) <= u(k-1)` holds every tick, one input is consumed per      */
/* output, and the phase stays pinned to ONE arm -- so the path is that */
/* arm's filter.  test_resamp_core's R == 1 case owns that statement    */
/* and measures it.  Nothing is being resampled at rate 1, so a phase   */
/* that never advances is the correct answer, not a tolerated one.      */
/*                                                                      */
/* Saturating here is wrong but undramatic, and the number matters      */
/* because an earlier version of this comment guessed it: measured      */
/* through this path, inc = 2^32-1 emits 4097 outputs for 4096 inputs   */
/* (1.000244) where 0 emits exactly 4096.  A slow drift, not the "two   */
/* outputs per input" previously claimed here.                          */
/*                                                                      */
/* The all-zero `Synth(sps=1)` waveform that the conversion             */
/* consolidation fixed belongs to the OLDER rule, where emission was    */
/* gated on the phase advancing and 0 therefore emitted nothing at all  */
/* -- which is also why unity needed a memcpy short-circuit to look     */
/* correct.  Saturating restored output under that rule.  This rule     */
/* makes 0 correct outright, and the short-circuit is gone.             */
/*                                                                      */
/* Thirty-two bits are sufficient precisely because the load test is    */
/* `u(k) <= u(k-1)` and not a carry-out: equality is what carries the   */
/* "a full period elapsed" case that 0 encodes.                         */
/* ------------------------------------------------------------------ */

static inline uint32_t
resamp_step_inc (double rate, int upsample)
{
  /* The arithmetic is resamp's -- which reciprocal, and which branch -- and
     the CONVERSION is nco_core.h's, like every other double-valued phase
     quantity in the library.  The modular face is the one this rule wants:
     `q` reaches exactly 2^32 at rate 1 (the upsample branch requires
     rate >= 1, so 2^32/rate <= 2^32 with equality only there), and one whole
     period per tick is not a limit to clamp but rate 1 itself, which the
     `u(k) <= u(k-1)` load test reads back from 0 as "a period elapsed".
     This used to convert here, through int64_t.  The reasoning was right and
     the location was not: owning a cast locally is what let a second one
     grow beside it in this same file, and that one carried a live defect.
     See nco_phase_units_mod. */
  double q = upsample ? 4294967296.0 / rate : rate * 4294967296.0;
  return nco_phase_units_mod (q);
}

static resamp_state_t *
resamp_create_from_bank (size_t num_phases, size_t num_taps, float *bank_owned,
                         double rate)
{
  resamp_state_t *s = calloc (1, sizeof (*s));
  if (!s)
    {
      free (bank_owned);
      return NULL;
    }

  s->rate        = rate;
  s->num_phases  = num_phases;
  s->num_taps    = num_taps;
  s->log2_phases = resamp_log2_u (num_phases);
  s->upsample    = (rate >= 1.0);
  s->bank        = bank_owned;
  s->phase       = 0;
  s->phase_inc   = resamp_step_inc (rate, s->upsample);
  s->ctrl_phase  = 0;
  s->ctrl_debt   = 0;
  s->ctrl_ahead  = 0;

  /* delay line: power-of-2 dual buffer */
  s->delay_cap = 1;
  while (s->delay_cap < num_taps)
    s->delay_cap <<= 1;
  s->delay_mask = s->delay_cap - 1;
  s->delay_head = 0;
  s->delay_buf  = calloc (2 * s->delay_cap, sizeof (float _Complex));
  if (!s->delay_buf)
    {
      free (s->bank);
      free (s);
      return NULL;
    }

  s->decim_iad = calloc (num_taps, sizeof (float _Complex));
  s->decim_tfd
      = calloc (num_taps > 1 ? num_taps - 1 : 1, sizeof (float _Complex));
  if (!s->decim_iad || !s->decim_tfd)
    {
      free (s->decim_tfd);
      free (s->decim_iad);
      free (s->delay_buf);
      free (s->bank);
      free (s);
      return NULL;
    }

  return s;
}

/* ------------------------------------------------------------------ */
/* Public lifecycle                                                    */
/* ------------------------------------------------------------------ */

resamp_state_t *
resamp_create (double rate)
{
  static const size_t NUM_PHASES = 4096;
  static const double ATTEN      = 60.0;
  static const double PB         = 0.4;
  static const double SB         = 0.6;

  size_t num_taps = resamp_kaiser_num_taps (NUM_PHASES, ATTEN, PB, SB);
  float *bank     = resamp_build_bank (NUM_PHASES, num_taps, ATTEN, PB, SB);
  if (!bank)
    return NULL;
  return resamp_create_from_bank (NUM_PHASES, num_taps, bank, rate);
}

resamp_state_t *
resamp_create_custom (size_t num_phases, size_t num_taps, const float *bank,
                      double rate)
{
  if (!num_phases || !num_taps || !bank || rate <= 0.0)
    return NULL;

  size_t len = num_phases * num_taps;
  float *b   = malloc (len * sizeof (float));
  if (!b)
    return NULL;
  memcpy (b, bank, len * sizeof (float));
  return resamp_create_from_bank (num_phases, num_taps, b, rate);
}

void
resamp_destroy (resamp_state_t *s)
{
  if (!s)
    return;
  free (s->decim_tfd);
  free (s->decim_iad);
  free (s->delay_buf);
  free (s->bank);
  free (s);
}

void
resamp_reset (resamp_state_t *s)
{
  s->phase      = 0;
  s->ctrl_phase = 0;
  s->ctrl_debt  = 0;
  s->ctrl_ahead = 0;
  s->delay_head = 0;
  memset (s->delay_buf, 0, 2 * s->delay_cap * sizeof (float _Complex));
  memset (s->decim_iad, 0, s->num_taps * sizeof (float _Complex));
  if (s->num_taps > 1)
    memset (s->decim_tfd, 0, (s->num_taps - 1) * sizeof (float _Complex));
}

/* ── Serializable state — standard envelope (see dp_state.h) ─────────────────
 * Order: phase, delay_head, ctrl phase, debt, ahead, then delay_buf
 * decim_iad (num_taps), decim_tfd (num_taps-1 when num_taps>1). */

size_t
resamp_state_bytes (const resamp_state_t *s)
{
  size_t b = sizeof (dp_state_hdr_t) + sizeof (uint32_t) + sizeof (size_t)
             + 3 * sizeof (uint32_t)
             + 2 * s->delay_cap * sizeof (float _Complex)
             + s->num_taps * sizeof (float _Complex);
  if (s->num_taps > 1)
    b += (s->num_taps - 1) * sizeof (float _Complex);
  return b;
}

void
resamp_get_state (const resamp_state_t *s, void *blob)
{
  dp_writer_t w = dp_writer_init (blob, resamp_state_bytes (s));
  dp_w_hdr (&w, RESAMP_STATE_MAGIC, RESAMP_STATE_VERSION,
            resamp_state_bytes (s));
  dp_w_u32 (&w, s->phase);
  dp_w_bytes (&w, &s->delay_head, sizeof (size_t));
  dp_w_u32 (&w, s->ctrl_phase);
  dp_w_u32 (&w, s->ctrl_debt);
  dp_w_u32 (&w, s->ctrl_ahead);
  dp_w_cf32 (&w, s->delay_buf, 2 * s->delay_cap);
  dp_w_cf32 (&w, s->decim_iad, s->num_taps);
  if (s->num_taps > 1)
    dp_w_cf32 (&w, s->decim_tfd, s->num_taps - 1);
}

int
resamp_set_state (resamp_state_t *s, const void *blob)
{
  int rc = dp_state_validate (blob, resamp_state_bytes (s), RESAMP_STATE_MAGIC,
                              RESAMP_STATE_VERSION);
  if (rc != DP_OK)
    return rc;
  dp_reader_t r = dp_reader_init (blob, resamp_state_bytes (s));
  r.off         = sizeof (dp_state_hdr_t);
  s->phase      = dp_r_u32 (&r);
  dp_r_bytes (&r, &s->delay_head, sizeof (size_t));
  s->ctrl_phase = dp_r_u32 (&r);
  s->ctrl_debt  = dp_r_u32 (&r);
  s->ctrl_ahead = dp_r_u32 (&r);
  dp_r_cf32 (&r, s->delay_buf, 2 * s->delay_cap);
  dp_r_cf32 (&r, s->decim_iad, s->num_taps);
  if (s->num_taps > 1)
    dp_r_cf32 (&r, s->decim_tfd, s->num_taps - 1);
  return DP_OK;
}

/* ------------------------------------------------------------------ */
/* Properties                                                          */
/* ------------------------------------------------------------------ */

double
resamp_get_rate (const resamp_state_t *s)
{
  return s->rate;
}

void
resamp_set_rate (resamp_state_t *s, double rate)
{
  s->rate      = rate;
  s->upsample  = (rate >= 1.0);
  s->phase_inc = resamp_step_inc (rate, s->upsample);
}

size_t
resamp_get_num_phases (const resamp_state_t *s)
{
  return s->num_phases;
}

size_t
resamp_get_num_taps (const resamp_state_t *s)
{
  return s->num_taps;
}

double
resamp_dc_gain (const resamp_state_t *s)
{
  double sum = 0.0;
  for (size_t t = 0; t < s->num_taps; t++)
    sum += (double)s->bank[t];
  return sum;
}

double
resamp_get_ctrl_acc (const resamp_state_t *s)
{
  /* The phase word as a fraction of one input interval: in [0, 1) by
     construction, which is what this accessor has always promised, and
     which is exactly nco_word_to_norm's range. */
  return nco_word_to_norm (s->ctrl_phase);
}

/* ------------------------------------------------------------------ */
/* Scalar dot product: Σ w[j] × h[j], CF32 × F32                     */
/* ------------------------------------------------------------------ */

static inline float _Complex dot_cf32 (const float _Complex *w, const float *h,
                                       size_t n)
{
  float si = 0.0f, sq = 0.0f;
  for (size_t j = 0; j < n; j++)
    {
      si += crealf (w[j]) * h[j];
      sq += cimagf (w[j]) * h[j];
    }
  return CMPLXF (si, sq);
}

/* ------------------------------------------------------------------ */
/* Dual-buffer delay line helpers                                      */
/* ------------------------------------------------------------------ */

static inline void
dl_push (resamp_state_t *s, float _Complex x)
{
  s->delay_head               = (s->delay_head - 1) & s->delay_mask;
  s->delay_buf[s->delay_head] = x;
  s->delay_buf[s->delay_head + s->delay_cap] = x;
}

static inline const float _Complex *
dl_ptr (const resamp_state_t *s)
{
  return &s->delay_buf[s->delay_head];
}

static inline const float *
get_branch (const resamp_state_t *s, uint32_t ph)
{
  /* num_phases == 1 gives log2_phases == 0, and `ph >> 32` on a uint32_t is
     UNDEFINED (C99 6.5.7p3) -- x86 masks the count to 5 bits, so it
     evaluates to `ph` itself and indexes bank[ph * num_taps], a wild read
     far outside a one-arm bank. A single-phase bank has exactly one arm, so
     the phase selects nothing and the answer is 0 by construction.

     This was reachable but masked: the only single-phase user is
     wfm_synth's polyphase RRC shaper at sps == 1, whose rate is then
     exactly 1.0 -- and the phase_inc conversion at that rate was ITSELF
     undefined, yielding 0 on x86, which pinned `ph` at 0 and made the bad
     shift return a harmless 0. One undefined conversion was hiding the
     other, so fixing either alone turns a silently dead waveform into a
     segfault. */
  size_t arm = s->log2_phases ? (size_t)(ph >> (32u - s->log2_phases)) : 0u;
  return &s->bank[arm * s->num_taps];
}

/* ------------------------------------------------------------------ */
/* Interpolation path — output-driven                                  */
/* One NCO tick per output sample; overflow pushes next input.        */
/* ------------------------------------------------------------------ */

static size_t
interp_execute (resamp_state_t *s, const float _Complex *in, size_t num_in,
                float _Complex *out, size_t max_out)
{
  size_t   xi = 0, oi = 0;
  uint32_t ph = s->phase, inc = s->phase_inc;

  while (xi < num_in && oi < max_out)
    {
      out[oi++] = dot_cf32 (dl_ptr (s), get_branch (s, ph), s->num_taps);
      /* Emit at every tick; LOAD when the accumulator fails to advance.
         `u(k) <= u(k-1)` -- the carry-out test this replaces was `<`, and
         so missed the equality case, which is exactly rate == 1. */
      uint32_t new_ph = ph + inc;
      if (new_ph <= ph)
        dl_push (s, in[xi++]);
      ph = new_ph;
    }
  s->phase = ph;
  return oi;
}

/* ------------------------------------------------------------------ */
/* Streaming interpolation — output-count driven                       */
/* Same per-output kernel as interp_execute, but it always emits       */
/* max_out outputs (no "input exhausted" early exit), so a producer     */
/* can feed exactly the inputs the overflow count requires.            */
/* ------------------------------------------------------------------ */

size_t
resamp_interp_inputs_needed (const resamp_state_t *s, size_t max_out)
{
  /* Overflows in max_out ticks from the current phase: the high 32 bits of
     (phase + max_out * phase_inc), computed in 64-bit so it can't wrap. */
  /* inc == 0 is rate 1: the accumulator never advances, so every tick
     loads, and the closed form below would wrongly say none do. */
  if (s->phase_inc == 0u)
    return max_out;
  uint64_t end
      = (uint64_t)s->phase + (uint64_t)max_out * (uint64_t)s->phase_inc;
  return (size_t)(end >> 32);
}

size_t
resamp_interp_fill (resamp_state_t *s, const float _Complex *in,
                    float _Complex *out, size_t max_out)
{
  size_t   xi = 0;
  uint32_t ph = s->phase, inc = s->phase_inc;

  for (size_t oi = 0; oi < max_out; oi++)
    {
      out[oi]         = dot_cf32 (dl_ptr (s), get_branch (s, ph), s->num_taps);
      uint32_t new_ph = ph + inc; /* load on u(k) <= u(k-1) */
      if (new_ph <= ph)
        dl_push (s, in[xi++]);
      ph = new_ph;
    }
  s->phase = ph;
  return xi;
}

/* ------------------------------------------------------------------ */
/* Decimation path — input-driven, transposed polyphase form          */
/*                                                                    */
/* Mirrors the reference Python _decimate():                          */
/*   For each input sample:                                           */
/*     1. Select polyphase arm from the pre-advance phase.            */
/*     2. Accumulate x[n] × bank[arm][N-1-t] into iad[t] for every t.*/
/*        Taps are reversed so iad[0] feeds the current output slot.  */
/*        Coefficients are pre-scaled by rate for unity passband gain. */
/*     3. Advance NCO.                                                */
/*     4. On overflow: dump I&D accumulators through the transposed   */
/*        tapped delay line and emit one output.                      */
/* ------------------------------------------------------------------ */

static size_t
decim_execute (resamp_state_t *s, const float _Complex *in, size_t num_in,
               float _Complex *out, size_t max_out)
{
  size_t          oi = 0;
  size_t          N  = s->num_taps;
  uint32_t        ph = s->phase, inc = s->phase_inc;
  float           scale = (float)s->rate;
  float _Complex *iad   = s->decim_iad;
  float _Complex *tfd   = s->decim_tfd;

  for (size_t xi = 0; xi < num_in && oi < max_out; xi++)
    {
      /* 1. Accumulate x[n] × reversed_h into integrate-and-dump */
      const float *h    = get_branch (s, ph);
      float _Complex xv = in[xi] * scale;
      for (size_t t = 0; t < N; t++)
        iad[t] += xv * h[N - 1 - t];

      /* 2. Advance NCO */
      /* The dual side of the same rule: the step is output periods per
         INPUT, and an output is due when the accumulator fails to advance.
         inc > 0 here because this path only runs for rate < 1. */
      uint32_t new_ph = ph + inc;
      if (new_ph <= ph)
        {
          /* 3. Dump I&D through transposed tapped delay line */
          float _Complex y = iad[0] + (N > 1 ? tfd[0] : 0.0f);
          for (size_t t = 1; t + 1 < N; t++)
            tfd[t - 1] = iad[t] + tfd[t];
          if (N > 1)
            tfd[N - 2] = iad[N - 1];
          memset (iad, 0, N * sizeof (*iad));
          out[oi++] = y;
        }
      ph = new_ph;
    }
  s->phase = ph;
  return oi;
}

/* ------------------------------------------------------------------ */
/* Public execute — dispatches on upsample flag                       */
/* ------------------------------------------------------------------ */

size_t
resamp_execute (resamp_state_t *s, const float _Complex *in, size_t num_in,
                float _Complex *out, size_t max_out)
{
  if (s->upsample)
    return interp_execute (s, in, num_in, out, max_out);
  return decim_execute (s, in, num_in, out, max_out);
}

/* ------------------------------------------------------------------ */
/* execute_ctrl_push — the same rule, with the rate steered per input  */
/* ------------------------------------------------------------------ */
/*
 * The control port rides the INTERPOLATING structure — dl_push() plus a dot
 * product over the delay line — because that is the only one of the two
 * forms that can be steered through unity in both directions, which is what
 * closing a timing loop requires.  So it obeys the interpolator's rule:
 * emit at every tick, load when the accumulator fails to advance.
 *
 * One thing differs from the free-running path, and only one.  There the
 * rate is fixed and at or above unity, so a tick loads at most one input
 * and the 32-bit `u(k) <= u(k-1)` says everything.  Here the steered rate
 * may fall BELOW unity, and a tick then spans more than one input — so the
 * whole intervals are counted in ctrl_debt and the fraction stays in
 * ctrl_phase.  Together they are the same quantity the single phase word
 * carries above, split only because a uint32 cannot hold a value >= 1.
 *
 * The debt is also, for free, what defers an output the caller had no room
 * for: clamping a count to max_out and walking away would destroy it, and a
 * lost output is a strobe-parity shift downstream.
 *
 * What this replaces was `acc += R; while (acc >= 1) acc -= 1` — the
 * DECIMATOR's accumulator on the interpolator's structure.  That is exact
 * only at R == 1, where the two are the same recurrence, which is why unity
 * looked healthy while every other rate carried a sawtooth timing error.
 * Measured as tone purity (a resampled pure tone must still be a pure
 * tone): -12 to -17 dB before, -71 to -75 dB after.
 *
 * Only the real part of ctrl[] is used.
 */

size_t
resamp_execute_ctrl_push (resamp_state_t *s, float _Complex x, double ctrl,
                          float _Complex *out, size_t max_out)
{
  /* NOTE the sample is NOT pushed here.  Nothing enters an interpolator's
     delay line without a load request: a tick emits, the accumulator fails
     to advance, and only THEN is an input consumed.  Pushing on entry --
     which this function used to do -- is an unrequested load, and it showed
     up as exactly one sample of group delay between this entry point and
     the block form, which loads only inside its load branch. */
  double delta = s->rate + ctrl;
  if (!(delta > RESAMP_CTRL_RATE_MIN))
    delta = RESAMP_CTRL_RATE_MIN;

  /* Input intervals per output, split into whole and fractional parts by
     ONE conversion.  Folding two quantities separately and adding the words
     truncates twice, which at a composite of exactly 1.0 sums to 2^32-1 and
     reads as a completed period that never happened. */
  double t_in  = 1.0 / delta;
  double whole = floor (t_in);

  /* The fraction of an input interval is a PHASE, so it converts where every
     other double-valued phase in the library converts. This site used to
     hold its own `(uint32_t)(frac_part * 2^32 + 0.5)`, and the private copy
     had both failure modes nco_core.h warns about.
     It ROUNDED where the library truncates -- the exact drift that made the
     conversion canonical in the first place -- and the rounding could reach
     a full period: with the fractional part within 0.5/2^32 of 1.0 the
     `+ 0.5` carries the product to 2^32, whose cast to uint32_t is undefined
     (C99 6.3.1.4) and on x86 lands as 0. Read back, 0 said "no fraction"
     while floor(t_in) said "no whole interval" either, so ctrl_debt came out
     0, no input was ever consumed, and the call emitted max_out copies of
     one sample off an unchanged delay line.
     Folding into [0, 1) and truncating cannot reach 2^32 at all, so the
     boundary stops being a case to handle. Reachable, not theoretical: the
     window was delta in (1.0, 1.0 + ~1.16e-10], and a Doppler ramp through
     zero sweeps a receiver's composite rate straight across it -- which is
     what cost async_dsss_receiver_spec_demo its lock at closest approach. */
  uint32_t frac = nco_norm_phase_to_word (t_in);
  uint32_t skip = whole >= 4294967295.0 ? 0xFFFFFFFFu : (uint32_t)whole;

  int    offered = 1; /* the caller's sample, not yet loaded */
  size_t n       = 0;

  while (offered && n < max_out)
    {
      if (s->ctrl_debt == 0u)
        {
          /* The arm IS the accumulator: a fraction of one INPUT interval,
             which is what a polyphase arm indexes, and in [0, 1) by
             construction -- so it cannot leave the bank and no clamp is
             reachable.  The saturating `arm >= num_phases` guard this
             replaces was a symptom of running the decimator's accumulator
             here, not of a range that needed guarding. */
          out[n++] = dot_cf32 (dl_ptr (s), get_branch (s, s->ctrl_phase),
                               s->num_taps);
          uint32_t new_ph = s->ctrl_phase + frac;
          /* STRICT wrap, unlike the free-running path's `<=`: there a whole
             period is encoded as an increment of 0 and equality decodes it,
             whereas here the whole intervals are already in `skip`, so
             counting equality too would count them twice -- which is
             exactly what an integer T (rate 1, 1/2, 1/3 ...) hits. */
          s->ctrl_debt  = skip + (new_ph < s->ctrl_phase ? 1u : 0u);
          s->ctrl_phase = new_ph;
        }
      if (s->ctrl_debt)
        {
          if (s->ctrl_ahead) /* satisfy from a sample already loaded */
            {
              s->ctrl_ahead--;
              s->ctrl_debt--;
            }
          else
            {
              dl_push (s, x);
              offered = 0;
              s->ctrl_debt--;
            }
        }
    }

  /* max_out ended the call before any tick asked for the sample.  The API
     cannot decline it, so load it and remember that it arrived unrequested;
     the next request is served from here.  This is also what defers an
     output the caller had no room for -- clamping a count and walking away
     would destroy it, and a lost output is a strobe-parity shift. */
  if (offered)
    {
      dl_push (s, x);
      s->ctrl_ahead++;
    }
  return n;
}

/* ------------------------------------------------------------------ */
/* execute_ctrl — the block form, a loop over the push form            */
/* so the two cannot drift.  ctrl is real double, as the push form takes. */
/* ------------------------------------------------------------------ */

size_t
resamp_execute_ctrl (resamp_state_t *s, const float _Complex *in,
                     const double *ctrl, size_t num_in, float _Complex *out,
                     size_t max_out)
{
  size_t oi = 0;
  for (size_t xi = 0; xi < num_in && oi < max_out; xi++)
    oi += resamp_execute_ctrl_push (s, in[xi], ctrl[xi], out + oi,
                                    max_out - oi);
  return oi;
}
