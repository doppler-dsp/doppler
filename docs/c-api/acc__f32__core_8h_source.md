

# File acc\_f32\_core.h

[**File List**](files.md) **>** [**acc\_f32**](dir_0465294bf3f41af7dbdebf91d81a0c4a.md) **>** [**acc\_f32\_core.h**](acc__f32__core_8h.md)

[Go to the documentation of this file](acc__f32__core_8h.md)


```C++

#ifndef ACC_F32_CORE_H
#define ACC_F32_CORE_H

#include "clib_common.h"
#include "jm_perf.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    float acc;
  } acc_f32_state_t;

  acc_f32_state_t *acc_f32_create (float acc);

  void acc_f32_destroy (acc_f32_state_t *state);

  void acc_f32_reset (acc_f32_state_t *state);

  JM_FORCEINLINE JM_HOT void
  acc_f32_step (acc_f32_state_t *state, float x)
  {
    state->acc += x;
  }

  void acc_f32_steps (acc_f32_state_t *state, const float *input, size_t n);

  float acc_f32_get_acc (const acc_f32_state_t *state);

  void acc_f32_set_acc (acc_f32_state_t *state, float acc);

  float acc_f32_get (acc_f32_state_t *state);

  float acc_f32_dump (acc_f32_state_t *state);

  void acc_f32_madd (acc_f32_state_t *state, const float *x, size_t x_len,
                     const float *h, size_t h_len);

  void acc_f32_add2d (acc_f32_state_t *state, const float *x, size_t x_len);

  void acc_f32_madd2d (acc_f32_state_t *state, const float *x, size_t x_len,
                       const float *h, size_t h_len);

#ifdef __cplusplus
}
#endif

#endif /* ACC_F32_CORE_H */
```


