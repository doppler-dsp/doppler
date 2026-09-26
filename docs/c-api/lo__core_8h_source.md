

# File lo\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**lo**](dir_939b5fbb8d3e3ebd3276389efab5bbba.md) **>** [**lo\_core.h**](lo__core_8h.md)

[Go to the documentation of this file](lo__core_8h.md)


```C++

#ifndef DP_LO_CORE_H
#define DP_LO_CORE_H

#include "doppler/clib_common.h"
#include "doppler/dp_state.h"
#include "doppler/jm_perf.h"
#include "doppler/nco/nco_core.h" /* nco_norm_freq_to_inc() -- the one shared cycles->phase-delta primitive */
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    uint32_t phase;     /* current accumulator value [0, 2^32)          */
    uint32_t phase_inc; /* advance per sample = floor(norm_freq * 2^32) */
    double norm_freq;   /* normalised frequency (cycles/sample)           */
  } dp_lo_state_t;

/* ---- Inline composition API (C-only; not exposed as Python methods) ----
 *
 * lo_init / lo_step let a tracking loop embed dp_lo_state_t BY VALUE and de-rotate
 * a sample stream one sample at a time with zero call overhead — the block
 * generators below (dp_lo_steps) stay the fast path for bulk synthesis.  The
 * shared sin LUT is exposed here so the inline step can index it directly.   */
#define LO_LUT_BITS 16u
#define LO_LUT_SIZE (1u << LO_LUT_BITS) /* 65536                    */
#define LO_LUT_QTR (LO_LUT_SIZE >> 2u)  /* 16384  (π/2 phase shift) */

  extern float lo_sin_lut[LO_LUT_SIZE];

  void lo_init (dp_lo_state_t *state, double norm_freq);

  JM_FORCEINLINE JM_HOT float _Complex lo_step (dp_lo_state_t *state)
  {
    uint16_t idx = (uint16_t)(state->phase >> (32u - LO_LUT_BITS));
    float _Complex out
        = CMPLXF (lo_sin_lut[(uint16_t)(idx + (uint16_t)LO_LUT_QTR)],
                  lo_sin_lut[idx]);
    state->phase += state->phase_inc;
    return out;
  }

  JM_FORCEINLINE JM_HOT float _Complex lo_step_ctrl (dp_lo_state_t *state,
                                                    double ctrl)
  {
    uint16_t idx = (uint16_t)(state->phase >> (32u - LO_LUT_BITS));
    float _Complex out
        = CMPLXF (lo_sin_lut[(uint16_t)(idx + (uint16_t)LO_LUT_QTR)],
                  lo_sin_lut[idx]);
    /* nco_norm_freq_to_inc() is the ONE shared cycles->phase-delta
     * primitive, and it TRUNCATES -- see nco_core.h for why rounding would
     * make the increment differ by host. This comment claimed the opposite
     * ("rounds, not truncates") from the consolidation until an audit
     * caught it; test_lo_core.c section 21 now pins truncation on this
     * path, so the prose cannot drift away from the code again. */
    state->phase += state->phase_inc + nco_norm_freq_to_inc (ctrl);
    return out;
  }

  dp_lo_state_t *dp_lo_create (double norm_freq);

  void dp_lo_destroy (dp_lo_state_t *state);

  void dp_lo_reset (dp_lo_state_t *state);

  /* ---- Properties ---- */

  double dp_lo_get_norm_freq (const dp_lo_state_t *state);
  void dp_lo_set_norm_freq (dp_lo_state_t *state, double norm_freq);

  uint32_t dp_lo_get_phase (const dp_lo_state_t *state);
  void dp_lo_set_phase (dp_lo_state_t *state, uint32_t phase);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ────────
   * Every composable filter exposes this triplet so a pure transducer
   * (ddc_fn / acq_fn) can serialize a channel's *mutable* state to a flat POD
   * and resume it bit-exactly on any thread/process/pod.  The blob holds only
   * what evolves per sample (here: the phase accumulator) — config (phase_inc,
   * norm_freq) is rebuilt from the descriptor.  Layout is the standard
   * envelope: [dp_state_hdr_t][uint32 phase]. */
#define LO_STATE_MAGIC DP_FOURCC ('L', 'O', '_', '_')
#define LO_STATE_VERSION 1u

  size_t dp_lo_state_bytes (const dp_lo_state_t *state);
  void dp_lo_get_state (const dp_lo_state_t *state, void *blob);
  int dp_lo_set_state (dp_lo_state_t *state, const void *blob);

  uint32_t dp_lo_get_phase_inc (const dp_lo_state_t *state);

  /* ---- Block generators ---- */

  size_t dp_lo_steps_max_out (dp_lo_state_t *state);

  size_t dp_lo_steps (dp_lo_state_t *state, size_t n, float _Complex *out,
                   size_t max_out);

  size_t dp_lo_steps_ctrl_max_out (dp_lo_state_t *state);

  size_t dp_lo_steps_ctrl (dp_lo_state_t *state, const double *ctrl, size_t ctrl_len,
                        float _Complex *out, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif /* LO_CORE_H */
```


