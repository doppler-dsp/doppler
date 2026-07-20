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
  s->code_nco.phase = nco_norm_to_inc (s->seed_chip / (double)s->sf);
  s->code_nco.phase_inc
      = nco_norm_to_inc (1.0 / ((double)s->sf * (double)s->sps));
  s->code_nco.norm_freq = 1.0 / ((double)s->sf * (double)s->sps);
  s->code_nco.nmax      = 0;
  s->chip_pos           = s->seed_chip;
  s->code_rate          = 1.0;
  s->acc_e              = 0.0f;
  s->acc_p              = 0.0f;
  s->acc_l              = 0.0f;
  s->acc_o              = 0.0f;
  s->last_error         = 0.0;
  s->seg_idx            = 0;
  s->have_prev_epoch    = 0;
  s->rng = 0x2545F491u ^ (uint32_t)s->sf; /* deterministic seed */
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

/* Composition faces of the lock detector (dll_core.h): thin extern
 * wrappers over the statics above so a composing channel (the DSSS
 * despreader) runs the same always-on detector. The block kernel below
 * does NOT call these: an extern call site inside its sample loop — even
 * one taken only at epoch rate — forces the compiler to assume the
 * register-cached correlator/code-phase state is clobbered and spill it
 * per iteration (measured ~5% on steps(); same mechanism as the ~20%
 * telemetry-flush lesson). The kernel uses the statics directly. */
void
dll_lock_look (dll_state_t *s, double norm)
{
  lock_look (s, s->acc_p / (float)norm, s->acc_o / (float)norm);
  s->acc_o = 0.0f;
}

void
dll_lock_epoch (dll_state_t *s)
{
  draw_offset (s);
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
  s->sf      = code_len ? code_len : 1;
  s->sps     = sps ? sps : 1;
  s->inv_sps = 1.0 / (double)s->sps;
  /* sf/sps are create-time invariants (no setter changes them after
     this), so their reciprocals are computed exactly once, here --
     never re-derived (let alone divided) in the tracking loop's
     per-epoch execution code (dll_update(), dll_steps_impl()'s
     segments>1 branch). */
  double tsamps    = (double)s->sf * (double)s->sps;
  s->inv_tsamps    = 1.0 / tsamps;
  s->inv_tsamps2   = s->inv_tsamps * s->inv_tsamps;
  s->inv_tsamps_sf = s->inv_tsamps / (double)s->sf;
  s->spacing       = spacing;
  s->seed_chip     = init_chip;
  s->bn            = bn;
  s->zeta          = zeta;
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
  /* dll_init always runs with segments == 1 (configure_geometry's
   * set_segments default; there is no by-value counterpart to dll_create()'s
   * segments parameter), so the segments>1 chunk/lookback buffers are never
   * allocated for this instance — explicitly NULL them (not calloc'd, unlike
   * dll_create) so a stack-embedded caller struct doesn't carry garbage
   * pointers. */
  s->chunk_p = s->chunk_e = s->chunk_l = s->sums = NULL;
  s->last_backward_p = s->last_e = s->last_l = NULL;
  /* In-place (stack-embedded) init: start detached — dll_create's calloc
   * gets this for free, a caller-owned struct would otherwise carry a
   * garbage telemetry pointer into the emit gates. */
  memset (&s->tlm, 0, sizeof s->tlm);
  seed (s);
}

/* Allocate the segments>1 chunk/lookback buffers (seven arrays, length
 * segments); on any failure, free whatever succeeded and return 0. Only
 * ever called from dll_create() -- dll_init()'s embedded/borrowed path is
 * always segments==1 (see dll_init()'s own comment), so this never needs a
 * matching deinit for that lifecycle. `sums` is pure epoch-local scratch
 * (rebuilt from chunk_p every epoch boundary; see dll_steps_impl's
 * segments>1 branch) but is persisted here rather than allocated per-epoch,
 * matching this codebase's "no allocation in the hot loop" rule. */
static int
alloc_segment_buffers (dll_state_t *s, size_t segments)
{
  s->chunk_p         = calloc (segments, sizeof (*s->chunk_p));
  s->chunk_e         = calloc (segments, sizeof (*s->chunk_e));
  s->chunk_l         = calloc (segments, sizeof (*s->chunk_l));
  s->sums            = calloc (segments, sizeof (*s->sums));
  s->last_backward_p = calloc (segments, sizeof (*s->last_backward_p));
  s->last_e          = calloc (segments, sizeof (*s->last_e));
  s->last_l          = calloc (segments, sizeof (*s->last_l));
  return s->chunk_p && s->chunk_e && s->chunk_l && s->sums
         && s->last_backward_p && s->last_e && s->last_l;
}

static void
free_segment_buffers (dll_state_t *s)
{
  free (s->chunk_p);
  free (s->chunk_e);
  free (s->chunk_l);
  free (s->sums);
  free (s->last_backward_p);
  free (s->last_e);
  free (s->last_l);
  s->chunk_p = s->chunk_e = s->chunk_l = s->sums = NULL;
  s->last_backward_p = s->last_e = s->last_l = NULL;
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
  if (obj->segments > 1 && !alloc_segment_buffers (obj, obj->segments))
    {
      free_segment_buffers (obj);
      free (copy);
      free (obj);
      return NULL;
    }
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
  free_segment_buffers (state);
  free (state);
}

void
dll_reset (dll_state_t *state)
{
  loop_filter_reset (&state->lf);
  seed (state);
}

size_t
dll_lookback_segments (size_t tsamps, double max_error_db)
{
  if (tsamps == 0)
    return 1;
  double phase_resolution = 1.0 - pow (10.0, -max_error_db / 10.0);
  double ideal            = ceil ((double)tsamps * phase_resolution);

  size_t best_size = 1;
  double best_dist = HUGE_VAL;
  for (size_t i = 1; i <= tsamps; i++)
    {
      if (tsamps % i != 0)
        continue;
      double dist = fabs ((double)i - ideal);
      if (dist < best_dist)
        {
          best_dist = dist;
          best_size = i;
        }
    }
  return tsamps / best_size;
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

/* Serializable state — whole-struct snapshot (loop_filter child and the
 * embedded code_nco are both POD, so their bytes are their state) plus,
 * when segments > 1, the six heap-owned chunk/lookback buffers packed
 * field-wise (same pattern as despreader_state_t's `flip_hist`) since
 * they're pointers, not part of the struct's own bytes. The borrowed
 * `code` pointer + its ownership are this instance's (config), restored
 * by create() and preserved here. */
size_t
dll_state_bytes (const dll_state_t *s)
{
  size_t extra = s->segments > 1 ? 6 * s->segments * sizeof (*s->chunk_p) : 0;
  return sizeof (dp_state_hdr_t) + sizeof (dll_state_t) + extra;
}

void
dll_get_state (const dll_state_t *s, void *blob)
{
  DP_GET_OPEN (DLL_STATE_MAGIC, DLL_STATE_VERSION, dll_state_bytes (s));
  /* Snapshot the struct but NULL the borrowed `code` pointer + its ownership,
   * and the seven segments>1 buffer pointers (six packed separately below;
   * `sums` is pure epoch-local scratch, never meaningful across calls, so
   * it is NULLed here and simply not packed at all -- serializing a raw
   * address would make the blob differ across otherwise-identical
   * instances): those are this instance's config (restored by create), and
   * the telemetry attachment is zeroed for the same reason (blobs stay
   * deterministic and attachment-independent; telemetry is observation,
   * not DSP state). set_state preserves the live values regardless. */
  dll_state_t tmp = *s;
  tmp.code        = NULL;
  tmp.owns_code   = 0;
  tmp.chunk_p = tmp.chunk_e = tmp.chunk_l = tmp.sums = NULL;
  tmp.last_backward_p = tmp.last_e = tmp.last_l = NULL;
  memset (&tmp.tlm, 0, sizeof tmp.tlm);
  dp_w_bytes (&_w, &tmp, sizeof tmp);
  if (s->segments > 1)
    {
      size_t n = s->segments;
      dp_w_bytes (&_w, s->chunk_p, n * sizeof (*s->chunk_p));
      dp_w_bytes (&_w, s->chunk_e, n * sizeof (*s->chunk_e));
      dp_w_bytes (&_w, s->chunk_l, n * sizeof (*s->chunk_l));
      dp_w_bytes (&_w, s->last_backward_p, n * sizeof (*s->last_backward_p));
      dp_w_bytes (&_w, s->last_e, n * sizeof (*s->last_e));
      dp_w_bytes (&_w, s->last_l, n * sizeof (*s->last_l));
    }
}

int
dll_set_state (dll_state_t *s, const void *blob)
{
  DP_SET_OPEN (DLL_STATE_MAGIC, DLL_STATE_VERSION, dll_state_bytes (s));
  const uint8_t *code
      = s->code; /* this instance's code + ownership (config) */
  int       owns = s->owns_code;
  dll_tlm_t tlm  = s->tlm; /* live attachment survives a state hand-off */
  /* This instance's own buffers (sized by ITS segments, fixed at create) —
   * never trust a size from the blob for allocation. `sums` is preserved
   * the same way though never packed/restored from the blob body (pure
   * epoch-local scratch, rebuilt from chunk_p at the next epoch boundary
   * regardless of whatever was in it before this call). */
  float complex *chunk_p = s->chunk_p, *chunk_e = s->chunk_e,
                *chunk_l = s->chunk_l, *sums = s->sums;
  float complex *last_backward_p = s->last_backward_p, *last_e = s->last_e,
                *last_l = s->last_l;
  dp_r_bytes (&_r, s, sizeof *s);
  s->code            = code;
  s->owns_code       = owns;
  s->tlm             = tlm;
  s->chunk_p         = chunk_p;
  s->chunk_e         = chunk_e;
  s->chunk_l         = chunk_l;
  s->sums            = sums;
  s->last_backward_p = last_backward_p;
  s->last_e          = last_e;
  s->last_l          = last_l;
  if (s->segments > 1)
    {
      size_t n = s->segments;
      dp_r_bytes (&_r, s->chunk_p, n * sizeof (*s->chunk_p));
      dp_r_bytes (&_r, s->chunk_e, n * sizeof (*s->chunk_e));
      dp_r_bytes (&_r, s->chunk_l, n * sizeof (*s->chunk_l));
      dp_r_bytes (&_r, s->last_backward_p, n * sizeof (*s->last_backward_p));
      dp_r_bytes (&_r, s->last_e, n * sizeof (*s->last_e));
      dp_r_bytes (&_r, s->last_l, n * sizeof (*s->last_l));
    }
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
          /* Off-peak (noise) tap on the same pre-advance phase as the
             prompt; accumulates over the same epoch. */
          dll_lock_accumulate (state, x[n]);
          int wrapped = dll_accumulate (state, x[n]);
          if (!wrapped)
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
  /* segments > 1: chunked output + a one-epoch-deep lookback for a clean
     power reference -- a direct C port of `prototypes/async_despreader/
     despreader_coupled.py`'s `find_max_power()`/`get_window()` (also
     `docs/design/async-despreader-working-design.md`'s own reference
     pseudocode), traceable against that Python source step for step (see
     the epoch-boundary block below). The OUTPUT is always this epoch's
     own natural, unshifted per-chunk prompt sums (an oversampled stream
     at `segments` samples/epoch, Python's `integrate_and_dump`) -- this
     is the actual despread symbol stream a composing receiver hands to
     its demodulator, NOT internal scratch, so it is never sign-adjusted
     here (see the emit-block comment below). The lookback only supplies
     a clean power
     reference (for the discriminator's denominator and the output
     normalization, Python's `max_abs`) by comparing the natural window
     against candidates built from the previous epoch's tail -- never an
     open-ended buffer, exactly one epoch deep. A plain argmax over every
     candidate, same as the reference -- no margin/hysteresis (a
     DLL_LOOKBACK_MARGIN threshold lived here previously; it was an
     undocumented deviation from the validated design, present in neither
     despreader_coupled.py/
     despreader_interp.py's own find_max_power nor the design doc's own
     pseudocode, and empirically made no difference to this scenario's own
     behavior -- removed). */
  size_t w      = state->segments;
  double tsamps = (double)(state->sf * state->sps);
  for (size_t n = 0; n < x_len; n++)
    {
      dll_lock_accumulate (state, x[n]);
      int wrapped = dll_accumulate (state, x[n]);
      for (;;)
        {
          if (state->seg_idx >= w)
            break;
          int last_chunk = (state->seg_idx + 1 == w);
          int ready = last_chunk
                          ? wrapped
                          : (state->chip_pos >= (double)(state->seg_idx + 1)
                                                    * state->seg_chips);
          if (!ready)
            break;
          /* Per-chunk lock-detector look: unrelated to the lookback (the
             noise tap's own statistics aren't biased by a data transition),
             so this stays exactly as it was pre-redesign -- immediate,
             seg_norm-normalized. */
          float complex part  = state->acc_p / (float)state->seg_norm;
          float complex noise = state->acc_o / (float)state->seg_norm;
          lock_look (state, part, noise);
          state->chunk_p[state->seg_idx] = state->acc_p;
          state->chunk_e[state->seg_idx] = state->acc_e;
          state->chunk_l[state->seg_idx] = state->acc_l;
          state->acc_e                   = 0.0f;
          state->acc_p                   = 0.0f;
          state->acc_l                   = 0.0f;
          state->acc_o                   = 0.0f;
          state->seg_idx++;
          if (state->seg_idx == w)
            {
              /* Epoch boundary -- find_max_power(), step by step:

                 Step 1 (Python: `partial_sums = x.reshape(windows,
                 step_size).sum(axis=1)`): chunk_p[] IS partial_sums,
                 already built one chunk per loop iteration above.

                 Step 2 (Python: `sums = partial_sums.cumsum()`): a plain
                 forward running sum of this epoch's own chunks. */
              state->sums[0] = state->chunk_p[0];
              for (size_t k = 1; k < w; k++)
                state->sums[k] = state->sums[k - 1] + state->chunk_p[k];

              /* Step 3 (Python: `correlations`): one candidate per
                 lookback shift. Candidate k=w-1 is the natural (unshifted)
                 window -- the WHOLE epoch, `sums[w-1]` (Python's own
                 `correlations[-1] = abs(sums[-1])`) -- taken as the
                 default below. Every other candidate k=0..w-2 borrows the
                 previous epoch's LAST (w-1-k) chunks (from
                 last_backward_p, saved at the end of the PREVIOUS
                 epoch's own Step 5 below -- last_backward_p[j] holds the
                 sum of that epoch's last (j+1) chunks) in place of this
                 epoch's own last (w-1-k) chunks: `sums[k] +
                 last_backward_p[w-2-k]`, exactly Python's `sums[:-1] +
                 last_backward_sums[::-1][1:]` re-indexed for a forward
                 loop instead of a reversed numpy slice. No previous
                 epoch yet: skip the loop entirely and keep the natural
                 window (Python's own `else: correlations[:-1] =
                 abs(sums[:-1])` branch is subsumed -- every un-compared
                 candidate is simply never preferred over the initial
                 default).

                 Step 4 (Python: `max_window = correlations.argmax()`):
                 plain argmax over ALL w candidates below -- no margin or
                 hysteresis, matching the reference exactly. */
              double best_abs  = cabsf (state->sums[w - 1]) / tsamps;
              size_t best_widx = 0; /* 0 = natural (unshifted) window */
              if (state->have_prev_epoch)
                for (size_t k = 0; k + 1 < w; k++)
                  {
                    float complex combined
                        = state->sums[k] + state->last_backward_p[w - 2 - k];
                    double p = cabsf (combined) / tsamps;
                    if (p > best_abs)
                      {
                        best_abs = p;
                        /* chunks borrowed from the previous epoch's tail;
                           Python's own window_index/step_size. */
                        best_widx = w - 1 - k;
                      }
                  }
              /* Step 5 (Python: `get_window(early, last_early,
                 window_index)`/`get_window(late, ...)`, then
                 `early_power`/`late_power`): reconstruct E/L over the
                 winning window -- last_e/last_l's tail (best_widx chunks,
                 the borrowed previous-epoch tail) + this epoch's own
                 chunk_e/chunk_l head (w-best_widx chunks) -- trivial when
                 best_widx == 0 (the natural window). */
              float complex acc_e_tot = 0.0f, acc_l_tot = 0.0f;
              if (best_widx > 0)
                for (size_t k = w - best_widx; k < w; k++)
                  {
                    acc_e_tot += state->last_e[k];
                    acc_l_tot += state->last_l[k];
                  }
              for (size_t k = 0; k < w - best_widx; k++)
                {
                  acc_e_tot += state->chunk_e[k];
                  acc_l_tot += state->chunk_l[k];
                }
              float  me = cabsf (acc_e_tot), ml = cabsf (acc_l_tot);
              double ep = (double)me * me, lp = (double)ml * ml;
              /* pp must be on the SAME raw (un-normalised) scale as ep/lp,
                 which come straight from acc_e_tot/acc_l_tot -- best_abs
                 was divided by tsamps for the search comparison and the
                 output denom below, so undo that here rather than mixing
                 a tsamps-scaled pp against a raw ep/lp (that mismatch, off
                 by roughly tsamps^2, pinned the discriminator at
                 DLL_DISC_CLAMP on essentially every epoch). */
              double best_mag = best_abs * tsamps;
              double pp       = best_mag * best_mag;
              double e        = 0.5 * (ep - lp) / (pp + DLL_EPS);
              if (e > DLL_DISC_CLAMP)
                e = DLL_DISC_CLAMP;
              else if (e < -DLL_DISC_CLAMP)
                e = -DLL_DISC_CLAMP;
              state->last_error = e;
              /* Divide the loop filter's FULL proportional+integral
                 output by tsamps^2 to get a PURE per-sample phase_inc
                 deviation -- the validated form (see the Python
                 prototype this was ported from, despreader.py's module
                 docstring point 2), not "integrator alone as the
                 sustained rate, plus the proportional term spread over
                 an extra factor of sf" (a scheme that diverges under
                 long-run stress: last_error creeps and saturates
                 DLL_DISC_CLAMP). The NCO free-runs at its own nominal
                 rate (1/tsamps, set once in seed()); ctrl never
                 involves that "1.0" nominal at all -- only the final
                 phase_inc combination does, as a separate additive
                 term, so a future second correction source (e.g. a
                 carrier-aiding term) can sum in without redefining
                 what "nominal" means. code_rate stays a public ratio
                 observable (1.0 = nominal) -- never fed back in. Nor
                 does this divide: inv_tsamps/inv_tsamps2 are
                 precomputed once at construction
                 (configure_geometry()), never here. */
              double lf_out    = loop_filter_step (&state->lf, e);
              double ctrl      = lf_out * state->inv_tsamps2;
              state->code_rate = 1.0 + lf_out * state->inv_tsamps;
              /* rate_aid (0 = off): the carrier-aiding rate bias, scaled by
                 the nominal per-sample rate so it rides the sample-and-hold
                 phase_inc continuously across the epoch (see dll_update()). */
              state->code_nco.phase_inc = nco_norm_to_inc (
                  state->inv_tsamps * (1.0 + state->rate_aid) + ctrl);

              /* Output: this epoch's own natural chunk sums, normalized by
                 the clean power reference found above -- never the
                 lookback-shifted reconstruction, and never sign-adjusted
                 either. `out[]` is not internal scratch: it is the
                 actual despread symbol stream a composing receiver hands
                 straight to its RateConverter/demodulator. Forcing one
                 sign onto a whole epoch would be wrong whenever the
                 natural window genuinely straddles a data-bit transition
                 (the very case a nonzero `best_widx` detects) -- the two
                 halves of that epoch then carry two REAL, different data
                 bits, and one epoch-wide sign would silently corrupt one
                 of them. A carrier-tracking consumer that wants a
                 data-wiped reference derives its own (see
                 dsss_receiver_core.c's `_carrier_update_from_partials`)
                 rather than this primitive baking one into its output. */
              double denom = best_abs > 0.0 ? state->seg_norm * best_abs
                                            : state->seg_norm;
              for (size_t k = 0; k < w; k++)
                if (emitted < max_out)
                  out[emitted++] = state->chunk_p[k] / (float)denom;

              /* Step 6 (Python: `backward_sums = partial_sums[::-1]
                 .cumsum()`, returned for the NEXT call's own
                 `last_backward_sums` argument): this epoch's reversed
                 running sum + raw chunk sums become the lookback
                 reference for the NEXT epoch (computed only after all
                 reads of the OLD last_backward_p/last_e/last_l above). */
              float complex bsum = 0.0f;
              for (size_t j = 0; j < w; j++)
                {
                  bsum += state->chunk_p[w - 1 - j];
                  state->last_backward_p[j] = bsum;
                }
              memcpy (state->last_e, state->chunk_e,
                      w * sizeof (*state->last_e));
              memcpy (state->last_l, state->chunk_l,
                      w * sizeof (*state->last_l));
              state->have_prev_epoch = 1;

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

void
dll_set_rate_aid (dll_state_t *state, double rate_aid)
{
  /* Just stores the bias; the next dll_update()/dll_steps() period boundary
     folds it into phase_inc (inv_tsamps*(1+rate_aid) + ctrl). Deliberately
     does NOT touch phase_inc here, so this is safe to call every period for
     continuous aiding without clobbering the loop's own steering -- a fresh
     DLL simply drifts for at most one (sub-chip) period before the first
     update applies the aid. code_rate (the loop's own ratio observable) is
     left untouched. */
  state->rate_aid = rate_aid;
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
