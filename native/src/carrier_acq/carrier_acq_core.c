#include "doppler/carrier_acq/carrier_acq_core.h"
#include "doppler/carrier_acq/carrier_acq_private.h"
#include "doppler/detection/detection_private.h"
#include "doppler/dp_complex.h"
#include "doppler/util/util_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* The average PSD of a random +-1 rectangular-pulse (NRZ) BPSK symbol
 * stream at symbol_rate_hz: sinc^2(f/symbol_rate_hz), DC-centred to
 * match dp_psd_power_twosided()'s own bin order (bin i -> (i - nfft/2) *
 * sample_rate_hz / nfft) -- ported from freq_refine.py's own
 * _known_symbol_psd_template, just derived from the DC-centred grid
 * instead of np.fft.fftfreq's native-order one. */
static void
carrier_acq_default_template (float *out, size_t nfft, double sample_rate_hz,
                              double symbol_rate_hz)
{
  for (size_t i = 0; i < nfft; i++)
    {
      double freq_hz
          = ((double)i - (double)nfft / 2.0) * sample_rate_hz / (double)nfft;
      double s = dp_sinc (freq_hz / symbol_rate_hz);
      out[i]   = (float)(s * s);
    }
}

/* 3-point parabolic sub-bin fit -- ported from freq_refine.py's own
 * _parabolic_peak. Returns the fractional bin offset in [-0.5, 0.5];
 * degenerates to 0 on a flat/noise-floor peak rather than dividing by
 * zero. */
static double
carrier_acq_parabolic_offset (float y1, float y2, float y3)
{
  float denom = y1 - 2.0f * y2 + y3;
  if (denom == 0.0f)
    return 0.0;
  return 0.5 * (double)(y1 - y3) / (double)denom;
}

/* The template's null statistics (docs/design/async-dsss-receiver.md §4.7).
 * One block's correlation at a lag is the quadratic form x^H A x of the
 * block's n samples, A[j][k] = w_j w_k tau(j - k), tau(d) the template's
 * transform at lag d, sum_m t_m exp(-2 pi i d (m - nfft/2) / nfft) (the
 * template is DC-centred at nfft/2). Under white noise it is a weighted sum
 * of exponentials, moment-matched to a chi-square of 2 tr(A)^2 / ||A||_F^2
 * degrees of freedom a block. |A[j][k]|^2 depends on j - k only through
 * |tau|, so the Frobenius sums are sums over the lag d of g(d) =
 * |tau(d)|^2 sum_{j - k = d} w_j^2 w_k^2: the lag process's correlation at
 * a shift of D bins is sum_d g(d) cos(2 pi d D / nfft) / sum_d g(d), its
 * second spectral moment 1 - that at D = 1, and the circular sum of it
 * nfft g(0) / sum_d g(d) (every |d| < n < nfft), so the median's
 * independent lags are sum_d g(d) / g(0). O(n * nfft), no matrix formed. */
static void
carrier_acq_null_stats (dp_carrier_acq_state_t *s, const float *w, size_t n,
                        const float *tmpl, size_t nfft)
{
  double f2 = 0.0, g0 = 0.0, g1 = 0.0, tr = 0.0, sw2 = 0.0;
  for (size_t j = 0; j < n; j++)
    sw2 += (double)w[j] * (double)w[j];
  for (size_t d = 0; d < n; d++)
    {
      double re = 0.0, im = 0.0;
      for (size_t m = 0; m < nfft; m++)
        {
          const double ph = -2.0 * M_PI * (double)d
                            * ((double)m - (double)nfft / 2.0) / (double)nfft;
          re += (double)tmpl[m] * cos (ph);
          im += (double)tmpl[m] * sin (ph);
        }
      double ww = 0.0;
      for (size_t k = 0; k + d < n; k++)
        ww += (double)w[k + d] * (double)w[k + d] * (double)w[k]
              * (double)w[k];
      const double g    = (re * re + im * im) * ww;
      const double mult = d == 0 ? 1.0 : 2.0; /* lags +d and -d */
      f2 += mult * g;
      g1 += mult * g * cos (2.0 * M_PI * (double)d / (double)nfft);
      if (d == 0)
        {
          g0 = g;
          tr = re * sw2;
        }
    }
  s->thr_nu1  = 2.0 * tr * tr / f2;
  s->thr_lam2 = 1.0 - g1 / f2;
  s->thr_neff = f2 / g0;
}

/* A chi-square of nu degrees of freedom: its density, and its median by
   bisection on the survival function (dp__gammaq, the detection module's
   one incomplete gamma). */
static double
carrier_acq_chi2_pdf (double y, double nu)
{
  if (y <= 0.0)
    return 0.0;
  const double h = 0.5 * nu;
  return exp ((h - 1.0) * log (y) - 0.5 * y - h * M_LN2 - dp_lgamma (h));
}

static double
carrier_acq_chi2_median (double nu)
{
  double lo = 0.0, hi = nu + 20.0 * sqrt (2.0 * nu) + 50.0;
  for (int i = 0; i < 100; i++)
    {
      const double mid = 0.5 * (lo + hi);
      if (dp__gammaq (0.5 * nu, 0.5 * mid) > 0.5)
        lo = mid;
      else
        hi = mid;
    }
  return 0.5 * (lo + hi);
}

/* P(peak / median > eta) under noise alone, at nu degrees of freedom a
   lag: Rice's upcrossing count of the chi-square lag process over the nfft
   circular lags, plus one lag's own tail, averaged over the median's
   spread -- a sample median of thr_neff independent lags, normal about the
   chi-square median with standard error 1 / (2 f(m) sqrt(thr_neff)) --
   by a 81-node rule over +-6 standard errors. */
static double
carrier_acq_null_pfa (const dp_carrier_acq_state_t *s, double nu, double m,
                      double se, double eta)
{
  double num = 0.0, den = 0.0;
  for (int i = 0; i < 81; i++)
    {
      const double z  = -6.0 + 12.0 * (double)i / 80.0;
      const double wt = exp (-0.5 * z * z);
      double       mm = m + se * z;
      if (mm < 1e-9)
        mm = 1e-9;
      const double y = eta * mm;
      double       p = (double)s->nfft * sqrt (2.0 * y * s->thr_lam2 / M_PI)
                           * carrier_acq_chi2_pdf (y, nu)
                       + dp__gammaq (0.5 * nu, 0.5 * y);
      num += wt * (p < 1.0 ? p : 1.0);
      den += wt;
    }
  return num / den;
}

double
dp__carrier_acq_derived_threshold (const dp_carrier_acq_state_t *s,
                                   size_t                        n_blocks)
{
  const double nu = s->thr_nu1 * (double)n_blocks;
  const double m  = carrier_acq_chi2_median (nu);
  const double se
      = 1.0 / (2.0 * carrier_acq_chi2_pdf (m, nu) * sqrt (s->thr_neff));
  double lo = 1.0, hi = 100.0; /* the null probability falls with eta */
  for (int i = 0; i < 60; i++)
    {
      const double mid = 0.5 * (lo + hi);
      if (carrier_acq_null_pfa (s, nu, m, se, mid) > s->pfa)
        lo = mid;
      else
        hi = mid;
    }
  return 0.5 * (lo + hi);
}

void
dp__carrier_acq_derive_threshold (dp_carrier_acq_state_t *ca)
{
  ca->thr_derived = 1;
}

dp_carrier_acq_state_t *
dp_carrier_acq_create (double sample_rate_hz, double symbol_rate_hz,
                       double resolution_hz, size_t zero_pad, int window,
                       float beta, const float *psd_template,
                       size_t psd_template_len, double pfa, double pd,
                       double design_snr, bool sequential, size_t max_n_blocks)
{
  if (sample_rate_hz <= 0.0 || symbol_rate_hz <= 0.0 || zero_pad < 1
      || pfa <= 0.0 || pfa >= 1.0 || pd <= 0.0 || pd >= 1.0
      || design_snr < 0.0)
    return NULL;

  /* Three windows are offered -- by the header, the manifest and the
   * Python face -- Hann, Kaiser and Blackman-Harris. PSD also takes the
   * rectangular window, which was never offered here, so it is refused
   * rather than built (#2185). (KAPPA, below, was fit at one Hann
   * configuration: it does not set rect apart from the other two.) The
   * bottom of the range is PSD's to check, as the one owner of the
   * index. */
  /* SSOT: view=carrier_acq_window */
  if (window > DP_PSD_WINDOW_BLACKMAN_HARRIS)
    return NULL;

  if (resolution_hz <= 0.0)
    resolution_hz = symbol_rate_hz / 10.0;
  /* Floored at 3.  The floor was first needed because PSD's Hann was then
   * the symmetric form, all zeros at 2, which PSD refused, and a caller
   * wrapping this create in dp_xnn -- AsyncDsssReceiver does, with
   * refine_n_fft as the block -- aborted on it (#1959, #1985).  PSD's
   * Hann is periodic now, [0, 1] at 2 (#2053), but a 2-point block still
   * resolves nothing useful, so the floor stays. */
  size_t n_fft = (size_t)llround (sample_rate_hz / resolution_hz);
  if (n_fft < 3)
    n_fft = 3;

  dp_carrier_acq_state_t *s = calloc (1, sizeof (*s));
  if (!s)
    return NULL;

  s->psd
      = dp_psd_create (n_fft, sample_rate_hz, window, beta, zero_pad,
                       /*full_scale=*/1.0, /*bits=*/0, /*mode=*/ACC_TRACE_MEAN,
                       /*alpha=*/0.1);
  if (!s->psd)
    {
      free (s);
      return NULL;
    }
  s->nfft = s->psd->k.nfft;

  if (psd_template_len != 0 && psd_template_len != s->nfft)
    {
      dp_psd_destroy (s->psd);
      free (s);
      return NULL;
    }

  float          *tmpl = dp_mallocn (s->nfft, sizeof (float));
  float _Complex *ref  = dp_mallocn (s->nfft, sizeof (float _Complex));
  if (!tmpl || !ref)
    {
      free (tmpl);
      free (ref);
      dp_psd_destroy (s->psd);
      free (s);
      return NULL;
    }
  if (psd_template_len == s->nfft)
    memcpy (tmpl, psd_template, s->nfft * sizeof (float));
  else
    carrier_acq_default_template (tmpl, s->nfft, sample_rate_hz,
                                  symbol_rate_hz);
  carrier_acq_null_stats (s, s->psd->k.w, s->psd->k.n, tmpl, s->nfft);
  double s_t = 0.0, s_t2 = 0.0;
  for (size_t k = 0; k < s->nfft; k++)
    {
      ref[k] = tmpl[k] + 0.0f * I;
      s_t += (double)tmpl[k];
      s_t2 += (double)tmpl[k] * (double)tmpl[k];
    }
  s->s_t  = s_t;
  s->s_t2 = s_t2;
  free (tmpl);

  /* Full-span noise/peak search (no sub-band restriction -- deferred,
   * see FINISHING_PLAN.md/the design memory): CarrierAcquisition always
   * runs after Acquisition's own coarse Doppler search, so the residual
   * it estimates is already small relative to the full analyzed span.
   * Median noise aggregation is more CFAR-robust than mean here: the
   * mainlobe peak itself sits inside the same full span being
   * aggregated over, and a plain mean would be biased upward by it. */
  s->det = dp_correlator_create_detector_1d (
      ref, s->nfft, /*dwell=*/1, /*noise_lo=*/0,
      /*noise_hi=*/s->nfft - 1, DET_NOISE_MEDIAN,
      /*threshold=*/0.0f, /*nthreads=*/1);
  free (ref);
  if (!s->det)
    {
      dp_psd_destroy (s->psd);
      free (s);
      return NULL;
    }

  s->pwr_buf   = dp_mallocn (s->nfft, sizeof (float));
  s->power_buf = dp_mallocn (s->nfft, sizeof (float _Complex));
  s->carry_buf = dp_mallocn (n_fft, sizeof (float _Complex));
  if (!s->pwr_buf || !s->power_buf || !s->carry_buf)
    {
      free (s->pwr_buf);
      free (s->power_buf);
      free (s->carry_buf);
      dp_correlator_destroy (s->det);
      dp_psd_destroy (s->psd);
      free (s);
      return NULL;
    }

  /* n_coh = n_fft: each non-coherent look is one whole-window FFT, so
   * the window length itself IS the coherent integration length -- the
   * same role Acquisition's own D*cb plays in its dp_det_n_noncoh() call. */
  int dw          = dp_det_n_noncoh (design_snr, (int)n_fft, pd, pfa, 100000);
  s->dwell_target = (dw > 0) ? (size_t)dw : 100000;
  s->max_n_blocks = (max_n_blocks > 0) ? max_n_blocks : 1;

  s->sample_rate_hz = sample_rate_hz;
  s->pfa            = pfa;
  s->sequential     = sequential;
  s->carry_len      = 0;
  s->n_blocks       = 0;
  s->ready          = false;
  s->residual_hz    = 0.0;
  return s;
}

void
dp_carrier_acq_destroy (dp_carrier_acq_state_t *state)
{
  if (!state)
    return;
  dp_psd_destroy (state->psd);
  dp_correlator_destroy (state->det);
  free (state->pwr_buf);
  free (state->power_buf);
  free (state->carry_buf);
  free (state);
}

void
dp_carrier_acq_reset (dp_carrier_acq_state_t *state)
{
  dp_psd_reset (state->psd);
  dp_correlator_reset (state->det);
  state->carry_len   = 0;
  state->n_blocks    = 0;
  state->ready       = false;
  state->residual_hz = 0.0;
}

/* Sub-bin refine the peak at `lag` directly off the detector's own
 * det_out (the correlation map from the dump that just fired -- no
 * second correlation pass needed) and mark ready. */
static void
carrier_acq_finish (dp_carrier_acq_state_t *s, size_t lag)
{
  size_t nfft    = s->nfft;
  float  y1      = crealf (s->det->det_out[(lag + nfft - 1) % nfft]);
  float  y2      = crealf (s->det->det_out[lag]);
  float  y3      = crealf (s->det->det_out[(lag + 1) % nfft]);
  double frac    = carrier_acq_parabolic_offset (y1, y2, y3);
  double bin_pos = (double)lag + frac;
  if (bin_pos > (double)nfft / 2.0)
    bin_pos -= (double)nfft;
  s->residual_hz = bin_pos * s->sample_rate_hz / (double)nfft;
  s->ready       = true;
}

/* The give-up bound for whichever mode is active: sequential mode
 * tests every block against its OWN rising CFAR threshold, so it must
 * not be capped by dwell_target (design_snr's one-shot point estimate
 * for non-sequential mode) -- an optimistic design_snr guess (small
 * dwell_target) would otherwise stop sequential mode from trying more
 * blocks exactly when real data shows it needs to. */
static size_t
carrier_acq_giveup_cap (const dp_carrier_acq_state_t *s)
{
  return s->sequential ? s->max_n_blocks : s->dwell_target;
}

/* Empirically-calibrated CFAR ratio threshold (test_stat = peak/
 * median) for THIS object's real statistic -- a power-spectrum-vs-
 * known-template correlation, NOT the classic complex-correlator
 * peak/noise envelope ratio dp_det_threshold_noncoherent() was derived
 * for (confirmed via Monte Carlo this session: that borrowed formula
 * is ~5x too conservative here, see FINISHING_PLAN.md's
 * CarrierAcquisition section / derive_carrier_acq_statistic.py).
 *
 * Derivation (validated for the MEAN; KAPPA below is where the
 * approximation lives): under H0, the averaged, CG^2-normalised
 * periodogram Pavg[m] ~ Gamma(n_blocks, mu/n_blocks) exactly, so
 * C[k] = sum_m Pavg[m]*template[(m-k) mod nfft] has EXACT mean
 * mu*s_t; its variance follows the same Gamma-sum shape
 * ((mu^2/n_blocks)*s_t2), up to window-induced bin correlation and
 * the argmax-over-nfft-correlated-lags extreme-value effect that a
 * plain per-lag variance doesn't capture. noise_est estimates mu*s_t,
 * so the ratio threshold is 1 + z*sqrt(s_t2/n_blocks)/s_t for some
 * tail quantile z; KAPPA*det_threshold(pfa) (dp_det_threshold() --
 * doppler's own existing sqrt(-2*ln(pfa)) primitive, reused rather
 * than adding a new inverse-normal-CDF) stands in for z.
 *
 * KAPPA=7.9 was fit via 3000-trial null-distribution Monte Carlo at
 * THIS object's own most common real configuration (sample_rate_hz=
 * 8000, symbol_rate_hz=1000, default resolution/zero_pad -- nfft=512,
 * hann window, default template -- matches every existing carrier_acq
 * test/gallery demo in this repo), checked at pfa in {1e-2,1e-3} and
 * n_blocks in {1,4,13}: consistently ERRS 10-30% ON THE CONSERVATIVE
 * SIDE (a slightly higher threshold, hence a slightly lower realized
 * Pfa/Pd than requested -- safer than the reverse) rather than being
 * exact. This is NOT yet a general closed form valid across arbitrary
 * nfft/window/template choices -- a proper Sidak/Bonferroni-style
 * effective-independent-lags derivation for the argmax extreme value
 * didn't cleanly close on a first attempt (see
 * derive_carrier_acq_statistic.py), and KAPPA itself was seen to need
 * real recalibration when nfft changed (an earlier, toy nfft=64
 * calibration point was wildly wrong at this object's real nfft=512
 * default -- caught by the very C test suite this fix now passes).
 * A derived rule exists beside it, dp__carrier_acq_derived_threshold()
 * (docs/design/async-dsss-receiver.md §4.7): Rice's upcrossing count over
 * the lags with the median's own spread, conservative where the template
 * leaves enough independent lags (a tone's, 23) and over-correcting where
 * it does not (this default sinc^2 one, 2.9). It is switched on per
 * estimator; this rule stays the default. */
#define CARRIER_ACQ_KAPPA 7.9

static float
carrier_acq_ratio_threshold (const dp_carrier_acq_state_t *s, size_t n_blocks)
{
  if (s->thr_derived)
    return (float)dp__carrier_acq_derived_threshold (s, n_blocks);
  double z      = CARRIER_ACQ_KAPPA * dp_det_threshold (s->pfa);
  double spread = sqrt (s->s_t2 / (double)n_blocks) / s->s_t;
  return (float)(1.0 + z * spread);
}

static void
carrier_acq_process_block (dp_carrier_acq_state_t *s,
                           const float _Complex   *block)
{
  dp_psd_accumulate (s->psd, block, s->psd->k.n);
  s->n_blocks++;

  bool do_test = s->sequential || (s->n_blocks == s->dwell_target);
  if (!do_test)
    return;

  size_t got = dp_psd_power_twosided (s->psd, s->pwr_buf, s->nfft);
  if (got == 0)
    return;
  for (size_t k = 0; k < s->nfft; k++)
    s->power_buf[k] = s->pwr_buf[k] + 0.0f * I;

  float eta_nc = carrier_acq_ratio_threshold (s, s->n_blocks);
  (void)dp_correlator_set_threshold (s->det, eta_nc); /* a detector: DP_OK */

  /* One hit at most (one frame); at one row its col is the lag. */
  dp_correlator_hit_t result[1];
  size_t n_res = dp_correlator_push (s->det, s->power_buf, s->nfft, result, 1);
  if (n_res > 0)
    carrier_acq_finish (s, result[0].col);
}

void
dp_carrier_acq_steps (dp_carrier_acq_state_t *state, const float _Complex *x,
                      size_t x_len)
{
  size_t cap = carrier_acq_giveup_cap (state);
  if (state->ready || state->n_blocks >= cap)
    return;

  size_t n_fft = state->psd->k.n;
  size_t off   = 0;

  if (state->carry_len > 0)
    {
      size_t need = n_fft - state->carry_len;
      size_t take = (x_len < need) ? x_len : need;
      memcpy (state->carry_buf + state->carry_len, x,
              take * sizeof (float _Complex));
      state->carry_len += take;
      off += take;
      if (state->carry_len == n_fft)
        {
          carrier_acq_process_block (state, state->carry_buf);
          state->carry_len = 0;
        }
    }

  while (!state->ready && state->n_blocks < cap && off + n_fft <= x_len)
    {
      carrier_acq_process_block (state, x + off);
      off += n_fft;
    }

  size_t rem = x_len - off;
  if (rem > 0 && rem < n_fft && !state->ready && state->n_blocks < cap)
    {
      memcpy (state->carry_buf, x + off, rem * sizeof (float _Complex));
      state->carry_len = rem;
    }
}

typedef struct
{
  uint64_t nfft;
  uint64_t dwell_target;
  uint64_t max_n_blocks;
  uint64_t n_blocks;
  uint8_t  ready;
  uint8_t  thr_derived; /* version 3: the threshold rule, 0 or 1 */
  /* Explicit, and named on purpose: a designated initializer zeroes members
     it does not mention, but the PADDING between them keeps unspecified
     values (C11 6.7.9p10), and this struct is written to the blob whole. The
     six bytes here and thr_derived are the seven the compiler would insert
     before the double, so the layout keeps its size. Both DSSS receivers
     already carry this member for this reason. */
  uint8_t  _pad[6];
  double   residual_hz;
  uint64_t carry_len;
} carrier_acq_extra_t;

size_t
dp_carrier_acq_state_bytes (const dp_carrier_acq_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (carrier_acq_extra_t)
         + dp_psd_state_bytes (s->psd) + dp_correlator_state_bytes (s->det)
         + s->psd->k.n * sizeof (float _Complex);
}

void
dp_carrier_acq_get_state (const dp_carrier_acq_state_t *s, void *blob)
{
  DP_GET_OPEN (CARRIER_ACQ_STATE_MAGIC, CARRIER_ACQ_STATE_VERSION,
               dp_carrier_acq_state_bytes (s));
  carrier_acq_extra_t extra = {
    .nfft         = (uint64_t)s->nfft,
    .dwell_target = (uint64_t)s->dwell_target,
    .max_n_blocks = (uint64_t)s->max_n_blocks,
    .n_blocks     = (uint64_t)s->n_blocks,
    .ready        = (uint8_t)s->ready,
    .thr_derived  = (uint8_t)(s->thr_derived != 0),
    .residual_hz  = s->residual_hz,
    .carry_len    = (uint64_t)s->carry_len,
  };
  dp_w_bytes (&_w, &extra, sizeof extra);
  DP_W_CHILD (&_w, dp_psd, s->psd);
  DP_W_CHILD (&_w, dp_correlator, s->det);
  /* Only the first carry_len samples are state. The rest of the buffer is
     whatever an earlier block left there, or what malloc handed back, so
     writing it made the blob a function of the allocator, not of the
     object (#2076). The region keeps its fixed psd->k.n size, as
     state_bytes() promises, and carries zeros past the carry. */
  const size_t tail = s->psd->k.n - s->carry_len;
  dp_w_cf32 (&_w, s->carry_buf, s->carry_len);
  void *zeros = dp_w_reserve (&_w, tail * sizeof (float _Complex));
  if (zeros)
    memset (zeros, 0, tail * sizeof (float _Complex));
}

int
dp_carrier_acq_set_state (dp_carrier_acq_state_t *s, const void *blob)
{
  DP_SET_OPEN (CARRIER_ACQ_STATE_MAGIC, CARRIER_ACQ_STATE_VERSION,
               dp_carrier_acq_state_bytes (s));
  carrier_acq_extra_t extra;
  dp_r_bytes (&_r, &extra, sizeof extra);
  if (extra.nfft != (uint64_t)s->nfft
      || extra.dwell_target != (uint64_t)s->dwell_target
      || extra.max_n_blocks != (uint64_t)s->max_n_blocks
      /* A carry is 0..n-1 samples: steps processes a full one at once, so
         get_state never writes n, and a blob that claims it is refused
         rather than restored into a state steps cannot reach. */
      || extra.carry_len >= (uint64_t)s->psd->k.n || extra.thr_derived > 1)
    return DP_ERR_INVALID;
  /* The residual is a bin's offset within the sample rate, so it lies within
     half of it. A forged residual of NaN or 1e298 reached the async hand-off's
     Dll as a NaN seed phase (doppler#2103, through set_state). `!(...)` also
     refuses a NaN, which fails every comparison. */
  if (!(fabs (extra.residual_hz) <= 0.5 * s->sample_rate_hz))
    return DP_ERR_INVALID;
  /* Both children are checked before either is written, so a refused one
     changes nothing: the detector's blob (after psd's) by its own check,
     then psd's restore, which refuses before it writes, then the
     detector's, which can no longer refuse. */
  const void *det_blob
      = (const uint8_t *)blob + _r.off + dp_psd_state_bytes (s->psd);
  if (dp_correlator_state_check (s->det, det_blob) != DP_OK)
    return DP_ERR_INVALID;
  DP_R_CHILD (&_r, dp_psd, s->psd);
  DP_R_CHILD (&_r, dp_correlator, s->det);
  dp_r_cf32 (&_r, s->carry_buf, s->psd->k.n);
  s->n_blocks    = (size_t)extra.n_blocks;
  s->ready       = (bool)extra.ready;
  s->residual_hz = extra.residual_hz;
  s->carry_len   = (size_t)extra.carry_len;
  /* The rule the writer tested with: a restored squared-tone estimator
     keeps its derived threshold whatever its target was built with. */
  s->thr_derived = extra.thr_derived;
  return DP_OK;
}
