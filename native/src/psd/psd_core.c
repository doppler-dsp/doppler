/**
 * @file psd_core.c
 * @brief PSD — averaging power-spectral-density estimator (Welch's method).
 *
 * Composition over reimplementation: each accumulated frame is windowed, run
 * through the shared ::dp_fft_state_t plan, converted to power, fftshifted to
 * DC-centred order and folded into an ::dp_acc_trace_state_t averager.  The
 * measurement getters read the averaged power back and reuse the spectral free
 * functions (::dp_find_peaks_f32, ::dp_noise_floor_db) for level statistics.
 *
 * Normalisation: dividing |X|^2 by the window coherent gain squared (cg^2)
 * makes a full-scale tone read its true power (psd_db); dividing instead by
 * fs*sum(w^2) yields PSD in dB/Hz (psd_dbhz).  The two differ only by the
 * constant 10*log10(cg^2 / (fs*s2)).  Band-*integrated* power (band_power,
 * total_band_power) is a third case: it normalises by the noise-power gain
 * nfft*s2, not cg^2, so an integrated band is window- and pad-independent —
 * see psd_band_lin.
 */
#include "doppler/psd/psd_core.h"
#include "doppler/spectral/spectral_core.h"
#include "doppler/util/util_core.h"
#include <math.h>
#include <string.h>

/* Power floor (~ -200 dB) guarding log10 of empty / zero bins. */
#define PSD_FLOOR 1e-20

/* Smallest power of two >= x. */

/* ── lifecycle ─────────────────────────────────────────────────────────── */

dp_psd_state_t *
dp_psd_create (size_t n, double fs, int window, float beta, size_t pad,
               double full_scale, size_t bits, int mode, double alpha)
{
  return dp_psd_create_hop (n, fs, window, beta, pad, full_scale, bits, mode,
                            alpha, n);
}

int
dp_psd_kernel_init (dp_psd_kernel_t *k, size_t n, int window, float beta,
                    size_t pad, double full_scale)
{
  /* Zeroed first, so dp_psd_kernel_free is safe whatever this returns. */
  memset (k, 0, sizeof *k);
  /* full_scale divides every reading, so it must be a finite positive
   * number; the negated form also refuses a NaN, which passed a plain
   * `<= 0.0` and built an estimator that read NaN. */
  if (n < 2 || pad < 1 || window < DP_PSD_WINDOW_HANN
      || window > DP_PSD_WINDOW_RECT
      || !(full_scale > 0.0 && isfinite (full_scale))
      || n > ((size_t)-1) / (2 * sizeof (float _Complex)))
    return DP_ERR_INVALID;
  /* Every buffer is sized from n or nfft = next_pow_two(n * pad), the
   * largest nfft complex floats, so n * pad may not pass the largest power
   * of two whose complex buffer has a byte count a size_t holds (2^60 on a
   * 64-bit size_t).  Refused here, before the product is formed and before
   * any allocation: n * pad wrapped -- n = 2, pad = 2^63 read nfft = 1 and
   * a frame of n samples into a 1-sample buffer -- and so did the byte
   * counts, n = 2^62 asking malloc for 0 bytes that the window fill then
   * ran off.  Not left to calloc's own overflow check: ASan and TSan
   * report an overflowing calloc as an error rather than a NULL. */
  const size_t top_pow2 = (((size_t)-1 / sizeof (float _Complex)) >> 1) + 1;
  if (pad > top_pow2 / n)
    return DP_ERR_INVALID;

  /* Caller-sized, so each can still fail, and does return an error: not
   * dp_xmalloc, which is for trusted sizes and would abort instead. */
  const size_t nfft = dp_next_pow_two (n * pad);
  k->n              = n;
  k->nfft           = nfft;
  k->full_scale     = full_scale;
  k->w              = (float *)malloc (n * sizeof (float));
  k->frame = (float _Complex *)malloc (nfft * sizeof (float _Complex));
  k->spec  = (float _Complex *)malloc (nfft * sizeof (float _Complex));
  k->pwr   = (float *)malloc (nfft * sizeof (float));
  if (!k->w || !k->frame || !k->spec || !k->pwr)
    {
      dp_psd_kernel_free (k);
      return DP_ERR_MEMORY;
    }

  if (window == DP_PSD_WINDOW_KAISER)
    dp_kaiser_window (k->w, n, beta);
  else if (window == DP_PSD_WINDOW_BLACKMAN_HARRIS)
    dp_blackman_harris_window (k->w, n);
  else if (window == DP_PSD_WINDOW_RECT)
    for (size_t i = 0; i < n; i++) /* rectangular: no spectral library entry */
      k->w[i] = 1.0f;
  else
    dp_hann_window (k->w, n);

  double cg = 0.0, s2 = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      cg += (double)k->w[i];
      s2 += (double)k->w[i] * (double)k->w[i];
    }
  /* Every reading divides by cg^2 (or s2): a window that sums to zero --
   * the symmetric Hann at n = 2 is [0, 0] -- would read NaN, or the -200 dB
   * floor for any input, so it is refused here (#1911 (f)).  So is one that
   * is not a finite number: a Kaiser beta of NaN, or one large enough that
   * I0 overflows, makes every tap NaN.  Not a threshold on the size:
   * Blackman-Harris at n = 2 sums to 1.2e-4, tiny but valid. */
  if (!(cg > 0.0 && isfinite (cg)))
    {
      dp_psd_kernel_free (k);
      return DP_ERR_INVALID;
    }
  k->cg   = cg;
  k->s2   = s2;
  k->enbw = (double)n * s2 / (cg * cg);

  k->fft = dp_fft_create (nfft, -1, 1);
  if (!k->fft)
    {
      dp_psd_kernel_free (k);
      return DP_ERR_MEMORY;
    }
  return DP_OK;
}

void
dp_psd_kernel_free (dp_psd_kernel_t *k)
{
  if (!k)
    return;
  if (k->fft)
    dp_fft_destroy (k->fft);
  free (k->w);
  free (k->frame);
  free (k->spec);
  free (k->pwr);
  memset (k, 0, sizeof *k);
}

dp_psd_state_t *
dp_psd_create_hop (size_t n, double fs, int window, float beta, size_t pad,
                   double full_scale, size_t bits, int mode, double alpha,
                   size_t hop)
{
  /* The single definition of the dBFS reference: bits>0 selects an ADC
   * full scale (2^(bits-1)); otherwise full_scale is the analog/general ref.
   * bits is a depth an ADC has: past 64 the reference outgrows any sample
   * format, and the cast to int below would be undefined past INT_MAX. */
  if (bits > 64)
    return NULL;
  if (bits > 0)
    full_scale = ldexp (1.0, (int)bits - 1);
  /* fs divides every Hz reading, so it must be a finite positive number;
   * the kernel holds full_scale, n, pad and the window to the same rule. */
  if (!(fs > 0.0 && isfinite (fs)))
    return NULL;
  if (hop == 0)
    hop = n;
  if (hop > n)
    return NULL;
  if (mode < ACC_TRACE_MEAN || mode > ACC_TRACE_MINHOLD)
    return NULL;
  /* alpha is not checked here: its rule is the averager's, and an exp-mode
   * alpha outside (0, 1] makes dp_acc_trace_create, below, return NULL. */

  dp_psd_state_t *s = (dp_psd_state_t *)calloc (1, sizeof (*s));
  if (!s)
    return NULL;
  /* The window, its gains, the scratch and the plan: the kernel's own. */
  if (dp_psd_kernel_init (&s->k, n, window, beta, pad, full_scale) != DP_OK)
    {
      dp_psd_destroy (s);
      return NULL;
    }

  const size_t nfft = s->k.nfft;
  s->fs             = fs;
  s->bits           = bits;
  s->enbw           = s->k.enbw;
  s->dbbuf          = (float *)malloc (nfft * sizeof (float));
  s->avg            = dp_acc_trace_create (nfft, mode, alpha);
  /* The framer owns a ring of two frames: the carry (< n) plus what one feed
   * admits, so a call loops once per frame, not once per sample. Its checks
   * share the allocation check above: a refused one frees the object. */
  s->hop  = hop;
  s->ring = dp_f32_create (2 * n);
  if (!s->dbbuf || !s->avg || !s->ring
      || dp_f32_framer_init (&s->fr, s->ring, n, hop) != DP_OK)
    {
      dp_psd_destroy (s);
      return NULL;
    }
  return s;
}

void
dp_psd_destroy (dp_psd_state_t *state)
{
  if (!state)
    return;
  dp_psd_kernel_free (&state->k);
  if (state->avg)
    dp_acc_trace_destroy (state->avg);
  free (state->dbbuf);
  if (state->ring)
    dp_f32_destroy (state->ring);
  free (state);
}

void
dp_psd_reset (dp_psd_state_t *state)
{
  dp_acc_trace_reset (state->avg);
  dp_f32_framer_reset (&state->fr);
}

/* Serializable state: the power averager, then the framer's snapshot of the
 * carried partial frame. Both children are self-contained sub-blobs with
 * fixed sizes (the framer's is fixed by its frame length, not its fill), so
 * the blob's size is a function of the config, as dp_state_validate requires.
 * Window, FFT plan, scratch, n, nfft and hop are config (create). */
size_t
dp_psd_state_bytes (const dp_psd_state_t *s)
{
  return sizeof (dp_state_hdr_t) + dp_acc_trace_state_bytes (s->avg)
         + dp_f32_framer_state_bytes (&s->fr);
}

void
dp_psd_get_state (const dp_psd_state_t *s, void *blob)
{
  DP_GET_OPEN (PSD_STATE_MAGIC, PSD_STATE_VERSION, dp_psd_state_bytes (s));
  DP_W_CHILD (&_w, dp_acc_trace, s->avg);
  DP_W_CHILD (&_w, dp_f32_framer, &s->fr);
}

int
dp_psd_set_state (dp_psd_state_t *s, const void *blob)
{
  DP_SET_OPEN (PSD_STATE_MAGIC, PSD_STATE_VERSION, dp_psd_state_bytes (s));
  /* The carried frame is checked before the average is written, so a refused
   * framer snapshot changes nothing: the framer's own check reads the blob
   * and writes nothing, and it refuses a hop that is not this object's. */
  uint64_t          frames;
  const void *const framer
      = (const uint8_t *)blob + _r.off + dp_acc_trace_state_bytes (s->avg);
  if (dp_f32_framer_state_frames (&s->fr, framer, &frames) != DP_OK)
    return DP_ERR_INVALID;
  DP_R_CHILD (&_r, dp_acc_trace, s->avg);
  DP_R_CHILD (&_r, dp_f32_framer, &s->fr);
  return DP_OK;
}

/* ── accumulation ──────────────────────────────────────────────────────── */

/* Transform the already-windowed-and-zero-padded k->frame into DC-centred
 * two-sided power, written to pwr[0..nfft). The one place a frame becomes a
 * spectrum: dp_psd_accumulate folds it into the average, dp_psd_frame_power
 * hands it to the caller, and neither can drift from the other. */
static void
psd_transform (dp_psd_kernel_t *k, float *pwr)
{
  const size_t nfft = k->nfft;
  const size_t half = nfft / 2; /* fftshift roll: bin 0 -> index nfft/2 */

  dp_fft_execute_cf32 (k->fft, k->frame, nfft, k->spec, nfft);

  for (size_t b = 0; b < nfft; b++)
    {
      const float re  = crealf (k->spec[b]);
      const float im  = cimagf (k->spec[b]);
      size_t      idx = b + half;
      if (idx >= nfft)
        idx -= nfft;
      pwr[idx] = re * re + im * im;
    }
}

/* Transform the kernel's frame and fold it into the running average. */
static void
psd_fold_frame (dp_psd_state_t *state)
{
  psd_transform (&state->k, state->k.pwr);
  dp_acc_trace_accumulate (state->avg, state->k.pwr, state->k.nfft);
}

/* Zero the zero-pad tail frame[n..nfft-1] (no-op when nfft == n). */
static void
psd_zero_pad (dp_psd_kernel_t *k)
{
  for (size_t i = k->n; i < k->nfft; i++)
    k->frame[i] = 0.0f;
}

/* Window one n-sample complex frame into k->frame and zero-pad it. */
static void
psd_window_frame (dp_psd_kernel_t *k, const float _Complex *xf)
{
  for (size_t i = 0; i < k->n; i++)
    k->frame[i] = k->w[i] * xf[i];
  psd_zero_pad (k);
}

/* Window one n-sample REAL frame into k->frame and zero-pad it. */
static void
psd_window_frame_real (dp_psd_kernel_t *k, const float *xf)
{
  for (size_t i = 0; i < k->n; i++)
    k->frame[i] = (float _Complex) (k->w[i] * xf[i]);
  psd_zero_pad (k);
}

void
dp_psd_accumulate (dp_psd_state_t *state, const float _Complex *x,
                   size_t x_len)
{
  /* The framer cuts the stream into frames at the hop and keeps the partial
   * one; every frame it hands out is windowed and folded in. The feed admits
   * whatever fits the ring (the carry is < n once drained, so the ring always
   * has room for at least one more sample), so the loop ends only when the
   * input is used up. A call of whole frames at hop = n takes the same path
   * as before, frame for frame. */
  size_t taken = 0, took = 0;
  while (taken < x_len
         && (took = dp_f32_framer_feed_view (&state->fr, x + taken,
                                             x_len - taken, SIZE_MAX))
                > 0)
    {
      taken += took;
      const float _Complex *frame;
      while ((frame = dp_f32_framer_next_view (&state->fr)) != NULL)
        {
          psd_window_frame (&state->k, frame);
          psd_fold_frame (state);
        }
    }
}

void
dp_psd_accumulate_real (dp_psd_state_t *state, const float *x, size_t x_len)
{
  const size_t n      = state->k.n;
  const size_t nframe = x_len / n;

  for (size_t f = 0; f < nframe; f++)
    {
      psd_window_frame_real (&state->k, x + f * n);
      psd_fold_frame (state);
    }
}

/* ── internal helpers ──────────────────────────────────────────────────── */

/* Pull the averaged power trace into the kernel's pwr scratch, which the
 * readouts borrow; 0 if nothing has been accumulated. */
static size_t
psd_pull_power (dp_psd_state_t *s)
{
  if (s->avg->count == 0)
    return 0;
  /* k.pwr is allocated at nfft, and s->avg is a length-nfft trace. */
  return dp_acc_trace_value (s->avg, s->k.nfft, s->k.pwr, s->k.nfft);
}

/* dBFS reference: cg^2 (coherent gain) times the 0-dBFS amplitude squared.
 * full_scale == 1.0 (the default) reduces this to the bare cg^2 normalisation,
 * so every dB getter is byte-identical to the un-scaled estimator. */
static double
psd_db_ref (const dp_psd_kernel_t *k)
{
  return k->cg * k->cg * k->full_scale * k->full_scale;
}

/* pwr[0..n) read against the kernel's reference -- the one conversion
 * every averaged readout and both single-frame readouts go through.  Linear
 * (full-scale^2 units, into `lin`) and dBFS with the -200 dB floor (into
 * `db`) come from the SAME double quotient, so the dB reading is exactly
 * 10*log10 of the linear one before either is rounded to float.  Taking the
 * log of the float-rounded linear value instead would move dB by up to an
 * ulp, and frame_db is pinned byte-for-byte (#1894's kernel promotion).
 * Either output may be NULL. */
static void
psd_read_power (const dp_psd_kernel_t *k, const float *pwr, float *lin,
                float *db, size_t n)
{
  const double ref = psd_db_ref (k);
  for (size_t i = 0; i < n; i++)
    {
      const double p = (double)pwr[i] / ref;
      if (lin)
        lin[i] = (float)p;
      if (db)
        db[i] = (float)(10.0 * log10 (fmax (p, PSD_FLOOR)));
    }
}

/* Fill out[0..n-1] with the averaged power spectrum in dBFS, where n is
 * min(nfft, max_out).  Returns the count written, 0 if nothing has been
 * accumulated yet.  The internal callers pass state->dbbuf, which is
 * allocated at nfft, so they pass nfft as the capacity. */
static size_t
psd_fill_db (dp_psd_state_t *s, float *out, size_t max_out)
{
  if (!psd_pull_power (s))
    return 0;
  const size_t n_out = s->k.nfft < max_out ? s->k.nfft : max_out;
  psd_read_power (&s->k, s->k.pwr, NULL, out, n_out);
  return n_out;
}

/* ── one frame, no averaging ────────────────────────────────────────────── */

void
dp_psd_frame_power (dp_psd_kernel_t *k, const float _Complex *x, float *pwr)
{
  psd_window_frame (k, x);
  psd_transform (k, pwr);
}

void
dp_psd_frame_linear (dp_psd_kernel_t *k, const float _Complex *x, float *out)
{
  dp_psd_frame_power (k, x, k->pwr);
  psd_read_power (k, k->pwr, out, NULL, k->nfft);
}

void
dp_psd_frame_db (dp_psd_kernel_t *k, const float _Complex *x, float *out)
{
  dp_psd_frame_power (k, x, k->pwr);
  psd_read_power (k, k->pwr, NULL, out, k->nfft);
}

/* ── linear-power accessors (raw spectral estimate for measurement) ────── */

size_t
dp_psd_power_twosided_max_out (dp_psd_state_t *state)
{
  return state->k.nfft;
}

size_t
dp_psd_power_twosided (dp_psd_state_t *state, size_t cap, float *out,
                       size_t max_out)
{
  (void)cap;
  if (!psd_pull_power (state))
    return 0;
  const double cg2 = state->k.cg * state->k.cg;
  /* Emission stops at the caller's capacity (jm gh-138). */
  const size_t n_out = state->k.nfft < max_out ? state->k.nfft : max_out;
  for (size_t i = 0; i < n_out; i++)
    out[i] = (float)((double)state->k.pwr[i] / cg2);
  return n_out;
}

size_t
dp_psd_power_onesided_max_out (dp_psd_state_t *state)
{
  return state->k.nfft / 2 + 1;
}

size_t
dp_psd_power_onesided (dp_psd_state_t *state, size_t cap, float *out,
                       size_t max_out)
{
  (void)cap;
  if (!psd_pull_power (state))
    return 0;
  const double cg2  = state->k.cg * state->k.cg;
  const size_t nfft = state->k.nfft;
  const size_t half = nfft / 2; /* DC sits at index half in pwr[] */

  /* Emission stops at the caller's capacity (jm gh-138). Unlike the other
   * readouts this one writes out[0] and out[half] OUTSIDE the loop, so the
   * clamp has to guard those two stores individually -- bounding the loop
   * alone would still overrun on a short buffer. */
  const size_t n_out = (half + 1) < max_out ? (half + 1) : max_out;
  if (n_out == 0)
    return 0;

  /* DC and Nyquist have no mirror partner; interior bins fold +k with -k. */
  out[0] = (float)((double)state->k.pwr[half] / cg2);
  if (half < n_out)
    out[half] = (float)((double)state->k.pwr[0] / cg2);
  for (size_t m = 1; m < half && m < n_out; m++)
    out[m] = (float)(((double)state->k.pwr[half + m]
                      + (double)state->k.pwr[half - m])
                     / cg2);
  return n_out;
}

/* Map a [lo,hi] Hz band to inclusive DC-centred bin indices.  Returns 0 if the
 * band lies entirely outside the analysed span [-fs/2, fs/2). */
static int
psd_band_bins (const dp_psd_state_t *s, double lo, double hi, size_t *ilo_out,
               size_t *ihi_out)
{
  if (hi < lo)
    {
      const double t = lo;
      lo             = hi;
      hi             = t;
    }
  const double half = (double)(s->k.nfft / 2);
  long         ilo  = (long)lround (lo / s->fs * (double)s->k.nfft + half);
  long         ihi  = (long)lround (hi / s->fs * (double)s->k.nfft + half);
  if (ihi < 0 || ilo > (long)s->k.nfft - 1)
    return 0;
  if (ilo < 0)
    ilo = 0;
  if (ihi > (long)s->k.nfft - 1)
    ihi = (long)s->k.nfft - 1;
  *ilo_out = (size_t)ilo;
  *ihi_out = (size_t)ihi;
  return 1;
}

/* Linear power integrated over a [lo,hi] Hz band (dBFS reference).
 *
 * Band-integrated power uses the *noise-power* (incoherent) window
 * normalisation nfft*s2, NOT the coherent gain cg^2 that psd_db uses to read a
 * single tone's peak bin.  The two differ by the window's equivalent noise
 * bandwidth, ENBW = nfft*s2/cg^2: a windowed tone leaks across ~ENBW bins, so
 * summing pwr/cg^2 over a band over-counts the true power by ENBW — and, with
 * zero-padding, by a further nfft/n.  Normalising by nfft*s2 (== cg^2 * nfft/n
 * * enbw, the same calibration nprmeas applies per bin) makes an integrated
 * band read its true power window- and pad-independently: by Parseval a
 * full-scale tone integrates to full_scale^2 and a noise band to its variance.
 */
static double
psd_band_lin (const dp_psd_state_t *s, double lo, double hi)
{
  size_t ilo, ihi;
  if (!psd_band_bins (s, lo, hi, &ilo, &ihi))
    return 0.0;
  const double ref
      = (double)s->k.nfft * s->k.s2 * s->k.full_scale * s->k.full_scale;
  double sum = 0.0;
  for (size_t i = ilo; i <= ihi; i++)
    sum += (double)s->k.pwr[i] / ref;
  return sum;
}

/* ── PSD trace getters ─────────────────────────────────────────────────── */

size_t
dp_psd_psd_db_max_out (dp_psd_state_t *state)
{
  return state->k.nfft;
}

size_t
dp_psd_psd_db (dp_psd_state_t *state, size_t n, float *out, size_t max_out)
{
  (void)n;
  /* psd_fill_db does the clamping and returns what it wrote. */
  return psd_fill_db (state, out, max_out);
}

size_t
dp_psd_psd_dbhz_max_out (dp_psd_state_t *state)
{
  return state->k.nfft;
}

size_t
dp_psd_psd_dbhz (dp_psd_state_t *state, size_t n, float *out, size_t max_out)
{
  /* Offset only what dp_psd_psd_db actually wrote -- walking to nfft here
   * would read past the clamp. */
  const size_t n_out = dp_psd_psd_db (state, n, out, max_out);
  if (!n_out)
    return 0;
  /* dB/Hz differs from the dBFS spectrum by a constant. */
  const double off
      = 10.0 * log10 (state->fs * state->k.s2 / (state->k.cg * state->k.cg));
  for (size_t i = 0; i < n_out; i++)
    out[i] = (float)((double)out[i] - off);
  return n_out;
}

/* ── band power ────────────────────────────────────────────────────────── */

size_t
dp_psd_band_power_max_out (dp_psd_state_t *state)
{
  (void)state;
  return 0; /* the binding grows the buffer to the bands length */
}

size_t
dp_psd_band_power (dp_psd_state_t *state, const double *bands,
                   size_t bands_len, float *out, size_t max_out)
{
  if (!psd_pull_power (state))
    return 0;
  /* One output per BAND, i.e. per pair of edges -- so the capacity a
   * caller needs is bands_len/2, not bands_len (jm gh-138). */
  size_t nb = bands_len / 2;
  if (nb > max_out)
    nb = max_out;
  for (size_t b = 0; b < nb; b++)
    {
      double lp = psd_band_lin (state, bands[2 * b], bands[2 * b + 1]);
      out[b]    = (float)(10.0 * log10 (fmax (lp, PSD_FLOOR)));
    }
  return nb;
}

double
dp_psd_total_band_power (dp_psd_state_t *state, const double *bands,
                         size_t bands_len)
{
  if (!psd_pull_power (state))
    return 10.0 * log10 (PSD_FLOOR);
  const size_t nb  = bands_len / 2;
  double       sum = 0.0;
  for (size_t b = 0; b < nb; b++)
    sum += psd_band_lin (state, bands[2 * b], bands[2 * b + 1]);
  return 10.0 * log10 (fmax (sum, PSD_FLOOR));
}

/* ── scalar measurements ───────────────────────────────────────────────── */

double
dp_psd_occupied_bw (dp_psd_state_t *state, double fraction)
{
  /* The search, and the rule that a fraction outside (0, 1) reads NaN, are
   * dp_obw_from_power's; this only hands it the averaged power, which the
   * averager already holds DC-centred and in double (avg->acc), so there is
   * nothing to copy or widen.  Before any frame it is handed no bins, so it
   * still answers NaN for a bad fraction and 0 for a good one.  The power is
   * not divided by cg^2: OBW is a ratio of power sums, so the scale cancels
   * -- and without the division equal bins sum exactly, so an edge landing
   * exactly on a bin boundary resolves exactly.  The private copy this
   * replaced divided, and missed such a tie by one bin. */
  const size_t n = state->avg->count ? state->k.nfft : 0;
  return dp_obw_from_power (state->avg->acc, n, state->fs, fraction);
}

double
dp_psd_noise_floor (dp_psd_state_t *state)
{
  if (!psd_fill_db (state, state->dbbuf, state->k.nfft))
    return 0.0;
  return dp_noise_floor_db (state->dbbuf, state->k.nfft);
}

double
dp_psd_snr (dp_psd_state_t *state, double lo_hz, double hi_hz)
{
  if (!psd_fill_db (state, state->dbbuf, state->k.nfft))
    return 0.0;
  const double floor = dp_noise_floor_db (state->dbbuf, state->k.nfft);

  size_t ilo, ihi;
  if (!psd_band_bins (state, lo_hz, hi_hz, &ilo, &ihi))
    return 0.0;
  double peak = -INFINITY;
  for (size_t i = ilo; i <= ihi; i++)
    if (state->dbbuf[i] > peak)
      peak = state->dbbuf[i];
  return peak - floor;
}

double
dp_psd_sfdr (dp_psd_state_t *state, float min_db)
{
  if (!psd_fill_db (state, state->dbbuf, state->k.nfft))
    return 0.0;
  dp_peak_t    pk[16];
  const size_t np
      = dp_find_peaks_f32 (state->dbbuf, state->k.nfft, 16, min_db, pk);
  if (np < 2)
    return 0.0;
  /* find_peaks_f32 returns peaks sorted by descending amplitude. */
  return (double)(pk[0].amplitude_db - pk[1].amplitude_db);
}
