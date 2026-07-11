#include "symsync/symsync_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Timing-lock statistic and sizing.
 *
 * History: two earlier designs (an EMA'd eye-concentration ratio, then
 * that ratio's EMA fed through a 1/(1+x) reciprocal) were built and
 * calibrated this same session and both looked wrong under Monte Carlo --
 * but the miscalibration turned out to be in the *test harness* (a
 * signal-generator truncation artifact that silenced the last ~30
 * symbols of every "clean" trial), not the statistics themselves. Once
 * that bug was found, a doppler user supplied the design actually
 * shipped here -- a formula they already use operationally -- which is
 * algebraically the same eye-concentration family (see below) but
 * block-averaged and sized from a closed-form (pfa, pd) derivation
 * instead of Monte-Carlo-tuned constants.
 *
 * lock_signal = 2*(|on-time|^2 - |mid|^2) / (|on-time|^2 + |mid|^2)
 *
 * -- a Gardner-style eye-opening ratio: at correct timing the on-time
 * sample sits at the eye's peak and the mid-symbol (transition-gate)
 * sample sits closer to a zero crossing, so lock_signal is positive and
 * grows with rolloff/Es-N0; under noise or wrong timing it hovers near
 * zero (confirmed against the real object for both raised-cosine and
 * rectangular pulses -- no sign flip once the test harness bug was
 * fixed, unlike what an earlier, buggy measurement this session
 * suggested).
 *
 * Sizing (symsync_configure_lock): a per-look mean is estimated from the
 * pulse rolloff and the minimum operating Es/N0,
 *
 *   mean = (0.6*rolloff+0.26)*(1 - exp(-0.275*10^(esno_min_db/10)))
 *
 * and the classic Gaussian test-statistic sizing gives the non-coherent
 * block size (avgs) and declare threshold:
 *
 *   avgs      = 2*var*((erfcinv(2*pfa)-erfcinv(2*pd))/mean)^2
 *   threshold = erfcinv(2*pfa)*mean/(erfcinv(2*pfa)-erfcinv(2*pd))
 *
 * erfcinv used directly (not the sqrt(2)*erfcinv(2p) == Q^-1(p)
 * conversion a Gaussian Q-function derivation would normally use) --
 * both formulas were supplied directly by a doppler user, not
 * re-derived against a primary source, implemented literally as given,
 * with one correction: the source formula's avgs used a bare "8" in
 * var's place, which this file originally kept unmodified. That "8"
 * turned out to be an uncalibrated placeholder, not a derived
 * constant -- see SYMSYNC_LOCK_STAT_VARIANCE below for the replacement
 * and the empirical work that picked it.
 *
 * threshold is scale-independent (var cancels out of the ratio), so it
 * is unaffected by this correction; only avgs -- and the resulting
 * declare latency -- changes.
 *
 * Empirically validated against the real object at the defaults below
 * (rolloff=0.35, esno_min=10dB, pfa=1e-3, pd=0.9 -> avgs=133,
 * threshold=0.311):
 *   - Pfa: 429 exceedances over 500,000 independent noise-only
 *     avgs-blocks (8.58e-4, right at the nominal pfa=1e-3 target with
 *     safe margin).
 *   - Pd: 2000/2000 exceedances over 2,000 independent RC-shaped-BPSK
 *     avgs-blocks at exactly the esno_min=10dB design point (nominal
 *     pd=0.9, met with large margin).
 * Both land correctly sized rather than accidentally oversized: see
 * SYMSYNC_LOCK_STAT_VARIANCE's comment for the factor-of-2 derivation
 * and the empirical elimination of a naive-but-wrong alternative.
 *
 * No down_thresh or n_up/n_down derivation is implied by the source
 * formula, so those default to the same shape dll_configure_lock uses:
 * up = down = threshold (no level hysteresis by default -- splitting
 * them needs an SNR-dependent detection probability the derivation
 * doesn't supply), n_up = 1 (declare on the very first above-threshold
 * avgs-block; the (pfa, pd) sizing already targets the desired
 * per-decision operating point, unlike Dll's default which additionally
 * compounds pfa further via a verify count), n_down = 8 (a modest time
 * hysteresis, not derived from the source formula, so a single noisy
 * block doesn't drop a live lock -- the raw escape hatch,
 * symsync_configure_lock_raw, exposes every knob for a caller that
 * wants to size these independently).
 *
 * SYMSYNC_LOCK_STAT_VARIANCE: the real per-look variance of
 * lock_signal under noise-only input, measured directly (5,000,000
 * samples, mean ~0 confirming the symmetry argument above, variance
 * ~1.343) rather than assumed. It is NOT avgs's scale factor by
 * itself, though -- the (avgs, threshold) formulas above use
 * erfcinv(2p) directly rather than the standard Gaussian
 * Q^-1(p) = sqrt(2)*erfcinv(2p); the sqrt(2) factors cancel in
 * threshold (identical either way) but not in avgs, which needs an
 * extra factor of 2 to match the classic
 * N = variance*((Q^-1(pfa)-Q^-1(pd))/mean)^2 sizing once rewritten in
 * terms of the erfcinv-based denom instead of the Q^-1-based one.
 * Both hypotheses were tried against native/validation/symsync_lock.c
 * before picking one: the measured variance alone (scale=1.343, no
 * factor of 2) blows past the pfa target by ~13x (empirical
 * 1.31e-2 vs nominal 1e-3) -- undersized avgs, not enough averaging to
 * suppress noise variance at that threshold. The factor-of-2-corrected
 * scale (2*1.343=2.686) lands right at the target (see the validation
 * numbers above). This is the reason SYMSYNC_LOCK_STAT_VARIANCE is
 * defined here as the bare measured variance, with the formula itself
 * spelling out "2*var" rather than folding the 2 into the constant --
 * collapsing them back into one opaque number is exactly the kind of
 * mismatched-units bug (an unnamed "8" standing in for an
 * undocumented combination of variance and a derivation-specific
 * factor) this fix exists to prevent recurring. */
#define SYMSYNC_LOCK_STAT_VARIANCE 1.343
#define SYMSYNC_LOCK_DEFAULT_ROLLOFF 0.35
#define SYMSYNC_LOCK_DEFAULT_ESNO_MIN_DB 10.0
#define SYMSYNC_LOCK_DEFAULT_PFA 1e-3
#define SYMSYNC_LOCK_DEFAULT_PD 0.9
#define SYMSYNC_LOCK_DEFAULT_N_UP 1u
#define SYMSYNC_LOCK_DEFAULT_N_DOWN 8u

/* Inverse complementary error function via a Winitzki (2008) rational
 * initial guess refined to ~machine precision by Newton's method on
 * erfc(x) - y = 0 (erfc() is standard C99). No house primitive for this
 * existed (detection_core.h's det_threshold() is a different, Rayleigh
 * -law statistic, not Gaussian) -- kept private to this file pending a
 * second consumer, at which point it belongs in the detection module. */
static double
erfcinv_ (double y)
{
  double x      = 1.0 - y; /* erfcinv(y) == erfinv(1 - y) */
  double ln1mx2 = log (1.0 - x * x);
  double a      = 0.147;
  double t1     = 2.0 / (M_PI * a) + ln1mx2 / 2.0;
  double inner  = t1 * t1 - ln1mx2 / a;
  double r      = sqrt (fmax (inner, 0.0)) - t1;
  double guess  = copysign (sqrt (fmax (r, 0.0)), x);
  for (int i = 0; i < 4; i++)
    {
      double fx  = erfc (guess) - y;
      double dfx = -2.0 / sqrt (M_PI) * exp (-guess * guess);
      guess -= fx / dfx;
    }
  return guess;
}

/* Nominal NCO increment: the accumulator advances by 1/sps of full scale per
 * input sample, wrapping once per symbol.  The on-time strobe is the wrap and
 * the mid-symbol strobe is the half-scale crossing, giving the Gardner
 * detector its two interpolants per symbol from one accumulator. */
static uint32_t
nominal_inc (size_t sps)
{
  double s = (double)(sps ? sps : 1);
  return (uint32_t)(4294967296.0 / s);
}

static void
seed (symsync_state_t *s)
{
  s->timing.phase     = 0;
  s->timing.phase_inc = s->base_inc;
  s->have_ontime      = 0;
  s->prev_ontime      = 0.0f;
  s->mid              = 0.0f;
  s->last_error       = 0.0;
  s->rate_est         = (double)s->sps;
  s->pwr_avg          = 1.0;
  s->lock_sum         = 0.0;
  s->lock_count       = 0;
  s->lock_stat        = 0.0;
  lockdet_reset (&s->lock); /* drop the lock; keep the configured rule */
  farrow_reset (&s->farrow);
}

void
symsync_init (symsync_state_t *s, size_t sps, double bn, double zeta,
              int order, int ted)
{
  /* Zero first so an in-place (stack-embedded) init byte-matches the
   * calloc + init done by symsync_create: seed() sets only the timing NCO's
   * phase/phase_inc, leaving the NCO's norm_freq/nmax to this memset. */
  memset (s, 0, sizeof (*s));
  s->sps      = sps ? sps : 1;
  s->bn       = bn;
  s->zeta     = zeta;
  s->base_inc = nominal_inc (s->sps);
  s->ted      = ted;
  farrow_init (&s->farrow, order);
  loop_filter_init (&s->lf, bn, zeta, 1.0); /* one update per symbol */
  (void)symsync_configure_lock (
      s, SYMSYNC_LOCK_DEFAULT_ROLLOFF, SYMSYNC_LOCK_DEFAULT_ESNO_MIN_DB,
      SYMSYNC_LOCK_DEFAULT_PFA, SYMSYNC_LOCK_DEFAULT_PD);
  seed (s);
}

symsync_state_t *
symsync_create (size_t sps, double bn, double zeta, int order, int ted)
{
  symsync_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  symsync_init (obj, sps, bn, zeta, order, ted);
  return obj;
}

void
symsync_destroy (symsync_state_t *state)
{
  free (state);
}

void
symsync_reset (symsync_state_t *state)
{
  loop_filter_reset (&state->lf);
  seed (state);
}

int
symsync_set_telemetry (symsync_state_t *state, dp_tlm_t *tlm,
                       const char *prefix, uint32_t decim)
{
  if (!tlm) /* detach: probe sites revert to the single-branch cost */
    {
      state->tlm.ctx = NULL;
      return DP_OK;
    }
  const char *p = prefix ? prefix : "sync";
  char        name[DP_TLM_NAME_MAX];
  (void)snprintf (name, sizeof (name), "%s.e", p);
  int id_e = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.freq", p);
  int id_freq = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.rate", p);
  int id_rate = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.lock", p);
  int id_lock = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.locked", p);
  int id_locked = dp_tlm_probe (tlm, name, decim);
  if (id_e < 0 || id_freq < 0 || id_rate < 0 || id_lock < 0 || id_locked < 0)
    return DP_ERR_INVALID; /* table full / bad prefix: attach fails whole */
  state->tlm.id_e      = id_e;
  state->tlm.id_freq   = id_freq;
  state->tlm.id_rate   = id_rate;
  state->tlm.id_lock   = id_lock;
  state->tlm.id_locked = id_locked;
  state->tlm.ctx       = tlm; /* set last: emit sites gate on ctx */
  return DP_OK;
}

void
symsync_tlm_flush (const symsync_state_t *s)
{
  /* The loop control isn't retained per symbol; reconstruct it from the
   * NCO increment it steered (float32 records — the uint32 rounding is
   * far below record precision). */
  double control = (double)s->timing.phase_inc / (double)s->base_inc - 1.0;
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_e, s->last_error);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_freq, control);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_rate, s->rate_est);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_lock, s->lock_stat);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_locked, (double)s->lock.locked);
}

/* Serializable state — pointer-free composition (nco + farrow + loop_filter
 * embedded by value, all POD) + scalar timing state: a whole-struct snapshot,
 * with the telemetry attachment zeroed in blobs and kept live across restore
 * (see DP_DEFINE_POD_STATE_TLM in dp_state.h). */
DP_DEFINE_POD_STATE_TLM (symsync, symsync_state_t, SYMSYNC_STATE_MAGIC,
                         SYMSYNC_STATE_VERSION, tlm)

void
symsync_configure (symsync_state_t *state, double bn, double zeta)
{
  state->bn   = bn;
  state->zeta = zeta;
  loop_filter_configure (&state->lf, bn, zeta, 1.0);
}

size_t
symsync_steps_max_out (symsync_state_t *state)
{
  (void)state;
  return 0;
}

size_t
symsync_steps (symsync_state_t *state, const float complex *x, size_t x_len,
               float complex *out, size_t max_out)
{
  size_t        emitted = 0;
  float complex y;
  /* The TED selection is hoisted out of the hot loop: a literal ted lets
   * the force-inlined step constant-fold the detector branch, so each
   * specialised loop body carries exactly one TED. The runtime `s->ted`
   * branch inside the loop kept both detector bodies live across the
   * per-sample path and measured ~30% slower at 64k blocks. */
  /* The telemetry check is hoisted to loop entry (attach is setup-time
   * only — SPSC contract), so the detached loops contain NO call site:
   * an extern call inside the loop forces the compiler to assume every
   * state field is clobbered per iteration, spilling the register-cached
   * NCO/Farrow hot state (measured ~20% slower detached even though the
   * call never executed). */
  if (!state->tlm.ctx)
    {
      if (state->ted == SYMSYNC_TED_DTTL)
        {
          for (size_t n = 0; n < x_len; n++)
            if (symsync_step_ted (state, x[n], &y, SYMSYNC_TED_DTTL)
                && emitted < max_out)
              out[emitted++] = y;
        }
      else
        {
          for (size_t n = 0; n < x_len; n++)
            if (symsync_step_ted (state, x[n], &y, SYMSYNC_TED_GARDNER)
                && emitted < max_out)
              out[emitted++] = y;
        }
    }
  else if (state->ted == SYMSYNC_TED_DTTL)
    {
      for (size_t n = 0; n < x_len; n++)
        if (symsync_step_ted (state, x[n], &y, SYMSYNC_TED_DTTL))
          {
            if (emitted < max_out)
              out[emitted++] = y;
            symsync_tlm_flush (state);
          }
    }
  else
    {
      for (size_t n = 0; n < x_len; n++)
        if (symsync_step_ted (state, x[n], &y, SYMSYNC_TED_GARDNER))
          {
            if (emitted < max_out)
              out[emitted++] = y;
            symsync_tlm_flush (state);
          }
    }
  return emitted;
}

double
symsync_get_bn (const symsync_state_t *state)
{
  return state->bn;
}

void
symsync_set_bn (symsync_state_t *state, double val)
{
  symsync_configure (state, val, state->zeta);
}

double
symsync_get_timing_error (const symsync_state_t *state)
{
  return state->last_error;
}

double
symsync_get_rate (const symsync_state_t *state)
{
  /* effective samples/symbol the loop is tracking (EMA of the instantaneous
   * strobe rate) */
  return state->rate_est;
}

double
symsync_get_lock_stat (const symsync_state_t *state)
{
  return state->lock_stat;
}

int
symsync_get_locked (const symsync_state_t *state)
{
  return state->lock.locked;
}

int
symsync_configure_lock (symsync_state_t *state, double rolloff,
                        double esno_min_db, double pfa, double pd)
{
  if (!(pfa > 0.0 && pfa < 1.0))
    return DP_ERR_INVALID;
  if (!(pd > 0.0 && pd < 1.0 && pd > pfa))
    return DP_ERR_INVALID;
  double mean = (0.6 * rolloff + 0.26)
                * (1.0 - exp (-0.275 * pow (10.0, esno_min_db / 10.0)));
  /* erfcinv used directly, matching the source formula literally -- no
   * sqrt(2) Q-function conversion applied (see this file's top-of-block
   * comment: implemented as given, not re-derived). The leading 2.0
   * corrects for that same missing sqrt(2): it is not part of the
   * variance and must not be folded into SYMSYNC_LOCK_STAT_VARIANCE
   * (see that constant's comment). */
  double qi_pfa = erfcinv_ (2.0 * pfa);
  double qi_pd  = erfcinv_ (2.0 * pd);
  double denom  = qi_pfa - qi_pd;
  double avgs_f
      = 2.0 * SYMSYNC_LOCK_STAT_VARIANCE * (denom / mean) * (denom / mean);
  size_t avgs = (size_t)ceil (avgs_f);
  if (avgs < 1)
    avgs = 1;
  double threshold = qi_pfa * mean / denom;
  symsync_configure_lock_raw (state, avgs, threshold, threshold,
                              SYMSYNC_LOCK_DEFAULT_N_UP,
                              SYMSYNC_LOCK_DEFAULT_N_DOWN);
  return DP_OK;
}

void
symsync_configure_lock_raw (symsync_state_t *state, size_t avgs,
                            double up_thresh, double down_thresh,
                            uint32_t n_up, uint32_t n_down)
{
  state->avgs       = avgs ? avgs : 1;
  state->lock_sum   = 0.0;
  state->lock_count = 0;
  lockdet_configure (&state->lock, up_thresh, down_thresh, n_up, n_down);
}
