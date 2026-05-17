

# File nco\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**nco**](dir_2f9ed967bc16fefd26d0244d883adb58.md) **>** [**nco\_core.h**](nco__core_8h.md)

[Go to the documentation of this file](nco__core_8h.md)


```C++

#ifndef NCO_CORE_H
#define NCO_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C"
{
#endif

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
    double norm_freq;   /* normalised frequency (cycles/sample)          */
    uint32_t nmax;      /* wrap target for steps_u32_scaled; 0 = raw   */
  } nco_state_t;

  nco_state_t *nco_create (double norm_freq, uint32_t nmax);

  void nco_destroy (nco_state_t *state);

  void nco_reset (nco_state_t *state);

  /* ---- Properties ---- */

  double nco_get_norm_freq (const nco_state_t *state);
  void nco_set_norm_freq (nco_state_t *state, double norm_freq);
  uint32_t nco_get_phase (const nco_state_t *state);
  void nco_set_phase (nco_state_t *state, uint32_t phase);
  uint32_t nco_get_phase_inc (const nco_state_t *state);

  /* ---- Block generators ---- */

  size_t nco_steps_u32_max_out (nco_state_t *state);

  size_t nco_steps_u32 (nco_state_t *state, size_t n, uint32_t *out);

  size_t nco_steps_u32_scaled_max_out (nco_state_t *state);

  size_t nco_steps_u32_scaled (nco_state_t *state, size_t n, uint32_t *out);

  size_t nco_steps_u32_ovf_max_out (nco_state_t *state);

  size_t nco_steps_u32_ovf (nco_state_t *state, size_t n, uint32_t *out,
                            uint8_t *out1);

#ifdef __cplusplus
}
#endif

#endif /* NCO_CORE_H */
```


