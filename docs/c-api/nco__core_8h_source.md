

# File nco\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**nco**](dir_2f9ed967bc16fefd26d0244d883adb58.md) **>** [**nco\_core.h**](nco__core_8h.md)

[Go to the documentation of this file](nco__core_8h.md)


```C++

#ifndef NCO_CORE_H
#define NCO_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include <math.h>
#ifdef __cplusplus
extern "C"
{
#endif

  JM_FORCEINLINE uint32_t
  nco_norm_to_inc (double cycles)
  {
    double d = cycles - floor (cycles); /* fractional cycles, [0, 1) */
    /* Truncate toward zero: the C99 float->unsigned conversion (6.3.1.4)
       discards the fractional part, and d < 1 makes d*2^32 strictly < 2^32,
       so the result is always in [0, 2^32) -- the documented contract, with
       no host-FP-sensitive rounding. (llround here could round d*2^32 UP to
       exactly 2^32 for d ~ 0.9999..., and (uint32_t)2^32 == 0 would freeze
       the NCO -- x86 landed on 2^32-1, arm64 on 2^32, hanging the closed
       loop on arm64.) */
    return (uint32_t)(d * 4294967296.0);
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
  } nco_state_t;

  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32 (nco_state_t *state)
  {
    uint32_t ph = state->phase;
    state->phase = ph + state->phase_inc;
    return ph;
  }

  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_scaled (nco_state_t *state)
  {
    uint32_t ph   = state->phase;
    uint32_t nmax = state->nmax;
    state->phase  = ph + state->phase_inc;
    return nmax == 0 ? ph : (uint32_t)(((uint64_t)ph * nmax) >> 32);
  }

  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_ovf (nco_state_t *state, uint8_t *carry)
  {
    uint32_t ph = state->phase;
    *carry      = NCO_ADD_OVF (ph, state->phase_inc, &state->phase);
    return ph;
  }

  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_ctrl (nco_state_t *state, double ctrl)
  {
    uint32_t ph = state->phase;
    state->phase = ph + state->phase_inc + nco_norm_to_inc (ctrl);
    return ph;
  }

  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_scaled_ctrl (nco_state_t *state, double ctrl)
  {
    uint32_t ph   = state->phase;
    uint32_t nmax = state->nmax;
    state->phase  = ph + state->phase_inc + nco_norm_to_inc (ctrl);
    return nmax == 0 ? ph : (uint32_t)(((uint64_t)ph * nmax) >> 32);
  }

  JM_FORCEINLINE JM_HOT uint32_t
  nco_step_u32_ovf_ctrl (nco_state_t *state, double ctrl, uint8_t *carry)
  {
    uint32_t ph  = state->phase;
    uint64_t sum = (uint64_t)ph + (uint64_t)state->phase_inc
                   + (uint64_t)nco_norm_to_inc (ctrl);
    *carry        = (uint8_t)((sum >> 32) != 0);
    state->phase  = (uint32_t)sum;
    return ph;
  }

  nco_state_t *nco_create (double norm_freq, uint32_t nmax);

  void nco_destroy (nco_state_t *state);

  void nco_reset (nco_state_t *state);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Only the running phase accumulator is serialized; phase_inc / nmax are
   * config restored by the constructor.  Envelope: [dp_state_hdr_t][u32
   * phase].
   */
#define NCO_STATE_MAGIC DP_FOURCC ('N', 'C', 'O', '_')
#define NCO_STATE_VERSION 1u

  size_t nco_state_bytes (const nco_state_t *state);
  void nco_get_state (const nco_state_t *state, void *blob);
  int nco_set_state (nco_state_t *state, const void *blob);

  /* ---- Properties ---- */

  double nco_get_norm_freq (const nco_state_t *state);
  void   nco_set_norm_freq (nco_state_t *state, double norm_freq);

  uint32_t nco_get_phase (const nco_state_t *state);
  void     nco_set_phase (nco_state_t *state, uint32_t phase);

  uint32_t nco_get_phase_inc (const nco_state_t *state);

  /* ---- Block generators ---- */

  size_t nco_steps_u32_max_out (nco_state_t *state);

  size_t nco_steps_u32 (nco_state_t *state, size_t n, uint32_t *out,
                        size_t max_out);

  size_t nco_steps_u32_scaled_max_out (nco_state_t *state);

  size_t nco_steps_u32_scaled (nco_state_t *state, size_t n, uint32_t *out,
                               size_t max_out);

  size_t nco_steps_u32_ovf_max_out (nco_state_t *state);

  size_t nco_steps_u32_ovf (nco_state_t *state, size_t n, uint32_t *out,
                            uint8_t *out1, size_t max_out);

  size_t nco_steps_u32_ctrl_max_out (nco_state_t *state);

  size_t nco_steps_u32_ctrl (nco_state_t *state, const float *ctrl,
                             size_t ctrl_len, uint32_t *out,
                             size_t max_out);

  size_t nco_steps_u32_scaled_ctrl_max_out (nco_state_t *state);

  size_t nco_steps_u32_scaled_ctrl (nco_state_t *state, const float *ctrl,
                                    size_t ctrl_len, uint32_t *out,
                                    size_t max_out);

  size_t nco_steps_u32_ovf_ctrl_max_out (nco_state_t *state);

  size_t nco_steps_u32_ovf_ctrl (nco_state_t *state, const float *ctrl,
                                 size_t ctrl_len, uint32_t *out,
                                 uint8_t *out1, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* NCO_CORE_H */
```


