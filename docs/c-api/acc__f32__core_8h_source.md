

# File acc\_f32\_core.h

[**File List**](files.md) **>** [**acc\_f32**](dir_c19b9e056cbdf00f39bf52805b44beb0.md) **>** [**acc\_f32\_core.h**](acc__f32__core_8h.md)

[Go to the documentation of this file](acc__f32__core_8h.md)


```C++

#ifndef DP_ACC_F32_CORE_H
#define DP_ACC_F32_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/dp_state.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    float acc;
  } dp_acc_f32_state_t;

  dp_acc_f32_state_t *dp_acc_f32_create (float acc);

  void dp_acc_f32_destroy (dp_acc_f32_state_t *state);

  void dp_acc_f32_reset (dp_acc_f32_state_t *state);

  JM_FORCEINLINE JM_HOT void
  dp_acc_f32_step (dp_acc_f32_state_t *state, float x)
  {
    state->acc += x;
  }

  void dp_acc_f32_steps (dp_acc_f32_state_t *state, const float *input, size_t n);

  float dp_acc_f32_get_acc (const dp_acc_f32_state_t *state);

  void dp_acc_f32_set_acc (dp_acc_f32_state_t *state, float value);

  float dp_acc_f32_get (dp_acc_f32_state_t *state);

  float dp_acc_f32_dump (dp_acc_f32_state_t *state);

  void dp_acc_f32_madd (dp_acc_f32_state_t *state, const float *x, size_t x_len,
                     const float *h, size_t h_len);

  void dp_acc_f32_add2d (dp_acc_f32_state_t *state, const float *x, size_t x_len);

  void dp_acc_f32_madd2d (dp_acc_f32_state_t *state, const float *x, size_t x_len,
                       const float *h, size_t h_len);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
   * Whole-struct POD snapshot (pointer-free); the running accumulator resumes exactly into an
   * identically-built instance. */
#define ACC_F32_STATE_MAGIC DP_FOURCC ('A', 'C', 'C', 'F')
#define ACC_F32_STATE_VERSION 1u
  size_t dp_acc_f32_state_bytes (const dp_acc_f32_state_t *state);
  void    dp_acc_f32_get_state (const dp_acc_f32_state_t *state, void *blob);
  int     dp_acc_f32_set_state (dp_acc_f32_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ACC_F32_CORE_H */
```


