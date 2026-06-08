#include "resamp/resamp_core.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* Kaiser bank builder                                                 */
/* ------------------------------------------------------------------ */

static double
_bessel_i0 (double x)
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
_kaiser_beta (double atten)
{
  if (atten > 50.0)
    return 0.1102 * (atten - 8.7);
  if (atten >= 21.0)
    return 0.5842 * pow (atten - 21.0, 0.4) + 0.07886 * (atten - 21.0);
  return 0.0;
}

static unsigned
_log2_u (size_t v)
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
_build_bank (size_t num_phases, size_t num_taps, double atten, double pb,
             double sb)
{
  double beta  = _kaiser_beta (atten);
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

  double b0 = _bessel_i0 (beta);
  for (size_t i = 0; i < proto; i++)
    {
      double m   = (double)i - halflen;
      double mid = (double)(proto - 1) * 0.5;
      double u   = 2.0 * ((double)i - mid) / (double)(proto - 1);
      double w   = _bessel_i0 (beta * sqrt (1.0 - u * u)) / b0;
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
_kaiser_num_taps (size_t num_phases, double atten, double pb, double sb)
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

static resamp_state_t *
_create_from_bank (size_t num_phases, size_t num_taps, float *bank_owned,
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
  s->log2_phases = _log2_u (num_phases);
  s->upsample    = (rate >= 1.0);
  s->bank        = bank_owned;
  s->phase       = 0;
  s->phase_inc   = s->upsample ? (uint32_t)(4294967296.0 / rate)
                               : (uint32_t)(rate * 4294967296.0);
  s->ctrl_acc    = 0.0;

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

  size_t num_taps = _kaiser_num_taps (NUM_PHASES, ATTEN, PB, SB);
  float *bank     = _build_bank (NUM_PHASES, num_taps, ATTEN, PB, SB);
  if (!bank)
    return NULL;
  return _create_from_bank (NUM_PHASES, num_taps, bank, rate);
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
  return _create_from_bank (num_phases, num_taps, b, rate);
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
  s->ctrl_acc   = 0.0;
  s->delay_head = 0;
  memset (s->delay_buf, 0, 2 * s->delay_cap * sizeof (float _Complex));
  memset (s->decim_iad, 0, s->num_taps * sizeof (float _Complex));
  if (s->num_taps > 1)
    memset (s->decim_tfd, 0, (s->num_taps - 1) * sizeof (float _Complex));
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
  s->phase_inc = s->upsample ? (uint32_t)(4294967296.0 / rate)
                             : (uint32_t)(rate * 4294967296.0);
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

/* ------------------------------------------------------------------ */
/* Overflow detection                                                  */
/* ------------------------------------------------------------------ */

#if defined(__GNUC__) || defined(__clang__)
#define ADD_OVF(a, b, res)                                                    \
  ((uint8_t)__builtin_add_overflow ((uint32_t)(a), (uint32_t)(b),             \
                                    (uint32_t *)(res)))
#else
static inline uint8_t
_add_ovf (uint32_t a, uint32_t b, uint32_t *res)
{
  *res = a + b;
  return (uint8_t)(*res < a);
}
#define ADD_OVF(a, b, res) _add_ovf ((a), (b), (res))
#endif

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
  size_t arm = ph >> (32u - s->log2_phases);
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
      uint32_t new_ph;
      if (ADD_OVF (ph, inc, &new_ph))
        dl_push (s, in[xi++]);
      ph = new_ph;
    }
  s->phase = ph;
  return oi;
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
      uint32_t new_ph;
      if (ADD_OVF (ph, inc, &new_ph))
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
  if (s->rate == 1.0)
    {
      size_t n = num_in < max_out ? num_in : max_out;
      memcpy (out, in, n * sizeof (float _Complex));
      return n;
    }
  if (s->upsample)
    return interp_execute (s, in, num_in, out, max_out);
  return decim_execute (s, in, num_in, out, max_out);
}

/* ------------------------------------------------------------------ */
/* execute_ctrl — unified input-driven, double-precision accumulator  */
/* ------------------------------------------------------------------ */
/*
 * For each input sample:
 *   1. Push into delay line.
 *   2. Advance acc by (rate + crealf(ctrl[i])).
 *   3. For each full period accumulated (acc >= 1.0), emit one output
 *      using the polyphase arm computed from the fractional remainder.
 *
 * Works for all rates including interpolation (rate > 1, multiple
 * outputs per input) and decimation (rate < 1, outputs every N inputs).
 * Only the real part of ctrl[] is used.
 */

size_t
resamp_execute_ctrl (resamp_state_t *s, const float _Complex *in,
                     const float _Complex *ctrl, size_t num_in,
                     float _Complex *out, size_t max_out)
{
  size_t oi  = 0;
  double acc = s->ctrl_acc;

  for (size_t xi = 0; xi < num_in && oi < max_out; xi++)
    {
      dl_push (s, in[xi]);
      acc += s->rate + (double)crealf (ctrl[xi]);

      while (acc >= 1.0 && oi < max_out)
        {
          acc -= 1.0;
          size_t arm = (size_t)(acc * (double)s->num_phases);
          if (arm >= s->num_phases)
            arm = s->num_phases - 1;
          out[oi++] = dot_cf32 (dl_ptr (s), &s->bank[arm * s->num_taps],
                                s->num_taps);
        }
    }
  s->ctrl_acc = acc;
  return oi;
}
