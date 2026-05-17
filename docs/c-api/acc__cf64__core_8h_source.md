

# File acc\_cf64\_core.h

[**File List**](files.md) **>** [**acc\_cf64**](dir_a31d3897e2036bab462df07bf5a3b557.md) **>** [**acc\_cf64\_core.h**](acc__cf64__core_8h.md)

[Go to the documentation of this file](acc__cf64__core_8h.md)


```C++

#ifndef ACC_CF64_CORE_H
#define ACC_CF64_CORE_H

#include "clib_common.h"
#include "jm_perf.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    double _Complex acc;
  } acc_cf64_state_t;

  acc_cf64_state_t *acc_cf64_create (double _Complex acc);

  void acc_cf64_destroy (acc_cf64_state_t *state);

  void acc_cf64_reset (acc_cf64_state_t *state);

  JM_FORCEINLINE JM_HOT void
  acc_cf64_step (acc_cf64_state_t *state, double complex x)
  {
    state->acc += x;
  }

  void acc_cf64_steps (acc_cf64_state_t *state, const double complex *input,
                       size_t n);

  double _Complex acc_cf64_get_acc (const acc_cf64_state_t *state);

  void acc_cf64_set_acc (acc_cf64_state_t *state, double _Complex acc);

  double complex acc_cf64_get (acc_cf64_state_t *state);

  double complex acc_cf64_dump (acc_cf64_state_t *state);

  void acc_cf64_madd (acc_cf64_state_t *state, const double complex *x,
                      size_t x_len, const float *h, size_t h_len);

  void acc_cf64_add2d (acc_cf64_state_t *state, const double complex *x,
                       size_t x_len);

  void acc_cf64_madd2d (acc_cf64_state_t *state, const double complex *x,
                        size_t x_len, const float *h, size_t h_len);

#ifdef __cplusplus
}
#endif

#endif /* ACC_CF64_CORE_H */
```


