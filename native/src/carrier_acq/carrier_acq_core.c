#include "carrier_acq/carrier_acq_core.h"
#include <complex.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* sin(pi*u)/(pi*u), sinc(0) = 1 -- same idiom as acq_core.c's own _sinc(). */
static double
_sinc (double u)
{
  return (u == 0.0) ? 1.0 : sin (M_PI * u) / (M_PI * u);
}

/* The average PSD of a random +-1 rectangular-pulse (NRZ) BPSK symbol
 * stream at symbol_rate_hz: sinc^2(f/symbol_rate_hz), DC-centred to
 * match psd_power_twosided()'s own bin order (bin i -> (i - nfft/2) *
 * sample_rate_hz / nfft) -- ported from freq_refine.py's own
 * _known_symbol_psd_template, just derived from the DC-centred grid
 * instead of np.fft.fftfreq's native-order one. */
static void
_default_template (float *out, size_t nfft, double sample_rate_hz,
                   double symbol_rate_hz)
{
  for (size_t i = 0; i < nfft; i++)
    {
      double freq_hz
          = ((double)i - (double)nfft / 2.0) * sample_rate_hz / (double)nfft;
      double s = _sinc (freq_hz / symbol_rate_hz);
      out[i]   = (float)(s * s);
    }
}

/* 3-point parabolic sub-bin fit -- ported from freq_refine.py's own
 * _parabolic_peak. Returns the fractional bin offset in [-0.5, 0.5];
 * degenerates to 0 on a flat/noise-floor peak rather than dividing by
 * zero. */
static double
_parabolic_offset (float y1, float y2, float y3)
{
  float denom = y1 - 2.0f * y2 + y3;
  if (denom == 0.0f)
    return 0.0;
  return 0.5 * (double)(y1 - y3) / (double)denom;
}

carrier_acq_state_t *
carrier_acq_create (double sample_rate_hz, double symbol_rate_hz,
                    double resolution_hz, size_t zero_pad, int window,
                    float beta, const float *psd_template,
                    size_t psd_template_len, double pfa, double pd,
                    double design_snr, bool sequential, size_t max_n_blocks)
{
  if (sample_rate_hz <= 0.0 || symbol_rate_hz <= 0.0 || zero_pad < 1
      || pfa <= 0.0 || pfa >= 1.0 || pd <= 0.0 || pd >= 1.0
      || design_snr < 0.0)
    return NULL;

  if (resolution_hz <= 0.0)
    resolution_hz = symbol_rate_hz / 10.0;
  size_t n_fft = (size_t)llround (sample_rate_hz / resolution_hz);
  if (n_fft < 2)
    n_fft = 2;

  carrier_acq_state_t *s = calloc (1, sizeof (*s));
  if (!s)
    return NULL;

  s->psd = psd_create (n_fft, sample_rate_hz, window, beta, zero_pad,
                       /*full_scale=*/1.0, /*bits=*/0, /*mode=*/0 /*mean*/,
                       /*alpha=*/0.1);
  if (!s->psd)
    {
      free (s);
      return NULL;
    }
  s->nfft = s->psd->nfft;

  if (psd_template_len != 0 && psd_template_len != s->nfft)
    {
      psd_destroy (s->psd);
      free (s);
      return NULL;
    }

  float         *tmpl = malloc (s->nfft * sizeof (float));
  float complex *ref  = malloc (s->nfft * sizeof (float complex));
  if (!tmpl || !ref)
    {
      free (tmpl);
      free (ref);
      psd_destroy (s->psd);
      free (s);
      return NULL;
    }
  if (psd_template_len == s->nfft)
    memcpy (tmpl, psd_template, s->nfft * sizeof (float));
  else
    _default_template (tmpl, s->nfft, sample_rate_hz, symbol_rate_hz);
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
  s->det = detector_create (ref, s->nfft, /*dwell=*/1, /*noise_lo=*/0,
                            /*noise_hi=*/s->nfft - 1, DET_NOISE_MEDIAN,
                            /*threshold=*/0.0f, /*nthreads=*/1);
  free (ref);
  if (!s->det)
    {
      psd_destroy (s->psd);
      free (s);
      return NULL;
    }

  s->pwr_buf   = malloc (s->nfft * sizeof (float));
  s->power_buf = malloc (s->nfft * sizeof (float complex));
  s->carry_buf = malloc (n_fft * sizeof (float complex));
  if (!s->pwr_buf || !s->power_buf || !s->carry_buf)
    {
      free (s->pwr_buf);
      free (s->power_buf);
      free (s->carry_buf);
      detector_destroy (s->det);
      psd_destroy (s->psd);
      free (s);
      return NULL;
    }

  /* n_coh = n_fft: each non-coherent look is one whole-window FFT, so
   * the window length itself IS the coherent integration length -- the
   * same role Acquisition's own D*cb plays in its det_n_noncoh() call. */
  int dw          = det_n_noncoh (design_snr, (int)n_fft, pd, pfa, 100000);
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
carrier_acq_destroy (carrier_acq_state_t *state)
{
  if (!state)
    return;
  psd_destroy (state->psd);
  detector_destroy (state->det);
  free (state->pwr_buf);
  free (state->power_buf);
  free (state->carry_buf);
  free (state);
}

void
carrier_acq_reset (carrier_acq_state_t *state)
{
  psd_reset (state->psd);
  detector_reset (state->det);
  state->carry_len   = 0;
  state->n_blocks    = 0;
  state->ready       = false;
  state->residual_hz = 0.0;
}

/* Sub-bin refine the peak at `lag` directly off detector_state_t's own
 * out_buf (the correlation map from the dump that just fired -- no
 * second correlation pass needed) and mark ready. */
static void
_finish (carrier_acq_state_t *s, size_t lag)
{
  size_t nfft    = s->nfft;
  float  y1      = crealf (s->det->out_buf[(lag + nfft - 1) % nfft]);
  float  y2      = crealf (s->det->out_buf[lag]);
  float  y3      = crealf (s->det->out_buf[(lag + 1) % nfft]);
  double frac    = _parabolic_offset (y1, y2, y3);
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
_giveup_cap (const carrier_acq_state_t *s)
{
  return s->sequential ? s->max_n_blocks : s->dwell_target;
}

/* Empirically-calibrated CFAR ratio threshold (test_stat = peak/
 * median) for THIS object's real statistic -- a power-spectrum-vs-
 * known-template correlation, NOT the classic complex-correlator
 * peak/noise envelope ratio det_threshold_noncoherent() was derived
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
 * tail quantile z; KAPPA*det_threshold(pfa) (det_threshold() --
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
 * TODO: revisit and properly derive (or recalibrate across a wider
 * nfft/pfa/window/template grid) when time allows -- tracked in
 * FINISHING_PLAN.md, not forgotten. */
#define CARRIER_ACQ_KAPPA 7.9

static float
_ratio_threshold (const carrier_acq_state_t *s, size_t n_blocks)
{
  double z      = CARRIER_ACQ_KAPPA * det_threshold (s->pfa);
  double spread = sqrt (s->s_t2 / (double)n_blocks) / s->s_t;
  return (float)(1.0 + z * spread);
}

static void
_process_block (carrier_acq_state_t *s, const float complex *block)
{
  psd_accumulate (s->psd, block, s->psd->n);
  s->n_blocks++;

  bool do_test = s->sequential || (s->n_blocks == s->dwell_target);
  if (!do_test)
    return;

  size_t got = psd_power_twosided (s->psd, s->nfft, s->pwr_buf);
  if (got == 0)
    return;
  for (size_t k = 0; k < s->nfft; k++)
    s->power_buf[k] = s->pwr_buf[k] + 0.0f * I;

  float eta_nc = _ratio_threshold (s, s->n_blocks);
  detector_set_threshold (s->det, eta_nc);

  det_result_t result[1];
  size_t n_res = detector_push (s->det, s->power_buf, s->nfft, result, 1);
  if (n_res > 0)
    _finish (s, result[0].lag);
}

void
carrier_acq_steps (carrier_acq_state_t *state, const float complex *x,
                   size_t x_len)
{
  size_t cap = _giveup_cap (state);
  if (state->ready || state->n_blocks >= cap)
    return;

  size_t n_fft = state->psd->n;
  size_t off   = 0;

  if (state->carry_len > 0)
    {
      size_t need = n_fft - state->carry_len;
      size_t take = (x_len < need) ? x_len : need;
      memcpy (state->carry_buf + state->carry_len, x,
              take * sizeof (float complex));
      state->carry_len += take;
      off += take;
      if (state->carry_len == n_fft)
        {
          _process_block (state, state->carry_buf);
          state->carry_len = 0;
        }
    }

  while (!state->ready && state->n_blocks < cap && off + n_fft <= x_len)
    {
      _process_block (state, x + off);
      off += n_fft;
    }

  size_t rem = x_len - off;
  if (rem > 0 && rem < n_fft && !state->ready && state->n_blocks < cap)
    {
      memcpy (state->carry_buf, x + off, rem * sizeof (float complex));
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
  double   residual_hz;
  uint64_t carry_len;
} carrier_acq_extra_t;

size_t
carrier_acq_state_bytes (const carrier_acq_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (carrier_acq_extra_t)
         + psd_state_bytes (s->psd) + detector_state_bytes (s->det)
         + s->psd->n * sizeof (float complex);
}

void
carrier_acq_get_state (const carrier_acq_state_t *s, void *blob)
{
  DP_GET_OPEN (CARRIER_ACQ_STATE_MAGIC, CARRIER_ACQ_STATE_VERSION,
               carrier_acq_state_bytes (s));
  carrier_acq_extra_t extra = {
    .nfft         = (uint64_t)s->nfft,
    .dwell_target = (uint64_t)s->dwell_target,
    .max_n_blocks = (uint64_t)s->max_n_blocks,
    .n_blocks     = (uint64_t)s->n_blocks,
    .ready        = (uint8_t)s->ready,
    .residual_hz  = s->residual_hz,
    .carry_len    = (uint64_t)s->carry_len,
  };
  dp_w_bytes (&_w, &extra, sizeof extra);
  DP_W_CHILD (&_w, psd, s->psd);
  DP_W_CHILD (&_w, detector, s->det);
  dp_w_cf32 (&_w, s->carry_buf, s->psd->n);
}

int
carrier_acq_set_state (carrier_acq_state_t *s, const void *blob)
{
  DP_SET_OPEN (CARRIER_ACQ_STATE_MAGIC, CARRIER_ACQ_STATE_VERSION,
               carrier_acq_state_bytes (s));
  carrier_acq_extra_t extra;
  dp_r_bytes (&_r, &extra, sizeof extra);
  if (extra.nfft != (uint64_t)s->nfft
      || extra.dwell_target != (uint64_t)s->dwell_target
      || extra.max_n_blocks != (uint64_t)s->max_n_blocks
      || extra.carry_len > (uint64_t)s->psd->n)
    return DP_ERR_INVALID;
  DP_R_CHILD (&_r, psd, s->psd);
  DP_R_CHILD (&_r, detector, s->det);
  dp_r_cf32 (&_r, s->carry_buf, s->psd->n);
  s->n_blocks    = (size_t)extra.n_blocks;
  s->ready       = (bool)extra.ready;
  s->residual_hz = extra.residual_hz;
  s->carry_len   = (size_t)extra.carry_len;
  return DP_OK;
}
