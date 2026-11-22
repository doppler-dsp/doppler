#include "doppler/carrier_nda/carrier_nda_core.h"
#include "doppler/detection/detection_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default carrier lock-detector rule (see dp_carrier_nda_configure_lock's
 * header doc). up_thresh/down_thresh (0.5/0.4) are a hysteresis band on this
 * `lock` statistic. n_up cannot be chosen by the usual "consecutive looks
 * compound independently" argument (~p^n_up): `lock` is a fast per-sample
 * EMA (CARRIER_NDA_LOCK_ALPHA, ~20-sample memory), so consecutive samples are
 * highly autocorrelated and that assumption does not hold. Direct Monte
 * Carlo against the real object (noise-only, no carrier) measured the risk
 * before shipping:
 *   n_up=8:                         4/30 trials falsely locked
 *   n_up=16:                        7/100 trials falsely locked
 *   n_up=32:                        1/100 trials falsely locked
 *   n_up=64:                        0/300 trials falsely locked
 * n_up=64 is the shipped default (real carrier still locks reliably,
 * 20/20 trials at ~15 dB) -- 64 samples is a small fraction of a
 * typical acquisition window, so this buys reliability at negligible
 * latency cost. */
#define CARRIER_NDA_LOCK_DEFAULT_UP 0.5
#define CARRIER_NDA_LOCK_DEFAULT_DOWN 0.4
#define CARRIER_NDA_LOCK_DEFAULT_N_UP 64u
#define CARRIER_NDA_LOCK_DEFAULT_N_DOWN 32u

/* Seed the loop integrator so the per-sample frequency estimate (lf.integ,
 * rad/sample) matches the requested carrier offset, and point the NCO at the
 * same frequency — de-rotation is correct from the first sample, before any
 * update runs. */
static void
seed (dp_carrier_nda_state_t *s, double init_norm_freq)
{
  dp_lo_init (&s->nco,
              init_norm_freq); /* centre freq lives in nco.phase_inc */
  s->lf.integ = 0.0;           /* loop filter integrates the correction
                                  from zero; the NCO control port adds it
                                  on top of the centre each sample.      */
  s->ctl_cyc = 0.0;
  /* The boxcar arm starts at unit gain; dp_boxcar_init clears its whole fixed
   * ring, so the pointer-free POD snapshot is deterministic regardless of
   * allocation — which is what dp_carrier_nda_init exists for: an embedder can
   * hold this by value, with no calloc, and still get identical state bytes.
   */
  dp_boxcar_init (&s->arm, s->arm_len, 1.0);
  s->lock       = 0.0;
  s->last_error = 0.0;
  /* The options' running sums restart; their settings are config. */
  s->pow_avg = s->pow_n = 0.0;
  s->blk_n              = 0;
  s->blk_num = s->blk_den = 0.0;
  dp_lockdet_reset (&s->lockdet); /* drop the lock; keep the configured rule */
}

/* Configure the carrier PI loop for the current (bn, zeta) and scale its gains
 * by 1/(2*pi) so the loop filter natively outputs the NCO's unit (cycles per
 * sample) from the discriminator's radian phase error. The whole loop then
 * runs in NCO cycles: carrier_nda_steer is a pure dp_loop_filter_step (no
 * per-sample conversion) and lf.integ is the frequency correction in
 * cycles/sample.
 *
 * bn is the loop noise bandwidth in cycles/sample. The moving-average arm
 * emits one sample per input sample, so the loop updates every sample (period
 * t = 1); n only sets the window length, so the gains are n-invariant. Folding
 * the radian->cycle constant into the gains leaves the open-loop product (disc
 * slope x gains x NCO gain) unchanged, so the dynamics are identical. */
static int
config_loop (dp_carrier_nda_state_t *s, double bn, double zeta)
{
  /* Scaled, and bn/zeta stored, only once the loop filter took them: it
     refuses outside its domain and writes nothing, and scaling its
     unchanged gains anyway divided the live loop by 2 pi again on every
     refused retune while the getter reported the refused value
     (doppler#2112). */
  if (dp_loop_filter_configure (&s->lf, bn, zeta, 1.0) != DP_OK)
    return DP_ERR_INVALID;
  s->lf.kp *= CARRIER_NDA_INV_2PI;
  s->lf.ki *= CARRIER_NDA_INV_2PI;
  s->bn   = bn;
  s->zeta = zeta;
  return DP_OK;
}

void
dp_carrier_nda_init (dp_carrier_nda_state_t *s, double bn, double zeta,
                     double init_norm_freq, size_t sps, int n, int m)
{
  s->sps     = sps ? sps : 1;
  s->m       = m;
  s->n       = n > 0 ? n : 1;
  s->arm_len = s->sps / (size_t)s->n;
  if (s->arm_len == 0)
    s->arm_len = 1;
  s->seed_norm_freq = init_norm_freq;
  /* In-place (stack/by-value-embedded) init: start detached. seed() is the
   * reset path and deliberately never touches `tlm`, so a reset keeps a live
   * attachment; that leaves init responsible for the initial zero.
   * dp_carrier_nda_create's calloc gets this for free. */
  memset (&s->tlm, 0, sizeof s->tlm);
  /* The composer's options, all off: one update a sample, the limiter, the
     lock EMA. */
  s->t_upd     = 1.0;
  s->den       = CARRIER_NDA_DEN_LIMITER;
  s->den_alpha = 1.0;
  s->den_gamma = 0.0;
  s->blk_len   = 0;
  s->blk_thr   = 0.0;
  s->blk_pfa   = 0.0;
  s->opt_zero  = 0;
  /* By-value init is unguarded: the caller checks (bn, zeta) with
     dp_loop_filter_params_ok() first, as dp_carrier_nda_create() does. */
  (void)config_loop (s, bn, zeta);
  dp_lockdet_init (
      &s->lockdet, CARRIER_NDA_LOCK_DEFAULT_UP, CARRIER_NDA_LOCK_DEFAULT_DOWN,
      CARRIER_NDA_LOCK_DEFAULT_N_UP, CARRIER_NDA_LOCK_DEFAULT_N_DOWN);
  seed (s, init_norm_freq);
}

dp_carrier_nda_state_t *
dp_carrier_nda_create (double bn, double zeta, double init_norm_freq,
                       size_t sps, int n, int m)
{
  if (m != 2 && m != 4 && m != 8)
    return NULL; /* only BPSK / QPSK / 8PSK */
  if (sps == 0 || n <= 0 || sps % (size_t)n != 0)
    return NULL; /* arm length must be a whole number of samples */
  if (sps / (size_t)n > BOXCAR_MAX_LEN)
    return NULL; /* boxcar arm window is a fixed in-struct ring */
  /* The loop filter's own domain, refused before allocating: a NaN or an
     overflowing bandwidth gave gains that never recover (doppler#2112). */
  if (!dp_loop_filter_params_ok (bn, zeta, 1.0))
    return NULL;
  dp_carrier_nda_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  dp_carrier_nda_init (obj, bn, zeta, init_norm_freq, sps, n, m);
  return obj;
}

void
dp_carrier_nda_destroy (dp_carrier_nda_state_t *state)
{
  free (state);
}

void
dp_carrier_nda_reset (dp_carrier_nda_state_t *state)
{
  dp_loop_filter_reset (&state->lf);
  seed (state, state->seed_norm_freq);
}

int
dp_carrier_nda_set_telemetry (dp_carrier_nda_state_t *state, dp_tlm_t *tlm,
                              const char *prefix, uint32_t decim)
{
  if (!tlm)
    {
      state->tlm.ctx = NULL;
      return DP_OK;
    }
  const char              *p      = prefix ? prefix : "car";
  static const char *const sfx[5] = { "lock", "e", "freq", "locked", "nco" };
  char                     name[5][DP_TLM_NAME_MAX];
  for (size_t i = 0; i < 5; i++) /* all names before any registration */
    if (dp_tlm_name_join (name[i], p, sfx[i]) != DP_OK)
      return DP_ERR_INVALID; /* overlong prefix: nothing registered */
  int id_lock   = dp_tlm_probe (tlm, name[0], decim);
  int id_e      = dp_tlm_probe (tlm, name[1], decim);
  int id_freq   = dp_tlm_probe (tlm, name[2], decim);
  int id_locked = dp_tlm_probe (tlm, name[3], decim);
  int id_nco    = dp_tlm_probe (tlm, name[4], decim);
  if (id_lock < 0 || id_e < 0 || id_freq < 0 || id_locked < 0 || id_nco < 0)
    return DP_ERR_INVALID; /* table full: attach fails whole */
  state->tlm.id_lock   = id_lock;
  state->tlm.id_e      = id_e;
  state->tlm.id_freq   = id_freq;
  state->tlm.id_locked = id_locked;
  state->tlm.id_nco    = id_nco;
  state->tlm.ctx       = tlm; /* set last: emit sites gate on ctx */
  return DP_OK;
}

void
dp_carrier_nda_tlm_flush (const dp_carrier_nda_state_t *s)
{
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_lock, s->lock);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_e, s->last_error);
  /* Tracked carrier = NCO centre + the loop's integrated correction
   * (cycles/sample) — the same reconstruction as get_norm_freq. */
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_freq, dp_carrier_nda_get_norm_freq (s));
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_locked, (double)s->lockdet.locked);
  /* The full command the LO applies a sample: summed over an update's
     samples, the phase it advanced by. */
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_nco, dp_carrier_nda_get_nco_freq (s));
}

/* A snapshot this object could hold: its embedded loop filter's gains are
 * the ones its bn, zeta and t give, its boxcar arm writes `ring[pos]`
 * (dp_boxcar_state_ok) and its lock detector's verdict is 0 or 1
 * (dp_lockdet_state_ok). A forged arm wrote past the struct (#2142).
 *
 * config_loop() folds 1/2pi into the filter's gains, so the stored ones are
 * the loop filter's own divided by it. The question dp_loop_filter_state_ok
 * answers is about the filter's own gains, so it is asked of a copy with the
 * fold undone, rather than a second gains check living here. It is the bare
 * predicate: every path that sets the filter (create, init, set_bn) goes
 * through dp_loop_filter_configure's domain check, so a zeroed filter is a
 * forgery that freezes the loop. A composer that embeds one by value (the
 * async receiver's car and car_held) initializes it before any blob is
 * taken, so a zeroed embedding is refused, not read as an unseeded loop. */
/* The block lock's threshold on R at a Pfa, dp_det_ratio_threshold()'s:
   set_block_lock sets it and the state check recomputes it. */
static double
carrier_nda_block_thresh (uint32_t len, double pfa)
{
  return dp_det_ratio_threshold (pfa, (int)len);
}

static int
carrier_nda_state_ok (const dp_carrier_nda_state_t *s)
{
  dp_loop_filter_state_t lf = s->lf;
  lf.kp /= CARRIER_NDA_INV_2PI;
  lf.ki /= CARRIER_NDA_INV_2PI;
  /* `m` is the order create() takes, and the retained bn/zeta the getters
     report are the loop's own, as config_loop() stores them (#2022). */
  /* The options as their setters, and an update, could leave them:
     - a period >= 1;
     - a known denominator with its own weight and design SNR in range (M =
       2 for every form but the limiter), and a running average that is
       finite and >= 0, and zero with its count while the limiter is in
       force;
     - a block lock of 2 to CARRIER_NDA_BLOCK_MAX_LEN updates whose count is
       inside it, whose sums are what updates leave (finite, den >= 0,
       |num| <= den, zero at a block's start), whose threshold is the one
       its (len, pfa) gives and is the rule the lock detector holds; and
       with the block lock off, nothing of it left.
     The threshold is compared within 1e-9 relative, not bit for bit: it
     comes out of a bisection over lgamma and a continued fraction, so a
     blob written on one libm and restored on another may differ by ulps,
     and 1e-9 is far under any difference a Pfa would notice. */
  const int den_ok
      = s->den_alpha > 0.0 && s->den_alpha <= 1.0 && isfinite (s->pow_avg)
        && s->pow_avg >= 0.0 && isfinite (s->pow_n) && s->pow_n >= 0.0
        && (s->den == CARRIER_NDA_DEN_LIMITER
                ? s->den_alpha == 1.0 && s->den_gamma == 0.0
                      && s->pow_avg == 0.0 && s->pow_n == 0.0
            : s->den == CARRIER_NDA_DEN_AVERAGED
                ? s->m == 2 && s->den_alpha < 1.0 && s->den_gamma == 0.0
            : s->den == CARRIER_NDA_DEN_UNIT
                ? s->m == 2 && isfinite (s->den_gamma) && s->den_gamma > 0.0
                : 0);
  int blk_ok;
  if (s->blk_len == 0)
    blk_ok = s->blk_n == 0 && s->blk_thr == 0.0 && s->blk_pfa == 0.0
             && s->blk_num == 0.0 && s->blk_den == 0.0;
  else
    {
      const double r = s->m == 2 && s->blk_len >= 2
                               && s->blk_len <= CARRIER_NDA_BLOCK_MAX_LEN
                               && s->blk_pfa > 0.0 && s->blk_pfa < 1.0
                           ? carrier_nda_block_thresh (s->blk_len, s->blk_pfa)
                           : NAN;
      blk_ok = isfinite (r) && s->blk_n < s->blk_len
               && fabs (s->blk_thr - r) <= 1e-9 * fabs (r)
               && s->lockdet.up_thresh == s->blk_thr
               && s->lockdet.down_thresh == s->blk_thr && isfinite (s->blk_num)
               && isfinite (s->blk_den) && s->blk_den >= 0.0
               && fabs (s->blk_num) <= s->blk_den * (1.0 + 1e-12)
               && (s->blk_n > 0 || (s->blk_num == 0.0 && s->blk_den == 0.0));
    }
  return (s->m == 2 || s->m == 4 || s->m == 8) && dp_loop_filter_state_ok (&lf)
         && s->bn == s->lf.bn && s->zeta == s->lf.zeta
         && dp_boxcar_state_ok (&s->arm) && dp_lockdet_state_ok (&s->lockdet)
         && isfinite (s->t_upd) && s->t_upd >= 1.0 && s->opt_zero == 0
         && (s->den == CARRIER_NDA_DEN_LIMITER
             || s->den == CARRIER_NDA_DEN_AVERAGED
             || s->den == CARRIER_NDA_DEN_UNIT)
         && den_ok && blk_ok;
}

/* Serializable state — pointer-free POD whole-struct snapshot, with the
 * telemetry attachment zeroed in blobs and kept live across restore, and a
 * snapshot carrier_nda_state_ok() rejects refused untouched. */
DP_DEFINE_POD_STATE_TLM_CHECKED (dp_carrier_nda, dp_carrier_nda_state_t,
                                 CARRIER_NDA_STATE_MAGIC,
                                 CARRIER_NDA_STATE_VERSION, tlm,
                                 carrier_nda_state_ok)

/* Output bound: emitted samples == input length (the de-rotated stream). */
size_t
dp_carrier_nda_steps_max_out (dp_carrier_nda_state_t *state)
{
  (void)state;
  return 0; /* one symbol per sps inputs, so symbols <= inputs */
}

size_t
dp_carrier_nda_steps (dp_carrier_nda_state_t *state, const float _Complex *x,
                      size_t x_len, float _Complex *out, size_t max_out)
{
  size_t emitted = 0;
  /* steps() is the per-sample loop: an update every sample. A composer's
     period is the composition API's (set_update_period). */
  if (state->t_upd != 1.0)
    return 0;
  /* The telemetry check is hoisted to loop entry (attach is setup-time
   * only — SPSC contract), so the detached loop contains NO call site:
   * an extern call inside the loop forces the compiler to assume every
   * state field is clobbered per iteration, spilling the register-cached
   * NCO/arm hot state (measured ~20% slower detached on the symsync
   * loops even though the call never executed). This loop updates every
   * sample, so the attached loop flushes per sample — the per-probe
   * decim set at attach is the throttle. */
  if (!state->tlm.ctx)
    {
      for (size_t i = 0; i < x_len; i++)
        {
          float _Complex d = carrier_nda_wipeoff (state, x[i]);
          /* The arm's moving average, then one update: at the defaults,
             carrier_nda_disc(), the lock EMA and its decision, the steer. */
          carrier_nda_update (state, dp_boxcar_step (&state->arm, d), 0.0);
          if (emitted < max_out)
            out[emitted++] = d;
        }
    }
  else
    {
      for (size_t i = 0; i < x_len; i++)
        {
          float _Complex d = carrier_nda_wipeoff (state, x[i]);
          /* The arm's moving average, then one update: at the defaults,
             carrier_nda_disc(), the lock EMA and its decision, the steer. */
          carrier_nda_update (state, dp_boxcar_step (&state->arm, d), 0.0);
          if (emitted < max_out)
            out[emitted++] = d;
          dp_carrier_nda_tlm_flush (state);
        }
    }
  return emitted;
}

double
dp_carrier_nda_get_norm_freq (const dp_carrier_nda_state_t *state)
{
  /* Tracked carrier = NCO centre (nco.norm_freq) + the loop's integrated
   * frequency correction (lf.integ, already in cycles/sample — the loop gains
   * carry the rad->cycle scale, see config_loop). This is the SMOOTHED
   * (integrator-only) frequency estimate: under a frequency ramp it lags the
   * true carrier by the constant Type-II ramp error (the standing phase error
   * lives in the omitted proportional path — see dp_carrier_nda_get_nco_freq).
   */
  return state->nco.norm_freq + state->lf.integ / state->t_upd;
}

double
dp_carrier_nda_get_nco_freq (const dp_carrier_nda_state_t *state)
{
  /* The instantaneous NCO frequency command = centre + the FULL loop-filter
   * output (ctl_cyc = integ + kp*e, cycles/sample), i.e. the exact frequency
   * carrier_nda_wipeoff de-rotates by this sample. Unlike get_norm_freq's
   * integrator, this includes the proportional term, so its MEAN rides a
   * frequency ramp with no lag; its per-sample variance IS the loop stress.
   * The right readout for observing the loop track dynamics; not a smoothed
   * estimate. */
  return state->nco.norm_freq + state->ctl_cyc;
}

void
dp_carrier_nda_set_norm_freq (dp_carrier_nda_state_t *state, double val)
{
  state->seed_norm_freq = val;
  dp_loop_filter_reset (&state->lf);
  seed (state, val);
}

double
dp_carrier_nda_get_lock (const dp_carrier_nda_state_t *state)
{
  return state->lock;
}

double
dp_carrier_nda_get_last_error (const dp_carrier_nda_state_t *state)
{
  return state->last_error;
}

double
dp_carrier_nda_get_bn (const dp_carrier_nda_state_t *state)
{
  return state->bn;
}

int
dp_carrier_nda_set_bn (dp_carrier_nda_state_t *state, double val)
{
  return config_loop (state, val, state->zeta);
}

int
dp_carrier_nda_get_m (const dp_carrier_nda_state_t *state)
{
  return state->m;
}

int
dp_carrier_nda_get_n (const dp_carrier_nda_state_t *state)
{
  return state->n;
}

size_t
dp_carrier_nda_get_sps (const dp_carrier_nda_state_t *state)
{
  return state->sps;
}

void
dp_carrier_nda_configure_lock (dp_carrier_nda_state_t *state, double up_thresh,
                               double down_thresh, uint32_t n_up,
                               uint32_t n_down)
{
  /* The EMA's rule: a block lock in force gives way to it, so the reported
     block_thresh and the rule deciding never disagree. */
  if (state->blk_len)
    {
      state->blk_len = state->blk_n = 0;
      state->blk_thr = state->blk_pfa = 0.0;
      state->blk_num = state->blk_den = 0.0;
      state->lock                     = 0.0;
    }
  dp_lockdet_configure (&state->lockdet, up_thresh, down_thresh, n_up, n_down);
}

int
dp_carrier_nda_get_locked (const dp_carrier_nda_state_t *state)
{
  return state->lockdet.locked;
}

int
dp_carrier_nda_set_update_period (dp_carrier_nda_state_t *state, double t_upd)
{
  if (!(isfinite (t_upd) && t_upd >= 1.0))
    return DP_ERR_INVALID;
  /* bn and zeta are kept and now mean per update; the filter's gains do
     not depend on the period (the loop is designed at t = 1 an update), so
     only the spread over the update's samples changes. The integrator holds
     cycles an update: rescaled by new/old it keeps the tracked frequency,
     integ / t_upd cycles a sample, through the call. ctl_cyc is already
     cycles a sample and stays. */
  state->lf.integ *= t_upd / state->t_upd;
  state->t_upd = t_upd;
  return DP_OK;
}

int
dp_carrier_nda_set_denominator (dp_carrier_nda_state_t *state, double alpha,
                                double gamma_d)
{
  if (!(alpha > 0.0 && alpha <= 1.0)
      || !(isfinite (gamma_d) && gamma_d >= 0.0))
    return DP_ERR_INVALID;
  /* alpha 1 with no noise subtracted is one update's own power: the limiter,
     kept on carrier_nda_disc()'s exact arithmetic. */
  const int den = gamma_d > 0.0 ? CARRIER_NDA_DEN_UNIT
                  : alpha < 1.0 ? CARRIER_NDA_DEN_AVERAGED
                                : CARRIER_NDA_DEN_LIMITER;
  if (den != CARRIER_NDA_DEN_LIMITER && state->m != 2)
    return DP_ERR_INVALID;
  state->den       = den;
  state->den_alpha = alpha;
  state->den_gamma = gamma_d;
  state->pow_avg = state->pow_n = 0.0;
  return DP_OK;
}

int
dp_carrier_nda_set_block_lock (dp_carrier_nda_state_t *state, uint32_t len,
                               double pfa, uint32_t n_up, uint32_t n_down)
{
  if (len == 0)
    {
      state->blk_len = 0;
      state->blk_thr = state->blk_pfa = 0.0;
      dp_lockdet_configure (&state->lockdet, CARRIER_NDA_LOCK_DEFAULT_UP,
                            CARRIER_NDA_LOCK_DEFAULT_DOWN,
                            CARRIER_NDA_LOCK_DEFAULT_N_UP,
                            CARRIER_NDA_LOCK_DEFAULT_N_DOWN);
    }
  else
    {
      if (state->m != 2 || len < 2 || len > CARRIER_NDA_BLOCK_MAX_LEN
          || !(pfa > 0.0 && pfa < 1.0))
        return DP_ERR_INVALID;
      state->blk_len = len;
      state->blk_pfa = pfa;
      state->blk_thr = carrier_nda_block_thresh (len, pfa);
      dp_lockdet_configure (&state->lockdet, state->blk_thr, state->blk_thr,
                            n_up, n_down);
    }
  state->blk_n   = 0;
  state->blk_num = state->blk_den = 0.0;
  state->lock                     = 0.0;
  dp_lockdet_reset (&state->lockdet);
  return DP_OK;
}

double
dp_carrier_nda_get_block_thresh (const dp_carrier_nda_state_t *state)
{
  return state->blk_thr;
}

void
dp_carrier_nda_hold (dp_carrier_nda_state_t *state)
{
  /* The integrator's frequency, cycles a sample: the proportional term was
     one update's phase step and is not repeated. */
  state->ctl_cyc = state->lf.integ / state->t_upd;
}

dp_lockdet_rule_t
dp_carrier_nda_get_lock_rule (const dp_carrier_nda_state_t *state)
{
  return dp_lockdet_rule (&state->lockdet);
}

double
dp_carrier_nda_get_nco_phase_ahead (const dp_carrier_nda_state_t *state,
                                    size_t                        ahead)
{
  /* Unsigned wrap is the NCO's modulus; the increment is the one
     lo_step_ctrl() adds each sample until the next steer. */
  const uint32_t inc
      = state->nco.phase_inc + nco_norm_freq_to_inc (state->ctl_cyc);
  return nco_word_to_norm (state->nco.phase + (uint32_t)ahead * inc);
}
