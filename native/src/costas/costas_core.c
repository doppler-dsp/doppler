#include "doppler/costas/costas_core.h"
#include "doppler/nco/nco_core.h"

#include <stdio.h>
#include <string.h>

/* Default carrier lock-detector rule (see dp_costas_configure_lock's header
 * doc for the full derivation). Under H0 the |Re P|/|P| metric is
 * |cos(theta)| for uniform theta: mean 2/pi (~0.637), per-symbol std
 * ~0.31, reduced to ~0.071 by the COSTAS_LOCK_ALPHA = 0.1 EMA — so the
 * declare threshold sits ~3 sigma above the no-carrier mean and the drop
 * threshold ~2 sigma, with declare-fast / drop-reluctant verify counts
 * (the EMA correlates adjacent looks; the counts guard band-edge dwell). */
#define COSTAS_LOCK_DEFAULT_UP 0.85
#define COSTAS_LOCK_DEFAULT_DOWN 0.78
#define COSTAS_LOCK_DEFAULT_N_UP 8u
#define COSTAS_LOCK_DEFAULT_N_DOWN 32u

/* Seed the loop integrator so the per-symbol frequency estimate
 * (lf.integ / tsamps, rad/sample) matches the requested carrier offset,
 * and point the NCO at the same frequency — de-rotation is correct from
 * the first sample, before any update runs. */
static void
seed (dp_costas_state_t *s, double init_norm_freq)
{
  dp_lo_init (&s->nco, init_norm_freq);
  s->lf.integ = init_norm_freq * 2.0 * M_PI * (double)s->tsamps;
  if (s->order == 3) /* the active filter starts at the seed, rate 0 */
    {
      s->sf.integ = s->lf.integ;
      s->sf.rate  = 0.0;
    }
  s->acc         = 0.0f;
  s->acc_n       = 0;
  s->prev        = 0.0f;
  s->have_prev   = 0;
  s->lock_metric = 0.0;
  s->last_error  = 0.0;
  dp_lockdet_reset (&s->lock); /* drop the lock; keep the configured rule */
}

void
dp_costas_init (dp_costas_state_t *s, double bn, double zeta,
                double init_norm_freq, size_t tsamps, double bn_fll)
{
  s->tsamps         = tsamps ? tsamps : 1;
  s->bn             = bn;
  s->zeta           = zeta;
  s->bn_fll         = bn_fll;
  s->k_fll          = 4.0 * bn_fll; /* 1st-order FLL aiding gain */
  s->seed_norm_freq = init_norm_freq;
  /* In-place (stack-embedded) init: start detached — dp_costas_create's calloc
   * gets this for free, a caller-owned struct would otherwise carry a
   * garbage telemetry pointer into the emit gates. */
  memset (&s->tlm, 0, sizeof s->tlm);
  s->_pad = 0; /* the struct is the blob: no byte of it left undefined */
  /* Order 2, the third-order filter all zero until selected. */
  memset (&s->sf, 0, sizeof s->sf);
  s->order      = 2;
  s->_pad_order = 0;
  dp_loop_filter_init (&s->lf, bn, zeta, 1.0); /* updates once per symbol */
  dp_lockdet_init (&s->lock, COSTAS_LOCK_DEFAULT_UP, COSTAS_LOCK_DEFAULT_DOWN,
                   COSTAS_LOCK_DEFAULT_N_UP, COSTAS_LOCK_DEFAULT_N_DOWN);
  seed (s, init_norm_freq);
}

dp_costas_state_t *
dp_costas_create (double bn, double zeta, double init_norm_freq, size_t tsamps,
                  double bn_fll)
{
  /* The loop filter's own domain, refused before allocating: a NaN or an
     overflowing bandwidth gave gains that never recover (doppler#2112). */
  if (!dp_loop_filter_params_ok (bn, zeta, 1.0))
    return NULL;
  if (!dp_loop_filter_fll_bn_ok (bn_fll))
    return NULL; /* doppler#2646: a negative/NaN/inf FLL bandwidth */
  dp_costas_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  dp_costas_init (obj, bn, zeta, init_norm_freq, tsamps, bn_fll);
  return obj;
}

void
dp_costas_destroy (dp_costas_state_t *state)
{
  free (state);
}

void
dp_costas_reset (dp_costas_state_t *state)
{
  /* The order is configuration, kept as bn and zeta are; the estimates
     rewind to the seed (rate 0). */
  dp_loop_filter_reset (&state->lf);
  if (state->order == 3)
    dp_state_filter_reset (&state->sf);
  seed (state, state->seed_norm_freq);
}

int
dp_costas_set_telemetry (dp_costas_state_t *state, dp_tlm_t *tlm,
                         const char *prefix, uint32_t decim)
{
  if (!tlm) /* detach: probe sites revert to the single-branch cost */
    {
      state->tlm.ctx = NULL;
      return DP_OK;
    }
  const char              *p = prefix ? prefix : "car";
  static const char *const sfx[7]
      = { "lock", "e", "freq", "locked", "nco", "freq_rate", "order" };
  char name[7][DP_TLM_NAME_MAX];
  for (size_t i = 0; i < 7; i++) /* all names before any registration */
    if (dp_tlm_name_join (name[i], p, sfx[i]) != DP_OK)
      return DP_ERR_INVALID; /* overlong prefix: nothing registered */
  int id_lock   = dp_tlm_probe (tlm, name[0], decim);
  int id_e      = dp_tlm_probe (tlm, name[1], decim);
  int id_freq   = dp_tlm_probe (tlm, name[2], decim);
  int id_locked = dp_tlm_probe (tlm, name[3], decim);
  int id_nco    = dp_tlm_probe (tlm, name[4], decim);
  int id_frate  = dp_tlm_probe (tlm, name[5], decim);
  int id_order  = dp_tlm_probe (tlm, name[6], decim);
  if (id_lock < 0 || id_e < 0 || id_freq < 0 || id_locked < 0 || id_nco < 0
      || id_frate < 0 || id_order < 0)
    return DP_ERR_INVALID; /* table full: attach fails whole */
  state->tlm.id_nco       = id_nco;
  state->tlm.id_freq_rate = id_frate;
  state->tlm.id_order     = id_order;
  state->tlm.id_lock      = id_lock;
  state->tlm.id_e         = id_e;
  state->tlm.id_freq      = id_freq;
  state->tlm.id_locked    = id_locked;
  state->tlm.ctx          = tlm; /* set last: emit sites gate on ctx */
  return DP_OK;
}

void
dp_costas_tlm_flush (const dp_costas_state_t *s)
{
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_lock, s->lock_metric);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_e, s->last_error);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_freq, s->nco.norm_freq);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_locked, (double)s->lock.locked);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_nco, dp_costas_get_nco_freq (s));
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_freq_rate, dp_costas_get_norm_rate (s));
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_order, (double)dp_costas_get_order (s));
}

/* A snapshot this object could hold: its embedded loop filter's gains are
 * the ones its bn, zeta and t give (the check LoopFilter's own restore
 * makes), its detector's verdict is 0 or 1 (dp_lockdet_state_ok, #2142), and
 * the rest is below. */
static int
costas_state_ok (const dp_costas_state_t *s)
{
  /* Everything create() and the setters refuse is refused here too, by the
     same predicates (#2022). A receiver holds its Costas loops all-zero
     until it seeds them, and that state round-trips, so `tsamps` is at
     least 1 OR the loop filter is the all-zero one: embedded_ok without
     state_ok is exactly the all-zero struct. The retained bn/zeta the
     getters report equal the loop's own, and the FLL assist is one
     set_bn_fll() would take, with its derived gain (0 for the zero state). */
  const int seeded = s->tsamps >= 1;
  const int lf_ok  = seeded ? dp_loop_filter_state_ok (&s->lf)
                            : dp_loop_filter_embedded_ok (&s->lf)
                                  && !dp_loop_filter_state_ok (&s->lf);
  /* The order (dp_costas_set_order): 2 or 3, or 0 in the all-zero loop a
     receiver holds before it seeds it; the third-order filter is
     the all-zero one it starts as, or one a live filter could hold; at
     order 3 it runs at the retained bn and the FLL assist is off. */
  static const dp_state_filter_state_t zero_sf;
  const int sf_zero = memcmp (&s->sf, &zero_sf, sizeof zero_sf) == 0;
  const int sf_ok   = sf_zero || dp_state_filter_state_ok (&s->sf);
  const int order_ok
      = s->_pad_order == 0
        && ((s->order == 0 && !seeded && sf_zero) || s->order == 2
            || (s->order == 3 && s->sf.bn == s->bn && s->sf.t == 1.0
                && s->bn_fll == 0.0));
  return lf_ok && s->bn == s->lf.bn && s->zeta == s->lf.zeta
         && dp_loop_filter_fll_bn_ok (s->bn_fll) && s->k_fll == 4.0 * s->bn_fll
         && dp_lockdet_state_ok (&s->lock) && sf_ok && order_ok;
}

/* Serializable state — pointer-free POD whole-struct snapshot, with the
 * telemetry attachment zeroed in blobs and kept live across restore, and a
 * snapshot costas_state_ok() rejects refused untouched (see
 * DP_DEFINE_POD_STATE_TLM_CHECKED in dp_state.h). */
DP_DEFINE_POD_STATE_TLM_CHECKED (dp_costas, dp_costas_state_t,
                                 COSTAS_STATE_MAGIC, COSTAS_STATE_VERSION, tlm,
                                 costas_state_ok)

int
dp_costas_configure (dp_costas_state_t *state, double bn, double zeta)
{
  /* The loop filter refuses outside its domain before writing anything, so
     bn and zeta are stored only once it took them: the getter never reports
     a bandwidth the loop is not running (doppler#2112). */
  /* At order 3 the active filter takes bn too, keeping its frequency and
     rate; both domains are checked before either filter is written. */
  if (state->order == 3
      && (!dp_state_filter_params_ok (bn, 1.0)
          || !dp_loop_filter_params_ok (bn, zeta, 1.0)))
    return DP_ERR_INVALID;
  if (dp_loop_filter_configure (&state->lf, bn, zeta, 1.0) != DP_OK)
    return DP_ERR_INVALID;
  if (state->order == 3)
    (void)dp_state_filter_configure (&state->sf, bn, 1.0);
  state->bn   = bn;
  state->zeta = zeta;
  return DP_OK;
}

/* Output bound: emitted symbols <= x_len; the binding sizes the buffer to the
 * input length, so 0 (== "caller sizes") is the correct sentinel. */
size_t
dp_costas_steps_max_out (dp_costas_state_t *state)
{
  (void)state;
  return 0; /* one symbol per tsamps >= 1 inputs, so symbols <= inputs */
}

size_t
dp_costas_steps (dp_costas_state_t *state, const float _Complex *x,
                 size_t x_len, float _Complex *out, size_t max_out)
{
  size_t emitted = 0;
  /* The telemetry check is hoisted to loop entry (attach is setup-time
   * only — SPSC contract), so the detached loop contains NO call site:
   * an extern call inside the loop forces the compiler to assume every
   * state field is clobbered per iteration, spilling the register-cached
   * NCO/accumulator hot state (measured ~20% slower detached on the
   * symsync loops even though the call never executed). */
  if (!state->tlm.ctx)
    {
      for (size_t n = 0; n < x_len; n++)
        {
          state->acc += costas_wipeoff (state, x[n]);
          if (++state->acc_n < state->tsamps)
            continue;
          /* symbol boundary: dump, steer the loop, emit the prompt */
          float _Complex prompt = state->acc;
          costas_update (state, prompt);
          if (emitted < max_out)
            out[emitted++] = prompt / (float)state->tsamps;
          state->acc   = 0.0f;
          state->acc_n = 0;
        }
    }
  else
    {
      for (size_t n = 0; n < x_len; n++)
        {
          state->acc += costas_wipeoff (state, x[n]);
          if (++state->acc_n < state->tsamps)
            continue;
          float _Complex prompt = state->acc;
          costas_update (state, prompt);
          if (emitted < max_out)
            out[emitted++] = prompt / (float)state->tsamps;
          state->acc   = 0.0f;
          state->acc_n = 0;
          dp_costas_tlm_flush (state);
        }
    }
  return emitted;
}

double
dp_costas_get_bn (const dp_costas_state_t *state)
{
  return state->bn;
}

int
dp_costas_set_bn (dp_costas_state_t *state, double val)
{
  return dp_costas_configure (state, val, state->zeta);
}

double
dp_costas_get_norm_freq (const dp_costas_state_t *state)
{
  return state->nco.norm_freq;
}

double
dp_costas_get_nco_freq (const dp_costas_state_t *state)
{
  /* Effective NCO frequency command = the loop-filter OUTPUT: the NCO
   * frequency register (which holds the integrator, set each symbol) PLUS
   * the proportional term applied to phase as a per-symbol nudge
   * (kp*e radians every tsamps samples == kp*e/(2*pi*tsamps) cycles/sample).
   * Its mean rides a frequency ramp with no lag; the integrator-only
   * get_norm_freq lags by the constant Type-II ramp error. */
  const double kp = state->order == 3 ? state->sf.kp : state->lf.kp;
  return state->nco.norm_freq
         + (kp * state->last_error) / (2.0 * M_PI * (double)state->tsamps);
}

void
dp_costas_set_norm_freq (dp_costas_state_t *state, double val)
{
  state->seed_norm_freq = val;
  dp_loop_filter_reset (&state->lf);
  if (state->order == 3)
    dp_state_filter_reset (&state->sf);
  seed (state, val);
}

double
dp_costas_get_lock_metric (const dp_costas_state_t *state)
{
  return state->lock_metric;
}

void
dp_costas_configure_lock (dp_costas_state_t *state, double up_thresh,
                          double down_thresh, uint32_t n_up, uint32_t n_down)
{
  dp_lockdet_configure (&state->lock, up_thresh, down_thresh, n_up, n_down);
}

int
dp_costas_get_locked (const dp_costas_state_t *state)
{
  return state->lock.locked;
}

dp_lockdet_rule_t
dp_costas_get_lock_rule (const dp_costas_state_t *state)
{
  return dp_lockdet_rule (&state->lock);
}

double
dp_costas_get_nco_phase_ahead (const dp_costas_state_t *state, size_t ahead)
{
  /* Unsigned wrap is the NCO's own modulus: (ahead mod 2^32) * inc mod
     2^32 is exactly ahead increments. */
  const uint32_t ph
      = state->nco.phase + (uint32_t)ahead * state->nco.phase_inc;
  return nco_word_to_norm (ph);
}

double
dp_costas_get_last_error (const dp_costas_state_t *state)
{
  return state->last_error;
}

double
dp_costas_get_bn_fll (const dp_costas_state_t *state)
{
  return state->bn_fll;
}

int
dp_costas_set_bn_fll (dp_costas_state_t *state, double val)
{
  /* Refused before anything is written, so the getter keeps reporting the
     assist the loop is running (doppler#2646). */
  if (!dp_loop_filter_fll_bn_ok (val))
    return DP_ERR_INVALID;
  /* Not measured with a rate integrator behind it (costas.md 3.2). */
  if (state->order == 3 && val > 0.0)
    return DP_ERR_INVALID;
  state->bn_fll = val;
  state->k_fll  = 4.0 * val;
  return DP_OK;
}

int
dp_costas_set_order (dp_costas_state_t *state, int order, double bn)
{
  if (order != 2 && order != 3)
    return DP_ERR_INVALID;
  if (!dp_loop_filter_params_ok (bn, state->zeta, 1.0))
    return DP_ERR_INVALID;
  if (order == 3
      && (!dp_state_filter_params_ok (bn, 1.0) || state->bn_fll > 0.0))
    return DP_ERR_INVALID;
  /* Domains checked: nothing below can refuse. The frequency estimate is
     the active filter's, carried into the one selected. */
  const double integ = state->order == 3 ? state->sf.integ : state->lf.integ;
  (void)dp_loop_filter_configure (&state->lf, bn, state->zeta, 1.0);
  if (order == 3)
    {
      if (state->order == 3)
        (void)dp_state_filter_configure (&state->sf, bn, 1.0);
      else
        {
          dp_state_filter_init (&state->sf, bn, 1.0);
          state->sf.integ = integ;
          state->sf.rate  = 0.0;
        }
    }
  else
    state->lf.integ = integ;
  state->order = order;
  state->bn    = bn;
  return DP_OK;
}

int
dp_costas_get_order (const dp_costas_state_t *state)
{
  return state->order;
}

int
dp_costas_set_norm_rate (dp_costas_state_t *state, double val)
{
  if (state->order != 3)
    return DP_ERR_INVALID;
  const double ts = (double)state->tsamps;
  state->sf.rate  = 2.0 * M_PI * val * ts * ts;
  return DP_OK;
}

double
dp_costas_get_norm_rate (const dp_costas_state_t *state)
{
  if (state->order != 3)
    return 0.0;
  const double ts = (double)state->tsamps;
  return state->sf.rate / (2.0 * M_PI * ts * ts);
}
