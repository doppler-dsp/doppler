#include "despreader/despreader_core.h"

#include <stdlib.h>
#include <string.h>

/* Reset the bit-sync state (histogram + accumulators). */
static void
bitsync_reset (despreader_state_t *ch)
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
despreader_init (despreader_state_t *ch, const uint8_t *code, size_t code_len,
                 size_t sps, double init_norm_freq, double init_chip,
                 double bn_carrier, double bn_code, double bn_fll, double zeta,
                 double spacing, size_t periods_per_bit)
{
  size_t tsamps = (code_len ? code_len : 1) * (sps ? sps : 1);
  /* one carrier-loop update per code period (the integrate-and-dump window) */
  costas_init (&ch->car, bn_carrier, zeta, init_norm_freq, tsamps, bn_fll);
  dll_init (&ch->code, code, code_len, sps, init_chip, bn_code, zeta, spacing);
  ch->code_copy       = NULL;
  ch->periods_per_bit = periods_per_bit ? periods_per_bit : 1;
  ch->flip_hist       = NULL;
  if (ch->periods_per_bit > 1)
    ch->flip_hist = calloc (ch->periods_per_bit, sizeof (*ch->flip_hist));
  bitsync_reset (ch);
}

despreader_state_t *
despreader_create (const uint8_t *code, size_t code_len, size_t sps,
                   double init_norm_freq, double init_chip, double bn_carrier,
                   double bn_code, double bn_fll, double zeta, double spacing,
                   size_t periods_per_bit)
{
  if (!code || code_len == 0)
    return NULL;
  despreader_state_t *ch = calloc (1, sizeof (*ch));
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
despreader_destroy (despreader_state_t *state)
{
  if (!state)
    return;
  free (state->flip_hist);
  free (state->code_copy);
  free (state);
}

void
despreader_reset (despreader_state_t *state)
{
  costas_reset (&state->car);
  dll_reset (&state->code);
  bitsync_reset (state);
}

/* Serializable state — costas + dll children as nested sub-blobs, then the
 * running bit-sync histogram + scalars; the owned code copy is config
 * (create). */
size_t
despreader_state_bytes (const despreader_state_t *s)
{
  return sizeof (dp_state_hdr_t) + costas_state_bytes (&s->car)
         + dll_state_bytes (&s->code)
         + (s->flip_hist ? s->periods_per_bit * sizeof (size_t) : 0)
         + 3 * sizeof (uint64_t) + sizeof (double) + 2 * sizeof (uint32_t);
}

void
despreader_get_state (const despreader_state_t *s, void *blob)
{
  DP_GET_OPEN (DESPREADER_STATE_MAGIC, DESPREADER_STATE_VERSION,
               despreader_state_bytes (s));
  DP_W_CHILD (&_w, costas, &s->car);
  DP_W_CHILD (&_w, dll, &s->code);
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
despreader_set_state (despreader_state_t *s, const void *blob)
{
  DP_SET_OPEN (DESPREADER_STATE_MAGIC, DESPREADER_STATE_VERSION,
               despreader_state_bytes (s));
  DP_R_CHILD (&_r, costas, &s->car);
  DP_R_CHILD (&_r, dll, &s->code);
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
process_sample (despreader_state_t *ch, float complex x, float complex *prompt)
{
  float complex d = costas_wipeoff (&ch->car, x); /* carrier wipe-off */
  dll_accumulate (&ch->code, d);                  /* E/P/L correlate */
  if (ch->code.chip_pos < (double)ch->code.sf)
    return 0;
  /* code-period boundary */
  float complex P = ch->code.acc_p;
  dll_update (&ch->code);      /* code loop on the early/late envelopes */
  costas_update (&ch->car, P); /* carrier loop on the prompt symbol */
  ch->code.acc_e = ch->code.acc_p = ch->code.acc_l = 0.0f;
  *prompt = P / (float)(ch->code.sf * ch->code.sps);
  return 1;
}

size_t
despreader_steps_max_out (despreader_state_t *state)
{
  (void)state;
  return 0;
}

size_t
despreader_steps (despreader_state_t *state, const float complex *x,
                  size_t x_len, float complex *out, size_t max_out)
{
  size_t emitted = 0;
  for (size_t n = 0; n < x_len; n++)
    {
      float complex prompt;
      if (process_sample (state, x[n], &prompt) && emitted < max_out)
        out[emitted++] = prompt;
    }
  return emitted;
}

/* Feed one prompt to the bit-sync; emit a hard data bit when an aligned group
 * of periods_per_bit prompts completes. Returns 1 (and sets *bit) when a bit
 * is emitted. For periods_per_bit == 1 every prompt is a bit. */
static int
bit_sync (despreader_state_t *ch, float complex P, uint8_t *bit)
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
despreader_bits_max_out (despreader_state_t *state)
{
  (void)state;
  return 0;
}

size_t
despreader_bits (despreader_state_t *state, const float complex *x,
                 size_t x_len, uint8_t *out, size_t max_out)
{
  size_t emitted = 0;
  for (size_t n = 0; n < x_len; n++)
    {
      float complex prompt;
      if (!process_sample (state, x[n], &prompt))
        continue;
      uint8_t bit;
      if (bit_sync (state, prompt, &bit) && emitted < max_out)
        out[emitted++] = bit;
    }
  return emitted;
}

double
despreader_get_norm_freq (const despreader_state_t *state)
{
  return state->car.nco.norm_freq;
}

void
despreader_set_norm_freq (despreader_state_t *state, double val)
{
  costas_set_norm_freq (&state->car, val);
}

double
despreader_get_code_phase (const despreader_state_t *state)
{
  return state->code.chip_pos;
}

double
despreader_get_code_rate (const despreader_state_t *state)
{
  return state->code.code_rate;
}

double
despreader_get_lock_metric (const despreader_state_t *state)
{
  return state->car.lock_metric;
}

size_t
despreader_get_bit_phase (const despreader_state_t *state)
{
  return state->bit_phase;
}

double
despreader_get_bn_carrier (const despreader_state_t *state)
{
  return state->car.bn;
}

void
despreader_set_bn_carrier (despreader_state_t *state, double val)
{
  costas_set_bn (&state->car, val);
}

double
despreader_get_bn_code (const despreader_state_t *state)
{
  return state->code.bn;
}

void
despreader_set_bn_code (despreader_state_t *state, double val)
{
  dll_set_bn (&state->code, val);
}
