#include "dll/dll_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Default always-on lock config, applied at create/init so the detector works
 * out of the box: pfa = 1e-3 over N = 20 non-coherent looks, EMA bandwidth
 * auto-derived (see dll_configure_lock).  Computed through the same
 * detection-module path a caller-supplied config takes, so the C and Python
 * defaults are identical by construction (this used to be a baked-constant
 * approximation the Python binding silently overrode). */
#define DLL_LOCK_DEFAULT_PFA 1e-3
#define DLL_LOCK_DEFAULT_N 20
/* Drop-side verify count. Sizing the drop run probabilistically needs the
 * per-decision detection probability, which needs an SNR the DLL doesn't
 * know — so the default is a modest fixed time hysteresis: two straight
 * sub-threshold decisions (false-drop rate (1-pd_dec)^2 per window). The
 * C-only dll_configure_lock_raw() exposes it for callers that do know. */
#define DLL_LOCK_DEFAULT_N_DOWN 2u

/* xorshift32 — a tiny, deterministic PRNG for the lock-detector noise tap's
 * random offset.  Reproducible from a fixed seed so tests/benches are stable.
 */
static uint32_t
xorshift32 (uint32_t *s)
{
  uint32_t x = *s;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return (*s = x);
}

/* Draw the next epoch's offset (noise) tap code phase: a random whole-chip
 * offset in [guard, sf - guard) so it clears the prompt/early/late main lobe
 * (the offset correlation is then signal-free for a low-sidelobe code). */
static void
draw_offset (dll_state_t *s)
{
  size_t guard = (size_t)(s->noise_guard + 0.999); /* ceil */
  if (2 * guard >= s->sf)
    {
      s->off_chips = 0.0; /* code too short to offset; degenerate */
      return;
    }
  size_t span  = s->sf - 2 * guard;
  s->off_chips = (double)(guard + xorshift32 (&s->rng) % span);
}

/* Re-seed the loop to its create-time code phase + nominal rate, and clear the
 * correlator accumulators. The loop filter integrator is reset by the caller
 * (dll_init / dll_reset) before this runs. */
static void
seed (dll_state_t *s)
{
  s->chip_pos   = s->seed_chip;
  s->code_rate  = 1.0;
  s->acc_e      = 0.0f;
  s->acc_p      = 0.0f;
  s->acc_l      = 0.0f;
  s->acc_o      = 0.0f;
  s->last_error = 0.0;
  s->seg_idx    = 0;
  s->sum_e      = 0.0;
  s->sum_l      = 0.0;
  s->rng        = 0x2545F491u ^ (uint32_t)s->sf; /* deterministic seed */
  draw_offset (s);
  /* Clear the lock detector's running state; keep its config (threshold/
     n_looks/alpha) so reset() re-seeds the loop without re-tuning the
     detector. */
  s->noise_ema  = 0.0;
  s->lock_sum   = 0.0;
  s->lock_count = 0;
  s->lock_nz    = 0;
  s->lock_stat  = 0.0;
  lockdet_reset (&s->lock);
}

/* One non-coherent look: fold the offset (noise) sample into the noise
 * reference and the prompt power into the running N-look sum; at the N-th look
 * form R = sqrt(2 * sum|P|^2 / E|O|^2) and latch the lock decision.
 *
 * The reference uses a cumulative-mean bootstrap: while fewer than 1/alpha
 * looks have been seen it is the running average (effective weight 1/k), then
 * it relaxes to the fixed-alpha EMA.  This converges as fast as possible and
 * is unbiased from the first look — without it the EMA stays seed-dominated
 * for ~1/alpha looks (~hundreds of epochs) and the noise floor (hence Pfa) is
 * wrong during that warm-up. */
static void
lock_look (dll_state_t *s, float complex prompt, float complex offset)
{
  double po = (double)crealf (offset) * (double)crealf (offset)
              + (double)cimagf (offset) * (double)cimagf (offset);
  s->lock_nz++;
  double a = 1.0 / (double)s->lock_nz; /* cumulative mean while k < 1/alpha */
  if (a < s->lock_alpha)
    a = s->lock_alpha; /* then the steady-state EMA */
  s->noise_ema += a * (po - s->noise_ema);
  double pp = (double)crealf (prompt) * (double)crealf (prompt)
              + (double)cimagf (prompt) * (double)cimagf (prompt);
  s->lock_sum += pp;
  if (++s->lock_count >= s->n_looks)
    {
      double denom = s->noise_ema > DLL_EPS ? s->noise_ema : DLL_EPS;
      s->lock_stat = sqrt (2.0 * s->lock_sum / denom);
      (void)lockdet_step (&s->lock, s->lock_stat);
      s->lock_sum   = 0.0;
      s->lock_count = 0;
    }
}

/* Set the partial-correlation count and its derived geometry (>= 1). */
static void
set_segments (dll_state_t *s, size_t segments)
{
  s->segments  = segments ? segments : 1;
  s->seg_chips = (double)s->sf / (double)s->segments;
  s->seg_norm  = (double)(s->sf * s->sps) / (double)s->segments;
}

static void
configure_geometry (dll_state_t *s, size_t code_len, size_t sps,
                    double init_chip, double bn, double zeta, double spacing)
{
  s->sf        = code_len ? code_len : 1;
  s->sps       = sps ? sps : 1;
  s->inv_sps   = 1.0 / (double)s->sps;
  s->spacing   = spacing;
  s->seed_chip = init_chip;
  s->bn        = bn;
  s->zeta      = zeta;
  /* The offset (noise) tap must clear the prompt/early/late lobe: early and
     late sit `spacing` chips out, so guard a couple chips beyond that. */
  s->noise_guard = spacing + 2.0;
  loop_filter_init (&s->lf, bn, zeta, 1.0); /* updates once per period */
  set_segments (s,
                1); /* default: coherent full-epoch (dll_create overrides) */
  (void)dll_configure_lock (s, DLL_LOCK_DEFAULT_PFA, DLL_LOCK_DEFAULT_N,
                            0.0 /* auto EMA bandwidth */);
}

void
dll_init (dll_state_t *s, const uint8_t *code, size_t code_len, size_t sps,
          double init_chip, double bn, double zeta, double spacing)
{
  configure_geometry (s, code_len, sps, init_chip, bn, zeta, spacing);
  /* In-place init of a caller-owned (possibly stack) state: loop_filter_init
     preserves the integrator (it doubles as a reconfigure), so zero it here —
     seed() sets code_rate = 1.0 and assumes integ == 0. dll_create() gets this
     free via calloc; an embedded/stack dll_state_t would otherwise start with
     a garbage code rate. */
  loop_filter_reset (&s->lf);
  s->code      = code; /* borrowed */
  s->owns_code = 0;
  /* In-place (stack-embedded) init: start detached — dll_create's calloc
   * gets this for free, a caller-owned struct would otherwise carry a
   * garbage telemetry pointer into the emit gates. */
  memset (&s->tlm, 0, sizeof s->tlm);
  seed (s);
}

dll_state_t *
dll_create (const uint8_t *code, size_t code_len, size_t sps, double init_chip,
            double bn, double zeta, double spacing, size_t segments)
{
  if (!code || code_len == 0 || segments == 0)
    return NULL;
  dll_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;
  uint8_t *copy = malloc (code_len);
  if (!copy)
    {
      free (obj);
      return NULL;
    }
  memcpy (copy, code, code_len);
  configure_geometry (obj, code_len, sps, init_chip, bn, zeta, spacing);
  set_segments (obj, segments);
  obj->code      = copy;
  obj->owns_code = 1;
  seed (obj);
  return obj;
}

void
dll_destroy (dll_state_t *state)
{
  if (!state)
    return;
  if (state->owns_code)
    free ((void *)state->code);
  free (state);
}

void
dll_reset (dll_state_t *state)
{
  loop_filter_reset (&state->lf);
  seed (state);
}

int
dll_set_telemetry (dll_state_t *state, dp_tlm_t *tlm, const char *prefix,
                   uint32_t decim)
{
  if (!tlm) /* detach: probe sites revert to the single-branch cost */
    {
      state->tlm.ctx = NULL;
      return DP_OK;
    }
  const char *p = prefix ? prefix : "code";
  char        name[DP_TLM_NAME_MAX];
  (void)snprintf (name, sizeof (name), "%s.e", p);
  int id_e = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.rate", p);
  int id_rate = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.lock", p);
  int id_lock = dp_tlm_probe (tlm, name, decim);
  (void)snprintf (name, sizeof (name), "%s.locked", p);
  int id_locked = dp_tlm_probe (tlm, name, decim);
  if (id_e < 0 || id_rate < 0 || id_lock < 0 || id_locked < 0)
    return DP_ERR_INVALID; /* table full / bad prefix: attach fails whole */
  state->tlm.id_e      = id_e;
  state->tlm.id_rate   = id_rate;
  state->tlm.id_lock   = id_lock;
  state->tlm.id_locked = id_locked;
  state->tlm.ctx       = tlm; /* set last: emit sites gate on ctx */
  return DP_OK;
}

void
dll_tlm_flush (const dll_state_t *s)
{
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_e, s->last_error);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_rate, s->code_rate);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_lock, s->lock_stat);
  dp_tlm_emit (s->tlm.ctx, s->tlm.id_locked, (double)s->lock.locked);
}

/* Serializable state — whole-struct snapshot (loop_filter child is
 * POD-embedded, so its bytes are its state); only the borrowed `code` pointer
 * + its ownership are this instance's (config), restored by create() and
 * preserved here. */
size_t
dll_state_bytes (const dll_state_t *s)
{
  (void)s;
  return sizeof (dp_state_hdr_t) + sizeof (dll_state_t);
}

void
dll_get_state (const dll_state_t *s, void *blob)
{
  DP_GET_OPEN (DLL_STATE_MAGIC, DLL_STATE_VERSION, dll_state_bytes (s));
  /* Snapshot the struct but NULL the borrowed `code` pointer + its ownership:
   * those are this instance's config (restored by create), and serializing a
   * machine address would make the blob differ across otherwise-identical
   * instances. The telemetry attachment is zeroed for the same reason (blobs
   * stay deterministic and attachment-independent; telemetry is observation,
   * not DSP state). set_state preserves the live values regardless. */
  dll_state_t tmp = *s;
  tmp.code        = NULL;
  tmp.owns_code   = 0;
  memset (&tmp.tlm, 0, sizeof tmp.tlm);
  dp_w_bytes (&_w, &tmp, sizeof tmp);
}

int
dll_set_state (dll_state_t *s, const void *blob)
{
  DP_SET_OPEN (DLL_STATE_MAGIC, DLL_STATE_VERSION, dll_state_bytes (s));
  const uint8_t *code
      = s->code; /* this instance's code + ownership (config) */
  int       owns = s->owns_code;
  dll_tlm_t tlm  = s->tlm; /* live attachment survives a state hand-off */
  dp_r_bytes (&_r, s, sizeof *s);
  s->code      = code;
  s->owns_code = owns;
  s->tlm       = tlm;
  return DP_OK;
}

void
dll_configure (dll_state_t *state, double bn, double zeta)
{
  state->bn   = bn;
  state->zeta = zeta;
  loop_filter_configure (&state->lf, bn, zeta, 1.0);
}

/* Output bound: emitted symbols <= x_len; the binding sizes the buffer to the
 * input length, so 0 (== "caller sizes") is the correct sentinel. */
size_t
dll_steps_max_out (dll_state_t *state)
{
  (void)state;
  return 0;
}

/* The block kernel with the telemetry decision as a compile-time literal:
 * a literal tlm_on lets each forced-inline instantiation constant-fold the
 * flush branch away, so the detached loops carry NO call site (an extern
 * call inside the loop — even never-taken — forces the compiler to assume
 * every state field is clobbered per iteration, spilling the register-
 * cached correlator/code-phase hot state; measured ~20% slower detached
 * on the symsync loops). Same mechanism as symsync_step_ted's literal
 * TED. */
static JM_FORCEINLINE size_t
dll_steps_impl (dll_state_t *state, const float complex *x, size_t x_len,
                float complex *out, size_t max_out, int tlm_on)
{
  size_t emitted = 0;
  /* segments == 1: coherent full-epoch integrate-and-dump (one prompt/period).
   */
  if (state->segments <= 1)
    {
      double tsamps = (double)(state->sf * state->sps);
      for (size_t n = 0; n < x_len; n++)
        {
          /* Off-peak (noise) tap at chip_pos + off_chips, evaluated on the
             same pre-advance phase as the prompt; accumulates over the same
             epoch. */
          double co = state->chip_pos + state->off_chips;
          if (co >= (double)state->sf)
            co -= (double)state->sf;
          state->acc_o
              += x[n]
                 * dll_replica (state, co, state->code_rate * state->inv_sps);
          dll_accumulate (state, x[n]);
          if (state->chip_pos < (double)state->sf)
            continue;
          float complex prompt = state->acc_p / (float)tsamps;
          float complex noise  = state->acc_o / (float)tsamps;
          dll_update (state);
          lock_look (state, prompt, noise);
          if (emitted < max_out)
            out[emitted++] = prompt;
          state->acc_e = 0.0f;
          state->acc_p = 0.0f;
          state->acc_l = 0.0f;
          state->acc_o = 0.0f;
          draw_offset (state); /* fresh noise phase for the next epoch */
          if (tlm_on)
            dll_tlm_flush (state);
        }
      return emitted;
    }
  /* segments > 1: dump a partial prompt every sf/segments chips, fold each
     partial's early/late envelopes into the non-coherent epoch sums, and steer
     the code NCO once per epoch on (sum|E|-sum|L|)/(sum|E|+sum|L|) — which a
     data flip cannot collapse (only the one straddling segment degrades). */
  for (size_t n = 0; n < x_len; n++)
    {
      /* Off-peak (noise) tap, accumulated per sample like the prompt and
         dumped per partial; gives one signal-free noise sample per emitted
         look. */
      double co = state->chip_pos + state->off_chips;
      if (co >= (double)state->sf)
        co -= (double)state->sf;
      state->acc_o
          += x[n] * dll_replica (state, co, state->code_rate * state->inv_sps);
      dll_accumulate (state, x[n]);
      while (state->seg_idx < state->segments
             && state->chip_pos
                    >= (double)(state->seg_idx + 1) * state->seg_chips)
        {
          float complex part  = state->acc_p / (float)state->seg_norm;
          float complex noise = state->acc_o / (float)state->seg_norm;
          if (emitted < max_out)
            out[emitted++] = part;
          lock_look (state, part, noise);
          state->sum_e += (double)cabsf (state->acc_e);
          state->sum_l += (double)cabsf (state->acc_l);
          state->acc_e = 0.0f;
          state->acc_p = 0.0f;
          state->acc_l = 0.0f;
          state->acc_o = 0.0f;
          state->seg_idx++;
          if (state->seg_idx == state->segments)
            {
              double me = state->sum_e, ml = state->sum_l;
              double e          = (me - ml) / (me + ml + DLL_EPS);
              state->last_error = e;
              loop_filter_step (&state->lf, e);
              state->code_rate = 1.0 + state->lf.integ;
              state->chip_pos -= (double)state->sf;
              state->chip_pos += state->lf.kp * e; /* proportional nudge */
              state->sum_e   = 0.0;
              state->sum_l   = 0.0;
              state->seg_idx = 0;
              draw_offset (state); /* fresh noise phase for the next epoch */
              if (tlm_on)
                dll_tlm_flush (state);
            }
        }
    }
  return emitted;
}

size_t
dll_steps (dll_state_t *state, const float complex *x, size_t x_len,
           float complex *out, size_t max_out)
{
  /* Telemetry hoisted to a literal at entry (attach is setup-time only —
   * SPSC contract): the detached instantiation is the pre-telemetry code
   * verbatim. */
  if (!state->tlm.ctx)
    return dll_steps_impl (state, x, x_len, out, max_out, 0);
  return dll_steps_impl (state, x, x_len, out, max_out, 1);
}

double
dll_get_bn (const dll_state_t *state)
{
  return state->bn;
}

void
dll_set_bn (dll_state_t *state, double val)
{
  dll_configure (state, val, state->zeta);
}

double
dll_get_code_phase (const dll_state_t *state)
{
  return state->chip_pos;
}

double
dll_get_code_rate (const dll_state_t *state)
{
  return state->code_rate;
}

double
dll_get_last_error (const dll_state_t *state)
{
  return state->last_error;
}

size_t
dll_get_segments (const dll_state_t *state)
{
  return state->segments;
}

int
dll_configure_lock (dll_state_t *state, double pfa, size_t n_looks,
                    double ref_snr_db)
{
  if (!(pfa > 0.0 && pfa < 1.0))
    return DP_ERR_INVALID;
  size_t n = n_looks ? n_looks : 1;
  /* Noise-reference EMA bandwidth from the estimator-SNR contract: the
   * signal-free |O|^2 samples are exponential — a DC level (the noise
   * power) in fluctuation of equal power, i.e. 0 dB estimator SNR per
   * sample — and det_ema_alpha sizes the EMA for the requested output
   * SNR. The auto derivation (ref_snr_db <= 0) holds the reference's
   * relative std to an eighth of the statistic's intrinsic H0 spread
   * (1/sqrt(N)), floored at ~33 dB: SNR_out = max(64*N, 2048) - 1,
   * which lands the classic 1/alpha = max(32*N, 1024) sizing as a
   * consequence rather than a constant. */
  double snr_out_db
      = ref_snr_db > 0.0
            ? ref_snr_db
            : 10.0 * log10 (fmax (64.0 * (double)n, 2048.0) - 1.0);
  double alpha = det_ema_alpha (0.0, snr_out_db);
  /* Declare-side time hysteresis, sized from the same pfa: the false-declare
   * budget is held three decades under the per-decision pfa, so the verify
   * count is det_verify_count(pfa, pfa*1e-3) — consecutive decisions
   * compound as pfa^n_up (2 for the default pfa = 1e-3). No level
   * hysteresis by default (up = down = eta): splitting the thresholds
   * needs the detection probability, which needs an SNR the DLL doesn't
   * know; the raw face exposes both thresholds for callers that do. */
  double   eta  = det_threshold_noncoherent (pfa, (int)n);
  uint32_t n_up = (uint32_t)det_verify_count (pfa, pfa * 1e-3);
  dll_configure_lock_raw (state, eta, eta, n, alpha, n_up,
                          DLL_LOCK_DEFAULT_N_DOWN);
  return DP_OK;
}

void
dll_configure_lock_raw (dll_state_t *state, double up_thresh,
                        double down_thresh, size_t n_looks, double alpha,
                        uint32_t n_up, uint32_t n_down)
{
  lockdet_init (&state->lock, up_thresh, down_thresh, n_up, n_down);
  state->n_looks    = n_looks ? n_looks : 1;
  state->lock_alpha = alpha;
  /* Re-tuning clears the in-flight statistic and drops the lock so the next
     decision uses only looks gathered under the new config. */
  state->lock_sum   = 0.0;
  state->lock_count = 0;
  state->lock_nz    = 0;
  state->noise_ema  = 0.0;
  state->lock_stat  = 0.0;
  lockdet_reset (&state->lock);
}

int
dll_get_locked (const dll_state_t *state)
{
  return state->lock.locked;
}

double
dll_get_lock_stat (const dll_state_t *state)
{
  return state->lock_stat;
}

double
dll_get_noise_est (const dll_state_t *state)
{
  return state->noise_ema;
}
