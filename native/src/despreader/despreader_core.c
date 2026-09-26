#include "doppler/despreader/despreader_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Reset the bit-sync state (histogram + accumulators). */
static void
bitsync_reset (dp_despreader_state_t *ch)
{
  if (ch->flip_hist)
    memset (ch->flip_hist, 0, ch->periods_per_bit * sizeof (*ch->flip_hist));
  ch->epoch_count   = 0;
  ch->bit_phase     = 0;
  ch->epochs_in_bit = 0;
  ch->bit_acc       = 0.0;
  ch->prev_sign     = 0;
  ch->have_prev     = 0;
}

void
despreader_init (dp_despreader_state_t *ch, const uint8_t *code,
                 size_t code_len, size_t sps, double init_norm_freq,
                 double init_chip, double bn_carrier, double bn_code,
                 double bn_fll, double zeta, double spacing,
                 size_t periods_per_bit)
{
  size_t tsamps = (code_len ? code_len : 1) * (sps ? sps : 1);
  /* one carrier-loop update per code period (the integrate-and-dump window) */
  costas_init (&ch->car, bn_carrier, zeta, init_norm_freq, tsamps, bn_fll);
  dll_init (&ch->code, code, code_len, sps, init_chip, bn_code, zeta, spacing);
  ch->code_copy       = NULL;
  ch->tlm_ctx         = NULL; /* start detached (stack-embed safe) */
  ch->periods_per_bit = periods_per_bit ? periods_per_bit : 1;
  ch->flip_hist       = NULL;
  if (ch->periods_per_bit > 1)
    ch->flip_hist = calloc (ch->periods_per_bit, sizeof (*ch->flip_hist));
  bitsync_reset (ch);
}

dp_despreader_state_t *
dp_despreader_create (const uint8_t *code, size_t code_len, size_t sps,
                      double init_norm_freq, double init_chip,
                      double bn_carrier, double bn_code, double bn_fll,
                      double zeta, double spacing, size_t periods_per_bit)
{
  if (!code || code_len == 0)
    return NULL;
  dp_despreader_state_t *ch = calloc (1, sizeof (*ch));
  if (!ch)
    return NULL;
  uint8_t *copy = malloc (code_len);
  if (!copy)
    {
      free (ch);
      return NULL;
    }
  memcpy (copy, code, code_len);
  despreader_init (ch, copy, code_len, sps, init_norm_freq, init_chip,
                   bn_carrier, bn_code, bn_fll, zeta, spacing,
                   periods_per_bit);
  ch->code_copy = copy; /* despreader owns the code (dll borrows it) */
  return ch;
}

void
dp_despreader_destroy (dp_despreader_state_t *state)
{
  if (!state)
    return;
  free (state->flip_hist);
  free (state->code_copy);
  free (state);
}

void
dp_despreader_reset (dp_despreader_state_t *state)
{
  dp_costas_reset (&state->car);
  dp_dll_reset (&state->code);
  bitsync_reset (state);
}

int
dp_despreader_set_telemetry (dp_despreader_state_t *state, dp_tlm_t *tlm,
                             const char *prefix, uint32_t decim)
{
  if (!tlm) /* detach both embedded loops */
    {
      state->tlm_ctx = NULL;
      (void)dp_costas_set_telemetry (&state->car, NULL, prefix, decim);
      (void)dp_dll_set_telemetry (&state->code, NULL, prefix, decim);
      return DP_OK;
    }
  const char *p = prefix ? prefix : "ch";
  char        name[DP_TLM_NAME_MAX];
  /* Pure forwarder: the carrier loop under "<prefix>.car", the code loop
   * under "<prefix>.code"; if the second registration fails the first is
   * unwound so nothing is left half-armed. */
  (void)snprintf (name, sizeof (name), "%s.car", p);
  int rc = dp_costas_set_telemetry (&state->car, tlm, name, decim);
  if (rc != DP_OK)
    return rc;
  (void)snprintf (name, sizeof (name), "%s.code", p);
  rc = dp_dll_set_telemetry (&state->code, tlm, name, decim);
  if (rc != DP_OK)
    {
      (void)dp_costas_set_telemetry (&state->car, NULL, p, decim);
      return rc;
    }
  state->tlm_ctx = tlm; /* the block loops gate on this */
  return DP_OK;
}

/* Emit both loops' telemetry for the code period just closed. Out-of-line
 * on purpose (see the hoisted split in dp_despreader_steps). */
static void
despreader_tlm_flush_ (const dp_despreader_state_t *ch)
{
  costas_tlm_flush (&ch->car);
  dll_tlm_flush (&ch->code);
}

/* Serializable state — costas + dll children as nested sub-blobs, then the
 * running bit-sync histogram + scalars; the owned code copy is config
 * (create). */
size_t
dp_despreader_state_bytes (const dp_despreader_state_t *s)
{
  return sizeof (dp_state_hdr_t) + dp_costas_state_bytes (&s->car)
         + dp_dll_state_bytes (&s->code)
         + (s->flip_hist ? s->periods_per_bit * sizeof (size_t) : 0)
         + 3 * sizeof (uint64_t) + sizeof (double) + 2 * sizeof (uint32_t);
}

void
dp_despreader_get_state (const dp_despreader_state_t *s, void *blob)
{
  DP_GET_OPEN (DESPREADER_STATE_MAGIC, DESPREADER_STATE_VERSION,
               dp_despreader_state_bytes (s));
  DP_W_CHILD (&_w, dp_costas, &s->car);
  DP_W_CHILD (&_w, dp_dll, &s->code);
  if (s->flip_hist)
    dp_w_bytes (&_w, s->flip_hist, s->periods_per_bit * sizeof (size_t));
  dp_w_u64 (&_w, s->epoch_count);
  dp_w_u64 (&_w, s->bit_phase);
  dp_w_u64 (&_w, s->epochs_in_bit);
  dp_w_f64 (&_w, s->bit_acc);
  dp_w_u32 (&_w, (uint32_t)s->prev_sign);
  dp_w_u32 (&_w, (uint32_t)s->have_prev);
}

int
dp_despreader_set_state (dp_despreader_state_t *s, const void *blob)
{
  DP_SET_OPEN (DESPREADER_STATE_MAGIC, DESPREADER_STATE_VERSION,
               dp_despreader_state_bytes (s));
  DP_R_CHILD (&_r, dp_costas, &s->car);
  DP_R_CHILD (&_r, dp_dll, &s->code);
  if (s->flip_hist)
    dp_r_bytes (&_r, s->flip_hist, s->periods_per_bit * sizeof (size_t));
  s->epoch_count   = (size_t)dp_r_u64 (&_r);
  s->bit_phase     = (size_t)dp_r_u64 (&_r);
  s->epochs_in_bit = (size_t)dp_r_u64 (&_r);
  s->bit_acc       = dp_r_f64 (&_r);
  s->prev_sign     = (int)dp_r_u32 (&_r);
  s->have_prev     = (int)dp_r_u32 (&_r);
  return DP_OK;
}

/* Process one input sample. On a code-period boundary, dump the prompt, update
 * both loops, and return 1 with the normalised prompt in *prompt. */
static int
process_sample (dp_despreader_state_t *ch, float _Complex x,
                float _Complex        *prompt)
{
  float _Complex d = costas_wipeoff (&ch->car, x); /* carrier wipe-off */
  dll_lock_accumulate (&ch->code, d); /* off-peak noise tap (lock det) */
  int wrapped = dll_accumulate (&ch->code, d); /* E/P/L correlate */
  if (!wrapped)
    return 0;
  /* code-period boundary */
  float _Complex P = ch->code.acc_p;
  dll_update (&ch->code);      /* code loop on the early/late envelopes */
  costas_update (&ch->car, P); /* carrier loop on the prompt symbol */
  /* Fold this period into the code-lock detector (full-epoch look) and
   * re-draw the noise offset — the same always-on CFAR detector dp_dll_steps
   * runs, so `code.locked` / `code.lock_stat` are live in composition. */
  dll_lock_look (&ch->code, (double)(ch->code.sf * ch->code.sps));
  ch->code.acc_e = ch->code.acc_p = ch->code.acc_l = 0.0f;
  dll_lock_epoch (&ch->code);
  *prompt = P / (float)(ch->code.sf * ch->code.sps);
  return 1;
}

size_t
dp_despreader_steps_max_out (dp_despreader_state_t *state)
{
  (void)state;
  return 0; /* one prompt per code period, so prompts <= inputs */
}

size_t
dp_despreader_steps (dp_despreader_state_t *state, const float _Complex *x,
                     size_t x_len, float _Complex *out, size_t max_out)
{
  size_t emitted = 0;
  /* The telemetry check is hoisted to loop entry (attach is setup-time
   * only — SPSC contract), so the detached loop contains NO call site:
   * an extern call inside the loop forces the compiler to assume every
   * state field is clobbered per iteration, spilling the register-cached
   * correlator/NCO hot state (measured ~20% slower detached on the
   * symsync loops even though the call never executed). */
  if (!state->tlm_ctx)
    {
      for (size_t n = 0; n < x_len; n++)
        {
          float _Complex prompt;
          if (process_sample (state, x[n], &prompt) && emitted < max_out)
            out[emitted++] = prompt;
        }
    }
  else
    {
      for (size_t n = 0; n < x_len; n++)
        {
          float _Complex prompt;
          if (process_sample (state, x[n], &prompt))
            {
              if (emitted < max_out)
                out[emitted++] = prompt;
              despreader_tlm_flush_ (state);
            }
        }
    }
  return emitted;
}

/* Feed one prompt to the bit-sync; emit a hard data bit when an aligned group
 * of periods_per_bit prompts completes. Returns 1 (and sets *bit) when a bit
 * is emitted. For periods_per_bit == 1 every prompt is a bit. */
static int
bit_sync (dp_despreader_state_t *ch, float _Complex P, uint8_t *bit)
{
  size_t N  = ch->periods_per_bit;
  double re = (double)crealf (P);
  if (N <= 1)
    {
      *bit = (re >= 0.0) ? 1u : 0u;
      ch->epoch_count++;
      return 1;
    }
  /* histogram the prompt-sign-flip positions; the bit boundary is where data
   * transitions cluster (the sign is constant within a locked data bit). */
  int s = (re >= 0.0) ? 1 : -1;
  if (ch->have_prev && s != ch->prev_sign)
    ch->flip_hist[ch->epoch_count % N]++;
  ch->prev_sign = s;
  ch->have_prev = 1;
  size_t best = 0, bv = ch->flip_hist[0];
  for (size_t i = 1; i < N; i++)
    if (ch->flip_hist[i] > bv)
      {
        bv   = ch->flip_hist[i];
        best = i;
      }
  ch->bit_phase = best;
  int emitted   = 0;
  if ((ch->epoch_count % N) == ch->bit_phase)
    {
      /* boundary: emit the just-completed bit if it spanned a full period */
      if (ch->epochs_in_bit == N)
        {
          *bit    = (ch->bit_acc >= 0.0) ? 1u : 0u;
          emitted = 1;
        }
      ch->bit_acc       = 0.0;
      ch->epochs_in_bit = 0;
    }
  ch->bit_acc += re;
  ch->epochs_in_bit++;
  ch->epoch_count++;
  return emitted;
}

size_t
dp_despreader_bits_max_out (dp_despreader_state_t *state)
{
  (void)state;
  return 0; /* one bit per periods_per_bit periods, so bits <= inputs */
}

size_t
dp_despreader_bits (dp_despreader_state_t *state, const float _Complex *x,
                    size_t x_len, uint8_t *out, size_t max_out)
{
  size_t emitted = 0;
  /* Guarded in-loop flush (not the steps() split): this loop already
   * makes a per-period call (bit_sync), so there is no pristine
   * register-resident fast path to protect. */
  for (size_t n = 0; n < x_len; n++)
    {
      float _Complex prompt;
      if (!process_sample (state, x[n], &prompt))
        continue;
      if (state->tlm_ctx)
        despreader_tlm_flush_ (state);
      uint8_t bit;
      if (bit_sync (state, prompt, &bit) && emitted < max_out)
        out[emitted++] = bit;
    }
  return emitted;
}

double
dp_despreader_get_norm_freq (const dp_despreader_state_t *state)
{
  return state->car.nco.norm_freq;
}

void
dp_despreader_set_norm_freq (dp_despreader_state_t *state, double val)
{
  dp_costas_set_norm_freq (&state->car, val);
}

double
dp_despreader_get_code_phase (const dp_despreader_state_t *state)
{
  return state->code.chip_pos;
}

double
dp_despreader_get_code_rate (const dp_despreader_state_t *state)
{
  return state->code.code_rate;
}

double
dp_despreader_get_lock_metric (const dp_despreader_state_t *state)
{
  return state->car.lock_metric;
}

int
dp_despreader_get_carrier_locked (const dp_despreader_state_t *state)
{
  return state->car.lock.locked;
}

int
dp_despreader_get_code_locked (const dp_despreader_state_t *state)
{
  return state->code.lock.locked;
}

void
dp_despreader_configure_carrier_lock (dp_despreader_state_t *state,
                                      double up_thresh, double down_thresh,
                                      uint32_t n_up, uint32_t n_down)
{
  dp_costas_configure_lock (&state->car, up_thresh, down_thresh, n_up, n_down);
}

int
dp_despreader_configure_code_lock (dp_despreader_state_t *state, double pfa,
                                   size_t n_looks, double ref_snr_db)
{
  return dp_dll_configure_lock (&state->code, pfa, n_looks, ref_snr_db);
}

size_t
dp_despreader_get_bit_phase (const dp_despreader_state_t *state)
{
  return state->bit_phase;
}

double
dp_despreader_get_bn_carrier (const dp_despreader_state_t *state)
{
  return state->car.bn;
}

void
dp_despreader_set_bn_carrier (dp_despreader_state_t *state, double val)
{
  dp_costas_set_bn (&state->car, val);
}

double
dp_despreader_get_bn_code (const dp_despreader_state_t *state)
{
  return state->code.bn;
}

void
dp_despreader_set_bn_code (dp_despreader_state_t *state, double val)
{
  dp_dll_set_bn (&state->code, val);
}
