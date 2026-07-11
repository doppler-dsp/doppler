#include "costas/costas_core.h"

#include <string.h>

/* Default carrier lock-detector rule (see costas_configure_lock's header
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
seed (costas_state_t *s, double init_norm_freq)
{
  lo_init (&s->nco, init_norm_freq);
  s->lf.integ    = init_norm_freq * 2.0 * M_PI * (double)s->tsamps;
  s->acc         = 0.0f;
  s->acc_n       = 0;
  s->prev        = 0.0f;
  s->have_prev   = 0;
  s->lock_metric = 0.0;
  s->last_error  = 0.0;
  lockdet_reset (&s->lock); /* drop the lock; keep the configured rule */
}

void
costas_init (costas_state_t *s, double bn, double zeta, double init_norm_freq,
             size_t tsamps, double bn_fll)
{
  s->tsamps         = tsamps ? tsamps : 1;
  s->bn             = bn;
  s->zeta           = zeta;
  s->bn_fll         = bn_fll;
  s->k_fll          = 4.0 * bn_fll; /* 1st-order FLL aiding gain */
  s->seed_norm_freq = init_norm_freq;
  /* In-place (stack-embedded) init: start detached — costas_create's calloc
   * gets this for free, a caller-owned struct would otherwise carry a
   * garbage telemetry pointer into the emit gates. */
  memset (&s->tlm, 0, sizeof s->tlm);
  loop_filter_init (&s->lf, bn, zeta, 1.0); /* updates once per symbol */
  lockdet_init (&s->lock, COSTAS_LOCK_DEFAULT_UP, COSTAS_LOCK_DEFAULT_DOWN,
                COSTAS_LOCK_DEFAULT_N_UP, COSTAS_LOCK_DEFAULT_N_DOWN);
  seed (s, init_norm_freq);
}

costas_state_t *
costas_create (double bn, double zeta, double init_norm_freq, size_t tsamps,
               double bn_fll)
{
  costas_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  costas_init (obj, bn, zeta, init_norm_freq, tsamps, bn_fll);
  return obj;
}

void
costas_destroy (costas_state_t *state)
{
  free (state);
}

void
costas_reset (costas_state_t *state)
{
  loop_filter_reset (&state->lf);
  seed (state, state->seed_norm_freq);
}

int
costas_set_telemetry (costas_state_t *state, dp_tlm_t *tlm, const char *prefix,
                      uint32_t decim)
{
  if (!tlm) /* detach: probe sites revert to the single-branch cost */
    {
      state->tlm.ctx = NULL;
      return DP_OK;
    }
  const char *p = prefix ? prefix : "car";
  char        name[DP_TLM_NAME_MAX];
  (void)snprintf (name, sizeof (name), "%s.lock", p);
  int id_lock = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.e", p);
  int id_e = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.freq", p);
  int id_freq = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.locked", p);
  int id_locked = dp_tlm_probe (tlm, name, decim);
  if (id_lock < 0 || id_e < 0 || id_freq < 0 || id_locked < 0)
    return DP_ERR_INVALID; /* table full / bad prefix: attach fails whole */
  state->tlm.id_lock   = id_lock;
  state->tlm.id_e      = id_e;
  state->tlm.id_freq   = id_freq;
  state->tlm.id_locked = id_locked;
  state->tlm.ctx       = tlm; /* set last: emit sites gate on ctx */
  return DP_OK;
}

void
costas_tlm_flush (const costas_state_t *s)
{
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_lock, s->lock_metric);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_e, s->last_error);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_freq, s->nco.norm_freq);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_locked, (double)s->lock.locked);
}

/* Serializable state — pointer-free POD whole-struct snapshot, with the
 * telemetry attachment zeroed in blobs and kept live across restore
 * (see DP_DEFINE_POD_STATE_TLM in dp_state.h). */
DP_DEFINE_POD_STATE_TLM (costas, costas_state_t, COSTAS_STATE_MAGIC,
                         COSTAS_STATE_VERSION, tlm)

void
costas_configure (costas_state_t *state, double bn, double zeta)
{
  state->bn   = bn;
  state->zeta = zeta;
  loop_filter_configure (&state->lf, bn, zeta, 1.0);
}

/* Output bound: emitted symbols <= x_len; the binding sizes the buffer to the
 * input length, so 0 (== "caller sizes") is the correct sentinel. */
size_t
costas_steps_max_out (costas_state_t *state)
{
  (void)state;
  return 0;
}

size_t
costas_steps (costas_state_t *state, const float complex *x, size_t x_len,
              float complex *out, size_t max_out)
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
          float complex prompt = state->acc;
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
          float complex prompt = state->acc;
          costas_update (state, prompt);
          if (emitted < max_out)
            out[emitted++] = prompt / (float)state->tsamps;
          state->acc   = 0.0f;
          state->acc_n = 0;
          costas_tlm_flush (state);
        }
    }
  return emitted;
}

double
costas_get_bn (const costas_state_t *state)
{
  return state->bn;
}

void
costas_set_bn (costas_state_t *state, double val)
{
  costas_configure (state, val, state->zeta);
}

double
costas_get_norm_freq (const costas_state_t *state)
{
  return state->nco.norm_freq;
}

void
costas_set_norm_freq (costas_state_t *state, double val)
{
  state->seed_norm_freq = val;
  loop_filter_reset (&state->lf);
  seed (state, val);
}

double
costas_get_lock_metric (const costas_state_t *state)
{
  return state->lock_metric;
}

void
costas_configure_lock (costas_state_t *state, double up_thresh,
                       double down_thresh, uint32_t n_up, uint32_t n_down)
{
  lockdet_configure (&state->lock, up_thresh, down_thresh, n_up, n_down);
}

int
costas_get_locked (const costas_state_t *state)
{
  return state->lock.locked;
}

double
costas_get_last_error (const costas_state_t *state)
{
  return state->last_error;
}

double
costas_get_bn_fll (const costas_state_t *state)
{
  return state->bn_fll;
}

void
costas_set_bn_fll (costas_state_t *state, double val)
{
  state->bn_fll = val;
  state->k_fll  = 4.0 * val;
}
