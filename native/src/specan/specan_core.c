/**
 * @file specan_core.c
 * @brief Specan implementation — DDC tuner/decimator + averaging-PSD display.
 *
 * See specan_core.h for the design.  This file owns only the natural-parameter
 * arithmetic (RBW → window length + Kaiser beta, span → decimation rate, the
 * display crop) and the per-call plumbing between the Ddc and the PSD core;
 * the signal processing itself lives in those composed objects.
 */
#include "doppler/specan/specan_core.h"
#include "doppler/util/util_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* The design (docs/design/specan.md), in three rules:
 *
 * 1. SPAN SETS THE RATE. fs_out = 1.28 * span, so ±span/2 = ±fs_out/2.56
 *    falls exactly on bin ±nfft/2.56 -- an integer for every power-of-two
 *    nfft >= 256 -- and inside the DDC passband (±0.4 * fs_out). A span the
 *    input cannot supply (1.28 * span > fs) is clamped to fs / 1.28 rather
 *    than kept with fs_out = fs, which would put its edges between bins.
 *
 * 2. THE WINDOW IS A POWER OF TWO, n, and the transform is
 *    nfft = max(n, SPECAN_NFFT_MIN): zero padding only when the window is
 *    shorter than the 512-point floor the display needs (401 bins). The base
 *    RBW, 2 * fs_out / n, is the narrowest a given n offers -- a Kaiser ENBW
 *    of 2 bins, beta ~12, peak sidelobe ~-90 dB. n is the SMALLEST power of
 *    two whose base RBW does not exceed the request (any larger one also
 *    fits, with a wider-than-needed ENBW), and at least SPECAN_N_MIN.
 *
 * 3. BETA WIDENS THE ENBW to meet the request: target = rbw * n / fs_out, in
 *    [2, 4), and beta = kaiser_beta_for_enbw(target, n). The widest RBW is
 *    4 bins of the shortest window, 4 * fs_out / SPECAN_N_MIN; a wider
 *    request is clamped to it. Auto is span / 100.
 *
 * Every RBW therefore gets beta >= ~12. The rule this replaces chose the
 * SMALLEST power of two >= fs_out / rbw, leaving a target in [1, 2): an RBW
 * of fs_out / 2^k asked for exactly 1 bin, which only a rectangle (beta 0,
 * -13 dB sidelobes) meets, and the specan demo sat exactly there. */
#define SPECAN_OVERSAMPLE 1.28
#define SPECAN_NFFT_MIN 512u
/* The shortest window. Below 16 the beta fit stops holding (7% at n = 8). */
#define SPECAN_N_MIN 16u
#define SPECAN_ENBW_MIN 2.0
#define SPECAN_ENBW_MAX 4.0
/* dp_psd_create's window index for Kaiser (its header: 0 Hann, 1 Kaiser,
 * 2 Blackman-Harris). The analyzer always uses Kaiser: beta is its RBW knob.
 */
#define PSD_WINDOW_KAISER 1
#define SPECAN_EPS 1e-20f

/* Kaiser beta whose ENBW, for an n-point window, is `enbw` bins (2..4).
 *
 * A cubic least-squares fit of beta against ENBW, from np.kaiser(4096, beta)
 * over beta in [10, 55] restricted to ENBW in [1.98, 4.02]. A symmetric
 * n-point window spans n - 1 sample intervals, so its ENBW is the long-window
 * value times n / (n - 1); the fit is evaluated at enbw * (n - 1) / n to
 * undo that. The realised ENBW is then within 0.03% of the target for every
 * n >= 16 (uncorrected it was 0.18% at 512 and 6.7% at 16). The analyzer
 * reports the RBW the window actually realises, so this sets how close that
 * lands to the request, not what is reported -- test_specan_core.c bounds
 * it. */
static double
kaiser_beta_for_enbw (double enbw, size_t n)
{
  double e = enbw * (double)(n - 1) / (double)n;
  return ((0.00590559 * e + 3.07532701) * e + 0.24521102) * e - 0.960144;
}

dp_specan_state_t *
dp_specan_create (double fs, double span, double rbw, double src_center,
                  double center, double offset_db, double full_scale,
                  size_t bits, size_t navg)
{
  if (fs <= 0.0 || span < 0.0 || rbw < 0.0 || navg < 1)
    return NULL;

  /* 1. Span sets the rate; clamp a span the input cannot supply. 0 is auto:
   * the widest span there is, the whole input band. */
  if (span == 0.0 || span * SPECAN_OVERSAMPLE > fs)
    span = fs / SPECAN_OVERSAMPLE;
  double fs_out = span * SPECAN_OVERSAMPLE;
  /* 0 is auto: span / 100, which is also the widest RBW (rule 3). */
  if (rbw == 0.0)
    rbw = span / 100.0;

  /* 2. The window: smallest power of two whose narrowest RBW fits the
   * request. The 1e-9 keeps an exact power of two (1000 Hz at 256 kHz is
   * 512.0) from being pushed to the next by rounding in the division. The
   * transform pads it to the display's floor only when it is shorter. */
  size_t n
      = dp_next_pow_two ((size_t)ceil (SPECAN_ENBW_MIN * fs_out / rbw - 1e-9));
  if (n < SPECAN_N_MIN)
    n = SPECAN_N_MIN;
  size_t nfft = n < SPECAN_NFFT_MIN ? SPECAN_NFFT_MIN : n;

  /* 3. Beta widens the ENBW to the request, up to 4 bins of the window. */
  double target = rbw * (double)n / fs_out;
  if (target < SPECAN_ENBW_MIN)
    target = SPECAN_ENBW_MIN;
  if (target > SPECAN_ENBW_MAX)
    target = SPECAN_ENBW_MAX;
  double beta = kaiser_beta_for_enbw (target, n);

  /* The display crop: bins ±nfft/2.56 are the span edges, exactly. */
  size_t half = (size_t)lround ((double)nfft / 2.56);

  dp_specan_state_t *s = calloc (1, sizeof *s);
  if (!s)
    return NULL;

  double rate = fs_out / fs;
  /* Mix the carrier at `center` (relative to src_center) DOWN to DC: the Ddc
   * shifts content up by +norm_freq, so a positive offset needs a negative LO
   * (matching ddc_core's "norm_freq = -f_carrier shifts f_carrier to DC"). */
  double norm_freq = -(center - src_center) / fs;
  s->ddc           = dp_ddc_create (norm_freq, rate);
  /* The PSD core owns the 0-dBFS reference (full_scale / bits); the display
   * reads it back as s->psd->full_scale, so dBFS is single-sourced. */
  s->psd = dp_psd_create (n, fs_out, PSD_WINDOW_KAISER, (float)beta, nfft / n,
                          full_scale, bits, 0, 0.1);
  s->pwr = malloc (nfft * sizeof *s->pwr);
  if (!s->ddc || !s->psd || !s->pwr)
    {
      dp_specan_destroy (s);
      return NULL;
    }

  s->fs_in      = fs;
  s->src_center = src_center;
  s->center     = center;
  s->span       = span;
  s->rbw        = s->psd->enbw * fs_out / (double)n; /* realised */
  s->offset_db  = offset_db;
  s->fs_out     = fs_out;
  s->beta       = beta;
  s->n          = n;
  s->nfft       = nfft;
  s->navg       = navg;
  s->disp_n     = 2 * half + 1;
  s->disp_lo    = nfft / 2 - half;
  return s;
}

void
dp_specan_destroy (dp_specan_state_t *state)
{
  if (!state)
    return;
  if (state->ddc)
    dp_ddc_destroy (state->ddc);
  if (state->psd)
    dp_psd_destroy (state->psd);
  free (state->scratch);
  free (state->pend);
  free (state->pwr);
  free (state);
}

void
dp_specan_reset (dp_specan_state_t *state)
{
  dp_ddc_reset (state->ddc);
  dp_psd_reset (state->psd);
  state->pend_len = 0;
}

/* Serializable state — ddc + psd children + the pending decimated samples.
 * `pend` grows lazily but never holds more than need = n*navg before a frame
 * drains, so it serializes at that fixed capacity (zero-padded → canonical);
 * display/rate fields are config (restored by create). */
size_t
dp_specan_state_bytes (const dp_specan_state_t *s)
{
  return sizeof (dp_state_hdr_t) + dp_ddc_state_bytes (s->ddc)
         + dp_psd_state_bytes (s->psd) + sizeof (uint64_t)
         + s->n * s->navg * sizeof (float _Complex);
}

void
dp_specan_get_state (const dp_specan_state_t *s, void *blob)
{
  DP_GET_OPEN (SPECAN_STATE_MAGIC, SPECAN_STATE_VERSION,
               dp_specan_state_bytes (s));
  DP_W_CHILD (&_w, dp_ddc, s->ddc);
  DP_W_CHILD (&_w, dp_psd, s->psd);
  dp_w_u64 (&_w, s->pend_len);
  dp_w_cf32 (&_w, s->pend, s->pend_len);
  for (size_t i = s->pend_len; i < s->n * s->navg; i++)
    dp_w_u64 (&_w, 0); /* zero-pad to the fixed capacity */
}

int
dp_specan_set_state (dp_specan_state_t *s, const void *blob)
{
  DP_SET_OPEN (SPECAN_STATE_MAGIC, SPECAN_STATE_VERSION,
               dp_specan_state_bytes (s));
  DP_R_CHILD (&_r, dp_ddc, s->ddc);
  DP_R_CHILD (&_r, dp_psd, s->psd);
  size_t need     = s->n * s->navg;
  size_t pend_len = (size_t)dp_r_u64 (&_r);
  if (pend_len > need)
    return DP_ERR_INVALID;
  if (s->pend_cap < pend_len)
    {
      float _Complex *p = realloc (s->pend, need * sizeof *p);
      if (!p)
        return DP_ERR_INVALID;
      s->pend     = p;
      s->pend_cap = need;
    }
  dp_r_cf32 (&_r, s->pend, pend_len);
  s->pend_len = pend_len;
  return DP_OK;
}

size_t
dp_specan_execute_max_out (dp_specan_state_t *state)
{
  return state->disp_n;
}

size_t
dp_specan_execute (dp_specan_state_t *state, const float _Complex *x,
                   size_t x_len, float *out, size_t max_out)
{
  /* Mix to DC and decimate; output length <= x_len since rate <= 1. */
  if (state->scratch_cap < x_len)
    {
      float _Complex *p = realloc (state->scratch, x_len * sizeof *p);
      if (!p)
        return 0;
      state->scratch     = p;
      state->scratch_cap = x_len;
    }
  size_t m = dp_ddc_execute (state->ddc, x, x_len, state->scratch,
                             state->scratch_cap);

  /* Buffer the decimated samples until a full averaging window is available.
   */
  size_t need = state->n * state->navg;
  if (state->pend_len + m > state->pend_cap)
    {
      size_t cap = state->pend_len + m;
      if (cap < need)
        cap = need;
      float _Complex *p = realloc (state->pend, cap * sizeof *p);
      if (!p)
        return 0;
      state->pend     = p;
      state->pend_cap = cap;
    }
  memcpy (state->pend + state->pend_len, state->scratch,
          m * sizeof *state->scratch);
  state->pend_len += m;

  if (state->pend_len < need)
    return 0;

  /* Consume EVERY complete window, not just the first.
   *
   * One call returns one spectrum, so when the caller hands over more than a
   * window's worth at a time this used to emit the OLDEST window and keep the
   * rest buffered forever. `pend_len` then grew without bound: feeding 4096
   * samples repeatedly at this config decimates to 524 and consumes 256, so
   * every call netted +268 and nothing ever gave it back. Measured over eight
   * calls -- 268, 536, 804, 1073, 1341, 1609, 1878, 2146 -- and it does not
   * turn around. Three consequences, in order of how badly they bite:
   *
   * 1. `dp_specan_state_bytes` reserves `n*navg` samples for `pend`, because
   * the state protocol makes the blob size a CONFIG fingerprint. So once
   *    pend_len passed 256, `dp_specan_get_state` wrote past the end of the
   *    caller's buffer -- 17 KB into a 2 KB reservation by the eighth call.
   * 2. The analyzer leaked ~2 KB per call, forever.
   * 3. The spectrum returned was computed from ever-staler samples, so a
   *    display fell further behind real time the longer it ran.
   *
   * Skipping to the newest complete window fixes all three and costs one
   * psd pass, not `frames` of them: the intermediate windows would each have
   * been overwritten by the next `dp_psd_reset` anyway. A display that cannot
   * keep up shows the LATEST frame; it does not queue history it will never
   * catch up on. That the buffer is now bounded by `need` is what makes the
   * fixed-size state blob correct, so the serialization round-trip that
   * exposed this passes as a consequence rather than by its own patch. */
  size_t frames = state->pend_len / need;
  dp_psd_reset (state->psd);
  dp_psd_accumulate (state->psd, state->pend + (frames - 1) * need, need);
  state->pend_len -= frames * need;
  memmove (state->pend, state->pend + frames * need,
           state->pend_len * sizeof *state->pend);

  dp_psd_power_twosided (state->psd, state->nfft, state->pwr, state->nfft);

  /* Crop the central display band and convert to dBFS (+ application offset).
   * The 0-dBFS reference is the PSD core's full_scale (single source). */
  double fs2 = state->psd->full_scale * state->psd->full_scale;
  size_t cnt = state->disp_n;
  if (cnt > max_out)
    cnt = max_out;
  for (size_t i = 0; i < cnt; i++)
    out[i] = 10.0f
                 * log10f (state->pwr[state->disp_lo + i] / (float)fs2
                           + SPECAN_EPS)
             + (float)state->offset_db;
  return cnt;
}

void
dp_specan_retune (dp_specan_state_t *state, double center)
{
  state->center = center;
  dp_ddc_set_norm_freq (state->ddc,
                        -(center - state->src_center) / state->fs_in);
  state->pend_len = 0; /* drop stale-tune samples; next frame is single-tune */
}
