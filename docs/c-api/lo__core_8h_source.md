

# File lo\_core.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**lo**](dir_e3bbeba8c021d4d74db794db08bafd77.md) **>** [**lo\_core.h**](lo__core_8h.md)

[Go to the documentation of this file](lo__core_8h.md)


```C++

#ifndef LO_CORE_H
#define LO_CORE_H

#include "clib_common.h"
#include "dp_state.h"
#include "jm_perf.h"
#include <math.h> /* floor() in the lo_step_ctrl control port */
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    uint32_t phase;     /* current accumulator value [0, 2^32)          */
    uint32_t phase_inc; /* advance per sample = floor(norm_freq * 2^32) */
    double norm_freq;   /* normalised frequency (cycles/sample)           */
  } lo_state_t;

/* ---- Inline composition API (C-only; not exposed as Python methods) ----
 *
 * lo_init / lo_step let a tracking loop embed lo_state_t BY VALUE and de-rotate
 * a sample stream one sample at a time with zero call overhead — the block
 * generators below (lo_steps) stay the fast path for bulk synthesis.  The
 * shared sin LUT is exposed here so the inline step can index it directly.   */
#define LO_LUT_BITS 16u
#define LO_LUT_SIZE (1u << LO_LUT_BITS) /* 65536                    */
#define LO_LUT_QTR (LO_LUT_SIZE >> 2u)  /* 16384  (π/2 phase shift) */

  extern float lo_sin_lut[LO_LUT_SIZE];

  void lo_init (lo_state_t *state, double norm_freq);

  JM_FORCEINLINE JM_HOT float complex lo_step (lo_state_t *state)
  {
    uint16_t idx = (uint16_t)(state->phase >> (32u - LO_LUT_BITS));
    float complex out
        = CMPLXF (lo_sin_lut[(uint16_t)(idx + (uint16_t)LO_LUT_QTR)],
                  lo_sin_lut[idx]);
    state->phase += state->phase_inc;
    return out;
  }

  JM_FORCEINLINE JM_HOT float complex lo_step_ctrl (lo_state_t *state,
                                                    double ctrl)
  {
    uint16_t idx = (uint16_t)(state->phase >> (32u - LO_LUT_BITS));
    float complex out
        = CMPLXF (lo_sin_lut[(uint16_t)(idx + (uint16_t)LO_LUT_QTR)],
                  lo_sin_lut[idx]);
    /* Fractional cycle of the control → 32-bit phase units (the LO's own
     * scaling); floor handles negative corrections by wrapping mod 1 cycle. */
    double f = ctrl - floor (ctrl);
    state->phase += state->phase_inc + (uint32_t)(f * 4294967296.0);
    return out;
  }

  lo_state_t *lo_create (double norm_freq);

  void lo_destroy (lo_state_t *state);

  void lo_reset (lo_state_t *state);

  /* ---- Properties ---- */

  double lo_get_norm_freq (const lo_state_t *state);
  void lo_set_norm_freq (lo_state_t *state, double norm_freq);

  uint32_t lo_get_phase (const lo_state_t *state);
  void lo_set_phase (lo_state_t *state, uint32_t phase);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Every composable filter exposes this triplet so a pure transducer
   * (ddc_fn / acq_fn) can serialize a channel's *mutable* state to a flat POD
   * and resume it bit-exactly on any thread/process/pod.  The blob holds only
   * what evolves per sample (here: the phase accumulator) — config (phase_inc,
   * norm_freq) is rebuilt from the descriptor.  Layout is the standard
   * envelope: [dp_state_hdr_t][uint32 phase]. */
#define LO_STATE_MAGIC DP_FOURCC ('L', 'O', '_', '_')
#define LO_STATE_VERSION 1u

  size_t lo_state_bytes (const lo_state_t *state);
  void lo_get_state (const lo_state_t *state, void *blob);
  int lo_set_state (lo_state_t *state, const void *blob);

  uint32_t lo_get_phase_inc (const lo_state_t *state);

  /* ---- Block generators ---- */

  size_t lo_steps_max_out (lo_state_t *state);

  size_t lo_steps (lo_state_t *state, size_t n, float complex *out);

  size_t lo_steps_ctrl_max_out (lo_state_t *state);

  size_t lo_steps_ctrl (lo_state_t *state, const float *ctrl, size_t ctrl_len,
                        float complex *out);

#ifdef __cplusplus
}
#endif

#endif /* LO_CORE_H */
```


