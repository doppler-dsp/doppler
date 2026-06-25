#include "despreader/despreader_core.h"
#include <complex.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Chip sign matching the transmit mapping (wfm_dsss_spread): 0 -> +1, 1 -> -1.
 */
static inline float
chip_sign (uint8_t c)
{
  return (c & 1u) ? -1.0f : 1.0f;
}

/* Seed/clear the per-symbol and loop state to the create-time conditions. */
static void
despreader_seed (despreader_state_t *s)
{
  loop_filter_reset (&s->lf_car);
  loop_filter_reset (&s->lf_code);
  /* The carrier integrator holds the per-symbol phase advance; seed it from
   * the create-time per-sample angular frequency. */
  s->lf_car.integ = s->seed_w * (double)s->tsamps;
  s->car_w        = s->seed_w;
  s->car_phase    = 0.0;
  s->chip_pos     = s->seed_chip;
  s->code_rate    = 1.0;
  s->acc_e = s->acc_p = s->acc_l = 0.0f;
  s->lock_metric                 = 0.0;
  s->snr_est                     = 0.0;
  s->preamble_left = s->acq_reps; /* re-arm preamble-aided pull-in */
}

despreader_state_t *
despreader_create (const uint8_t *code, size_t code_len, size_t sf, size_t sps,
                   double init_norm_freq, double init_chip_phase,
                   double bn_carrier, double bn_code)
{
  if (!code || code_len == 0 || sf == 0 || code_len < sf || sps < 2)
    return NULL;

  despreader_state_t *s = calloc (1, sizeof (*s));
  if (!s)
    return NULL;

  s->code = malloc (sf);
  if (!s->code)
    {
      free (s);
      return NULL;
    }
  for (size_t i = 0; i < sf; i++)
    s->code[i] = code[i];

  s->sf        = sf;
  s->sps       = sps;
  s->tsamps    = sf * sps;
  s->seed_w    = init_norm_freq * 2.0 * M_PI; /* cycles/sample -> rad/sample */
  s->seed_chip = init_chip_phase;

  /* Both loops update once per symbol, so the loop-filter update period is one
   * "unit"; bn is the loop noise bandwidth normalized to the symbol rate. */
  loop_filter_init (&s->lf_car, bn_carrier, 0.707, 1.0);
  loop_filter_init (&s->lf_code, bn_code, 0.707, 1.0);

  despreader_seed (s);
  return s;
}

void
despreader_destroy (despreader_state_t *state)
{
  if (!state)
    return;
  free (state->code);
  free (state->acq_code);
  free (state);
}

void
despreader_set_acq (despreader_state_t *state, const uint8_t *acq_code,
                    size_t acq_code_len, size_t acq_reps)
{
  free (state->acq_code);
  state->acq_code      = NULL;
  state->acq_sf        = 0;
  state->acq_reps      = 0;
  state->preamble_left = 0;
  if (!acq_code || acq_code_len == 0 || acq_reps == 0)
    return; /* disable: payload-only */
  state->acq_code = malloc (acq_code_len);
  if (!state->acq_code)
    return;
  for (size_t i = 0; i < acq_code_len; i++)
    state->acq_code[i] = acq_code[i];
  state->acq_sf        = acq_code_len;
  state->acq_reps      = acq_reps;
  state->preamble_left = acq_reps;
}

void
despreader_reset (despreader_state_t *state)
{
  despreader_seed (state);
}

/* Shared streaming kernel: carrier wipe-off, early/prompt/late despread, and
 * per-symbol integrate-and-dump driving the two tracking loops. Exactly one of
 * csym / bits is non-NULL; returns the number of symbols emitted. */
static size_t
despread_run (despreader_state_t *s, const float complex *x, size_t x_len,
              float complex *bitsym_csym, uint8_t *bits, size_t max_out)
{
  const double inv_sps = 1.0 / (double)s->sps;
  size_t       n_out   = 0;

  /* Current code/length: the acq code during the preamble, then the data code.
   * Re-evaluated at every symbol boundary (the only place preamble_left
   * moves). */
  int            preamble   = s->preamble_left > 0;
  const uint8_t *code       = preamble ? s->acq_code : s->code;
  size_t         cur_sf     = preamble ? s->acq_sf : s->sf;
  size_t         cur_tsamps = cur_sf * s->sps;

  for (size_t n = 0; n < x_len && n_out < max_out; n++)
    {
      /* Carrier wipe-off (inline NCO). */
      float complex carrier = cexpf ((float)(s->car_phase) * I);
      float complex d       = x[n] * conjf (carrier);
      s->car_phase += s->car_w;

      /* Early / prompt / late chip indices (early advanced by half a chip,
       * late delayed), wrapped over the periodic code. */
      double cp = s->chip_pos;
      size_t pj = (size_t)cp;
      if (pj >= cur_sf)
        pj = cur_sf - 1;
      double ce = cp + 0.5;
      if (ce >= (double)cur_sf)
        ce -= (double)cur_sf;
      double cl = cp - 0.5;
      if (cl < 0.0)
        cl += (double)cur_sf;
      size_t ej = (size_t)ce;
      size_t lj = (size_t)cl;
      if (ej >= cur_sf)
        ej = cur_sf - 1;
      if (lj >= cur_sf)
        lj = cur_sf - 1;

      s->acc_p += d * chip_sign (code[pj]);
      s->acc_e += d * chip_sign (code[ej]);
      s->acc_l += d * chip_sign (code[lj]);

      s->chip_pos += s->code_rate * inv_sps;

      if (s->chip_pos < (double)cur_sf)
        continue;

      /* ── symbol/period boundary: dump and update both loops ── */
      float complex P = s->acc_p;
      if (!preamble)
        {
          /* Emit only payload symbols; the preamble pulls the loops in. */
          if (bitsym_csym)
            bitsym_csym[n_out] = P / (float)cur_tsamps;
          else
            bits[n_out] = (crealf (P) >= 0.0f) ? 1u : 0u;
          n_out++;
        }

      /* DLL: normalized non-coherent early-minus-late envelope. */
      float  me = cabsf (s->acc_e), ml = cabsf (s->acc_l);
      double e_dll = (double)(me - ml) / ((double)(me + ml) + 1e-12);
      loop_filter_step (&s->lf_code, e_dll);
      s->code_rate = 1.0 + s->lf_code.integ;
      s->chip_pos -= (double)cur_sf;
      s->chip_pos
          += s->lf_code.kp * e_dll; /* proportional phase nudge (chips) */

      /* Costas: the preamble symbol is a known +1, so use a coherent
       * full-range atan2 discriminator (pulls in a wide residual); the data
       * payload uses a decision-directed, amplitude-normalized detector. */
      float  reP = crealf (P), imP = cimagf (P);
      float  aP    = cabsf (P) + 1e-12f;
      double e_cos = preamble ? (double)atan2f (imP, reP)
                              : (double)(((reP >= 0.0f) ? imP : -imP) / aP);
      loop_filter_step (&s->lf_car, e_cos);
      s->car_w
          = s->lf_car.integ / (double)cur_tsamps; /* per-symbol -> /sample */
      s->car_phase += s->lf_car.kp * e_cos;       /* phase nudge (rad) */

      /* Status read-backs (EMA). */
      double inst_lock = (double)fabsf (reP) / (double)aP;
      s->lock_metric += 0.1 * (inst_lock - s->lock_metric);
      double inst_snr = (double)(reP * reP) / ((double)(imP * imP) + 1e-12);
      s->snr_est += 0.1 * (inst_snr - s->snr_est);

      s->acc_e = s->acc_p = s->acc_l = 0.0f;

      /* Preamble -> payload transition: switch to the data code, preserving
       * the tracked carrier rate across the symbol-period change. */
      if (preamble)
        {
          s->preamble_left--;
          if (s->preamble_left == 0)
            {
              size_t new_tsamps = s->sf * s->sps;
              s->lf_car.integ   = s->car_w * (double)new_tsamps;
              preamble          = 0;
              code              = s->code;
              cur_sf            = s->sf;
              cur_tsamps        = new_tsamps;
            }
        }
    }
  return n_out;
}

size_t
despreader_steps_max_out (despreader_state_t *state)
{
  (void)state;
  return 0; /* binding sizes the buffer to the input length (>= #symbols) */
}

size_t
despreader_steps (despreader_state_t *state, const float complex *x,
                  size_t x_len, float complex *out, size_t max_out)
{
  return despread_run (state, x, x_len, out, NULL, max_out);
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
  return despread_run (state, x, x_len, NULL, out, max_out);
}

/* ── property accessors ── */
double
despreader_get_bn_carrier (const despreader_state_t *s)
{
  return s->lf_car.bn;
}
void
despreader_set_bn_carrier (despreader_state_t *s, double val)
{
  loop_filter_configure (&s->lf_car, val, s->lf_car.zeta, s->lf_car.t);
}
double
despreader_get_bn_code (const despreader_state_t *s)
{
  return s->lf_code.bn;
}
void
despreader_set_bn_code (despreader_state_t *s, double val)
{
  loop_filter_configure (&s->lf_code, val, s->lf_code.zeta, s->lf_code.t);
}
double
despreader_get_norm_freq (const despreader_state_t *s)
{
  return s->car_w / (2.0 * M_PI); /* rad/sample -> cycles/sample */
}
void
despreader_set_norm_freq (despreader_state_t *s, double val)
{
  s->car_w        = val * 2.0 * M_PI;
  s->lf_car.integ = s->car_w * (double)s->tsamps;
}
double
despreader_get_code_phase (const despreader_state_t *s)
{
  return s->chip_pos;
}
double
despreader_get_lock_metric (const despreader_state_t *s)
{
  return s->lock_metric;
}
double
despreader_get_snr_est (const despreader_state_t *s)
{
  return s->snr_est;
}
