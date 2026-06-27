#include "dll/dll_core.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Default always-on lock config, applied at create/init so the detector works
 * out of the box: pfa = 1e-3 over N = 20 non-coherent looks.  DLL_LOCK_DEFAULT
 * _THRESH is det_threshold_noncoherent(1e-3, 20) baked as a constant so the
 * core links only -lm; the Python binding recomputes the threshold for a
 * caller-supplied pfa (it already links the detection module).  The EMA noise
 * reference averages 1/alpha cells, kept >> N so its variance does not inflate
 * Pfa. */
#define DLL_LOCK_DEFAULT_N 20
#define DLL_LOCK_DEFAULT_THRESH                                               \
  8.567494 /* det_threshold_noncoherent(1e-3,20) */
#define DLL_LOCK_DEFAULT_ALPHA (1.0 / 1024.0)

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
  s->locked     = 0;
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
      double denom  = s->noise_ema > DLL_EPS ? s->noise_ema : DLL_EPS;
      s->lock_stat  = sqrt (2.0 * s->lock_sum / denom);
      s->locked     = (s->lock_stat > s->lock_thresh);
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
  dll_configure_lock (s, DLL_LOCK_DEFAULT_THRESH, DLL_LOCK_DEFAULT_N,
                      DLL_LOCK_DEFAULT_ALPHA);
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

size_t
dll_steps (dll_state_t *state, const float complex *x, size_t x_len,
           float complex *out, size_t max_out)
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
            }
        }
    }
  return emitted;
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

void
dll_configure_lock (dll_state_t *state, double threshold, size_t n_looks,
                    double alpha)
{
  state->lock_thresh = threshold;
  state->n_looks     = n_looks ? n_looks : 1;
  state->lock_alpha  = alpha;
  /* Re-tuning clears the in-flight statistic so the next decision uses only
     looks gathered under the new config. */
  state->lock_sum   = 0.0;
  state->lock_count = 0;
  state->lock_nz    = 0;
  state->noise_ema  = 0.0;
  state->lock_stat  = 0.0;
  state->locked     = 0;
}

int
dll_get_locked (const dll_state_t *state)
{
  return state->locked;
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
