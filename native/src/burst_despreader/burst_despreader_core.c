#include "doppler/burst_despreader/burst_despreader_core.h"
#include "doppler/dp_complex.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Chip sign matching the transmit mapping (dp_wfm_dsss_spread): 0 -> +1, 1 ->
 * -1.
 */
static inline float
chip_sign (uint8_t c)
{
  return (c & 1u) ? -1.0f : 1.0f;
}

/* Seed/clear the per-symbol and loop state to the create-time conditions. */
static void
burst_despreader_seed (dp_burst_despreader_state_t *s)
{
  dp_loop_filter_reset (&s->lf_car);
  dp_loop_filter_reset (&s->lf_code);
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
  s->sum_lock                    = 0.0;
  s->sum_re2                     = 0.0;
  s->sum_im2                     = 0.0;
  s->stat_n                      = 0;
  s->preamble_left = s->acq_reps; /* re-arm preamble-aided pull-in */
}

dp_burst_despreader_state_t *
dp_burst_despreader_create (const uint8_t *code, size_t code_len, size_t sf,
                            size_t sps, double init_norm_freq,
                            double init_chip_phase, double bn_carrier,
                            double bn_code)
{
  /* The seeds are what reset() returns to and what set_state compares, so
     a non-finite one is refused here: NaN would equal nothing, its own
     blob included (#2041). */
  if (!code || code_len == 0 || sf == 0 || code_len < sf || sps < 2
      || !isfinite (init_norm_freq) || !isfinite (init_chip_phase))
    return NULL;

  dp_burst_despreader_state_t *s = calloc (1, sizeof (*s));
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
  dp_loop_filter_init (&s->lf_car, bn_carrier, 0.707, 1.0);
  dp_loop_filter_init (&s->lf_code, bn_code, 0.707, 1.0);

  burst_despreader_seed (s);
  return s;
}

void
dp_burst_despreader_destroy (dp_burst_despreader_state_t *state)
{
  if (!state)
    return;
  free (state->code);
  free (state->acq_code);
  free (state);
}

/* set_acq's predicate, ONE home: a preamble is a code of acq_sf > 0 chips
   tracked for acq_reps > 0 periods, or there is none and both are 0;
   preamble_left counts the periods still to track, so it never exceeds
   acq_reps (0 with no preamble). set_acq leaves only such states, and
   set_state accepts only such states (#2041). */
static int
burst_despreader_acq_ok (size_t acq_sf, size_t acq_reps, size_t preamble_left)
{
  if ((acq_sf == 0) != (acq_reps == 0))
    return 0;
  return preamble_left <= acq_reps;
}

void
dp_burst_despreader_set_acq (dp_burst_despreader_state_t *state,
                             const uint8_t *acq_code, size_t acq_code_len,
                             size_t acq_reps)
{
  free (state->acq_code);
  state->acq_code      = NULL;
  state->acq_sf        = 0;
  state->acq_reps      = 0;
  state->preamble_left = 0;
  if (!acq_code || acq_code_len == 0
      || !burst_despreader_acq_ok (acq_code_len, acq_reps, 0))
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
dp_burst_despreader_reset (dp_burst_despreader_state_t *state)
{
  burst_despreader_seed (state);
}

/* Serializable state: [header][the struct, its two pointers zeroed][the acq
   code, acq_sf bytes]. The data code is create-time config, rebuilt by
   create; the acq code is NOT -- only set_acq() sets it -- so, being a
   setter's value, it travels (#2022). Its length is the blob's size: a
   blob from an object whose acq code is another length, or that has none,
   is the wrong size and refused before anything is read (#2041). */
size_t
dp_burst_despreader_state_bytes (const dp_burst_despreader_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (dp_burst_despreader_state_t)
         + s->acq_sf;
}

void
dp_burst_despreader_get_state (const dp_burst_despreader_state_t *s,
                               void                              *blob)
{
  DP_GET_OPEN (BURST_DESPREADER_STATE_MAGIC, BURST_DESPREADER_STATE_VERSION,
               dp_burst_despreader_state_bytes (s));
  /* memcpy, not assignment: the padding is copied too, so the blob is a
     function of the object. */
  dp_burst_despreader_state_t tmp;
  memcpy (&tmp, s, sizeof tmp);
  tmp.code     = NULL; /* machine addresses, not state */
  tmp.acq_code = NULL;
  dp_w_bytes (&_w, &tmp, sizeof tmp);
  if (s->acq_sf)
    dp_w_bytes (&_w, s->acq_code, s->acq_sf);
}

/* A loop filter from a blob. Its damping and update period are this
   object's create-time config, which no setter reaches, so they are reject
   keys. Its bn is set_bn_*'s value, so it travels (#2022), and its gains
   must be the ones that bn derives -- dp_loop_filter_init's derivation, not
   a copy of it. Every number in it must be finite, as a healthy run leaves
   it. */
static int
burst_despreader_lf_ok (const dp_loop_filter_state_t *b,
                        const dp_loop_filter_state_t *live)
{
  if (b->zeta != live->zeta || b->t != live->t)
    return 0;
  dp_loop_filter_state_t d = *b;
  dp_loop_filter_init (&d, b->bn, b->zeta, b->t);
  return d.kp == b->kp && d.ki == b->ki && isfinite (b->bn) && isfinite (b->kp)
         && isfinite (b->ki) && isfinite (b->integ);
}

int
dp_burst_despreader_set_state (dp_burst_despreader_state_t *s,
                               const void                  *blob)
{
  DP_SET_OPEN (BURST_DESPREADER_STATE_MAGIC, BURST_DESPREADER_STATE_VERSION,
               dp_burst_despreader_state_bytes (s));
  /* Decoded into a temporary and checked whole before anything is written:
     a refused blob leaves this object exactly as it was. It used to be read
     straight into the live object, sf and the acq fields included, so a
     foreign blob indexed this object's codes past their ends -- or a NULL
     acq code -- in the next steps() (#2041). */
  dp_burst_despreader_state_t tmp;
  dp_r_bytes (&_r, &tmp, sizeof tmp);
  const void *acq = s->acq_sf ? dp_r_reserve (&_r, s->acq_sf) : NULL;
  if (_r.err
      /* Create-time config that sizes or indexes the code buffers: a blob
         made for another code length or chip rate is refused. */
      || tmp.sf != s->sf || tmp.sps != s->sps
      || tmp.tsamps != s->tsamps
      /* More create-time config, which reset() reseeds from. */
      || tmp.seed_w != s->seed_w || tmp.seed_chip != s->seed_chip
      || !burst_despreader_lf_ok (&tmp.lf_car, &s->lf_car)
      || !burst_despreader_lf_ok (&tmp.lf_code, &s->lf_code)
      /* set_acq's value: this object's acq-code length (the blob's size
         already agrees; the field must too), in a state set_acq leaves. */
      || tmp.acq_sf != s->acq_sf
      || !burst_despreader_acq_ok (tmp.acq_sf, tmp.acq_reps, tmp.preamble_left)
      /* Running state, finite as a healthy run leaves it. One NaN input
         sample poisons the loops for good, and such a state is refused
         rather than restored. No range bound: the kernel's chip index is
         total (chip_index), and a set_acq'd object holds a chip_pos past
         its acq-code length until the next boundary, so a bound would
         refuse a blob the object itself made. */
      || !isfinite (tmp.car_phase) || !isfinite (tmp.car_w)
      || !isfinite (tmp.chip_pos)
      || !isfinite (tmp.code_rate)
      /* stat_n == SIZE_MAX would wrap to 0 at the next payload prompt and
         divide the lock metric by zero. */
      || tmp.stat_n == SIZE_MAX)
    return DP_ERR_INVALID;

  uint8_t *code     = s->code; /* this object's own buffers */
  uint8_t *acq_code = s->acq_code;
  memcpy (s, &tmp, sizeof *s);
  s->code     = code;
  s->acq_code = acq_code;
  if (s->acq_sf)
    memcpy (s->acq_code, acq, s->acq_sf);
  return DP_OK;
}

/* A code position as a chip index, total over every double. The cast alone
   is undefined for NaN, for values <= -1 and past SIZE_MAX, and a create
   argument, a blob or one NaN input sample can put any of those in chip_pos
   (#2041). So the clamps the kernel always applied are taken in double,
   BEFORE the cast: below 0 (and NaN) is chip 0, past the last chip is the
   last chip, and in between it truncates exactly as before. */
static inline size_t
chip_index (double pos, size_t n)
{
  if (!(pos > 0.0))
    return 0;
  if (pos >= (double)(n - 1))
    return n - 1;
  return (size_t)pos;
}

/* Shared streaming kernel: carrier wipe-off, early/prompt/late despread, and
 * per-symbol integrate-and-dump driving the two tracking loops. Exactly one of
 * csym / bits is non-NULL; returns the number of symbols emitted. */
static size_t
despread_run (dp_burst_despreader_state_t *s, const float _Complex *x,
              size_t x_len, float _Complex *bitsym_csym, uint8_t *bits,
              size_t max_out)
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
      float _Complex carrier = cexpf ((float)(s->car_phase) * I);
      float _Complex d       = x[n] * conjf (carrier);
      s->car_phase += s->car_w;

      /* Early / prompt / late chip indices (early advanced by half a chip,
       * late delayed), wrapped over the periodic code. */
      double cp = s->chip_pos;
      double ce = cp + 0.5;
      if (ce >= (double)cur_sf)
        ce -= (double)cur_sf;
      double cl = cp - 0.5;
      if (cl < 0.0)
        cl += (double)cur_sf;
      size_t pj = chip_index (cp, cur_sf);
      size_t ej = chip_index (ce, cur_sf);
      size_t lj = chip_index (cl, cur_sf);

      s->acc_p += d * chip_sign (code[pj]);
      s->acc_e += d * chip_sign (code[ej]);
      s->acc_l += d * chip_sign (code[lj]);

      s->chip_pos += s->code_rate * inv_sps;

      if (s->chip_pos < (double)cur_sf)
        continue;

      /* ── symbol/period boundary: dump and update both loops ── */
      float _Complex P = s->acc_p;
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
      dp_loop_filter_step (&s->lf_code, e_dll);
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
      dp_loop_filter_step (&s->lf_car, e_cos);
      s->car_w
          = s->lf_car.integ / (double)cur_tsamps; /* per-symbol -> /sample */
      s->car_phase += s->lf_car.kp * e_cos;       /* phase nudge (rad) */

      /* Status read-backs — cumulative over the burst, not EMA: a burst
       * is one-shot, and a fixed-alpha smoother is warmup-dominated for
       * its entire length (alpha = 0.1 is a ~19-symbol memory). Cumulative
       * sums weight every prompt equally and feed the calibrated
       * whole-burst lock statistic (dp_burst_despreader_get_lock_stat).
       * PAYLOAD prompts only: a preamble prompt integrates a different
       * code length (acq_sf vs sf — mixed variance scales would break
       * both the F(n,n) H0 law and the SNR calibration) and carries
       * pull-in transients that bias snr_est low. */
      if (!preamble)
        {
          double inst_lock = (double)fabsf (reP) / (double)aP;
          s->sum_lock += inst_lock;
          s->sum_re2 += (double)reP * (double)reP;
          s->sum_im2 += (double)imP * (double)imP;
          s->stat_n++;
          s->lock_metric = s->sum_lock / (double)s->stat_n;
          /* Accumulate-then-ratio SNR: E[Re^2] = A^2 + sigma^2, E[Im^2]
           * = sigma^2, so (sum Re^2 - sum Im^2)/sum Im^2 estimates
           * A^2/sigma^2 directly; clamp noise-only negative excursions. */
          double diff = s->sum_re2 - s->sum_im2;
          s->snr_est
              = (diff > 0.0 && s->sum_im2 > 0.0) ? diff / s->sum_im2 : 0.0;
        }

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
dp_burst_despreader_steps_max_out (dp_burst_despreader_state_t *state)
{
  (void)state;
  return 0; /* binding sizes the buffer to the input length (>= #symbols) */
}

size_t
dp_burst_despreader_steps (dp_burst_despreader_state_t *state,
                           const float _Complex *x, size_t x_len,
                           float _Complex *out, size_t max_out)
{
  return despread_run (state, x, x_len, out, NULL, max_out);
}

size_t
dp_burst_despreader_bits_max_out (dp_burst_despreader_state_t *state)
{
  (void)state;
  return 0; /* one hard bit per code period, so bits <= inputs */
}

size_t
dp_burst_despreader_bits (dp_burst_despreader_state_t *state,
                          const float _Complex *x, size_t x_len, uint8_t *out,
                          size_t max_out)
{
  return despread_run (state, x, x_len, NULL, out, max_out);
}

/* ── property accessors ── */
double
dp_burst_despreader_get_bn_carrier (const dp_burst_despreader_state_t *s)
{
  return s->lf_car.bn;
}
void
dp_burst_despreader_set_bn_carrier (dp_burst_despreader_state_t *s, double val)
{
  dp_loop_filter_configure (&s->lf_car, val, s->lf_car.zeta, s->lf_car.t);
}
double
dp_burst_despreader_get_bn_code (const dp_burst_despreader_state_t *s)
{
  return s->lf_code.bn;
}
void
dp_burst_despreader_set_bn_code (dp_burst_despreader_state_t *s, double val)
{
  dp_loop_filter_configure (&s->lf_code, val, s->lf_code.zeta, s->lf_code.t);
}
double
dp_burst_despreader_get_norm_freq (const dp_burst_despreader_state_t *s)
{
  return s->car_w / (2.0 * M_PI); /* rad/sample -> cycles/sample */
}
void
dp_burst_despreader_set_norm_freq (dp_burst_despreader_state_t *s, double val)
{
  s->car_w        = val * 2.0 * M_PI;
  s->lf_car.integ = s->car_w * (double)s->tsamps;
}
double
dp_burst_despreader_get_code_phase (const dp_burst_despreader_state_t *s)
{
  return s->chip_pos;
}
double
dp_burst_despreader_get_lock_metric (const dp_burst_despreader_state_t *s)
{
  return s->lock_metric;
}
double
dp_burst_despreader_get_snr_est (const dp_burst_despreader_state_t *s)
{
  return s->snr_est;
}

double
dp_burst_despreader_get_lock_stat (const dp_burst_despreader_state_t *s)
{
  if (s->stat_n == 0 || s->sum_im2 <= 0.0)
    return 0.0;
  return sqrt ((double)s->stat_n * s->sum_re2 / s->sum_im2);
}

size_t
dp_burst_despreader_get_stat_n (const dp_burst_despreader_state_t *s)
{
  return s->stat_n;
}
