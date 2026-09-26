

# File nco\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**nco**](dir_64d3c3497a73d925321234bd516bd8bf.md) **>** [**nco\_core.h**](nco__core_8h.md)

[Go to the documentation of this file](nco__core_8h.md)


```C++

#ifndef DP_NCO_CORE_H
#define DP_NCO_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif

  JM_FORCEINLINE uint32_t
  nco_phase_units (double units)
  {
    /* Establish 6.3.1.4's precondition rather than assume it. The negated
       form is deliberate: every comparison with NaN is false, so `!(u > 0)`
       rejects NaN where `u < 0.0` would wave it through to the cast. */
    if (!(units > 0.0))
      return 0u;
    if (units >= 4294967296.0)
      return 4294967295u;
    return (uint32_t)units;
  }

  JM_FORCEINLINE uint32_t
  nco_phase_units_mod (double units)
  {
    if (!(units > 0.0))
      return 0u;
    if (units < 4294967296.0)
      return (uint32_t)units;
    return (uint32_t)fmod (units, 4294967296.0);
  }

  JM_FORCEINLINE double
  nco_word_to_norm (uint32_t word)
  {
    return (double)word / 4294967296.0;
  }

  JM_FORCEINLINE uint32_t
  nco_norm_fold_ (double norm)
  {
    /* The fold is in [0, 1) mathematically but NOT in floating point: for
       any norm in [-2^-53, 0) the subtraction rounds up to exactly 1.0
       (`-1e-20 - (-1) == 1.0`), so the product reaches 2^32 and only
       nco_phase_units() keeps that out of the cast. The true fraction is
       then in (1 - 2^-53, 1), whose truncation is 2^32-1 -- the same value
       -1e-16, one representable step away, already returns. */
    double d = norm - floor (norm);
    return nco_phase_units (d * 4294967296.0);
  }

  JM_FORCEINLINE uint32_t
  nco_norm_freq_to_inc (double norm_freq)
  {
    return nco_norm_fold_ (norm_freq);
  }

  JM_FORCEINLINE uint32_t
  nco_norm_phase_to_word (double norm_phase)
  {
    return nco_norm_fold_ (norm_phase);
  }

  JM_FORCEINLINE double
  nco_steer_scale (double control, double lo, double hi)
  {
    double scale = 1.0 + control;
    /* Negated, so NaN lands on lo rather than sailing through -- the same
       reasoning as nco_phase_units(). */
    if (!(scale > lo))
      return lo;
    if (scale > hi)
      return hi;
    return scale;
  }

#if defined(__GNUC__) || defined(__clang__)
#define NCO_ADD_OVF(a, b, res)                                                \
  ((uint8_t)__builtin_add_overflow ((uint32_t)(a), (uint32_t)(b),             \
                                    (uint32_t *)(res)))
#else
static inline uint8_t
nco_add_ovf_ (uint32_t a, uint32_t b, uint32_t *res)
{
  *res = a + b;
  return (uint8_t)(*res < a);
}
#define NCO_ADD_OVF(a, b, res) nco_add_ovf_ ((a), (b), (res))
#endif

  typedef struct
  {
    uint32_t phase;     /* current accumulator value [0, 2^32)         */
    uint32_t phase_inc; /* advance per sample = floor(norm_freq * 2^32) */
    double   norm_freq; /* normalised frequency (cycles/sample)          */
    uint32_t nmax;      /* wrap target for steps_u32_scaled; 0 = raw   */
  } dp_nco_state_t;

  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32 (dp_nco_state_t *state)
  {
    uint32_t ph = state->phase;
    state->phase = ph + state->phase_inc;
    return ph;
  }

  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_scaled (dp_nco_state_t *state)
  {
    uint32_t ph   = state->phase;
    uint32_t nmax = state->nmax;
    state->phase  = ph + state->phase_inc;
    return nmax == 0 ? ph : (uint32_t)(((uint64_t)ph * nmax) >> 32);
  }

  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_ovf (dp_nco_state_t *state, uint8_t *carry)
  {
    uint32_t ph = state->phase;
    *carry      = NCO_ADD_OVF (ph, state->phase_inc, &state->phase);
    return ph;
  }

  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_ctrl (dp_nco_state_t *state, double ctrl)
  {
    uint32_t ph = state->phase;
    state->phase = ph + state->phase_inc + nco_norm_freq_to_inc (ctrl);
    return ph;
  }

  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_scaled_ctrl (dp_nco_state_t *state, double ctrl)
  {
    uint32_t ph   = state->phase;
    uint32_t nmax = state->nmax;
    state->phase  = ph + state->phase_inc + nco_norm_freq_to_inc (ctrl);
    return nmax == 0 ? ph : (uint32_t)(((uint64_t)ph * nmax) >> 32);
  }

  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_ovf_ctrl (dp_nco_state_t *state, double ctrl, uint8_t *carry)
  {
    uint32_t ph = state->phase;
    /* Wrapping u32 add: bit-for-bit the modulo advance the 64-bit sum
       used to produce -- only the event derivation below changes. */
    uint32_t nph   = ph + state->phase_inc + nco_norm_freq_to_inc (ctrl);
    double   delta = state->norm_freq + ctrl; /* signed, pre-fold */

    state->phase = nph;
    if (delta > 0.0)
      *carry = (uint8_t)(nph < ph || delta >= 1.0);
    else if (delta < 0.0)
      *carry = (uint8_t)(nph > ph || delta <= -1.0);
    else
      *carry = 0;
    return ph;
  }

  dp_nco_state_t *dp_nco_create (double norm_freq, uint32_t nmax);

  void dp_nco_destroy (dp_nco_state_t *state);

  void dp_nco_reset (dp_nco_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Only the running phase accumulator is serialized; phase_inc / nmax are
   * config restored by the constructor.  Envelope: [dp_state_hdr_t][u32
   * phase].
   */
#define NCO_STATE_MAGIC DP_FOURCC ('N', 'C', 'O', '_')
#define NCO_STATE_VERSION 1u

  size_t dp_nco_state_bytes (const dp_nco_state_t *state);
  void dp_nco_get_state (const dp_nco_state_t *state, void *blob);
  int dp_nco_set_state (dp_nco_state_t *state, const void *blob);

  /* ---- Properties ---- */

  double dp_nco_get_norm_freq (const dp_nco_state_t *state);
  void   dp_nco_set_norm_freq (dp_nco_state_t *state, double norm_freq);

  uint32_t dp_nco_get_phase (const dp_nco_state_t *state);
  void     dp_nco_set_phase (dp_nco_state_t *state, uint32_t phase);

  uint32_t dp_nco_get_phase_inc (const dp_nco_state_t *state);

  /* ---- Block generators ---- */

  size_t dp_nco_steps_u32_max_out (dp_nco_state_t *state);

  size_t dp_nco_steps_u32 (dp_nco_state_t *state, size_t n, uint32_t *out,
                        size_t max_out);

  size_t dp_nco_steps_u32_scaled_max_out (dp_nco_state_t *state);

  size_t dp_nco_steps_u32_scaled (dp_nco_state_t *state, size_t n, uint32_t *out,
                               size_t max_out);

  size_t dp_nco_steps_u32_ovf_max_out (dp_nco_state_t *state);

  size_t dp_nco_steps_u32_ovf (dp_nco_state_t *state, size_t n, uint32_t *out,
                            uint8_t *out1, size_t max_out);

  size_t dp_nco_steps_u32_ctrl_max_out (dp_nco_state_t *state);

  size_t dp_nco_steps_u32_ctrl (dp_nco_state_t *state, const double *ctrl,
                             size_t ctrl_len, uint32_t *out,
                             size_t max_out);

  size_t dp_nco_steps_u32_scaled_ctrl_max_out (dp_nco_state_t *state);

  size_t dp_nco_steps_u32_scaled_ctrl (dp_nco_state_t *state, const double *ctrl,
                                    size_t ctrl_len, uint32_t *out,
                                    size_t max_out);

  size_t dp_nco_steps_u32_ovf_ctrl_max_out (dp_nco_state_t *state);

  size_t dp_nco_steps_u32_ovf_ctrl (dp_nco_state_t *state, const double *ctrl,
                                 size_t ctrl_len, uint32_t *out,
                                 uint8_t *out1, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* NCO_CORE_H */
```


