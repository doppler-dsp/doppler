

# File acc\_cf64\_core.h

[**File List**](files.md) **>** [**acc\_cf64**](dir_d950eea1844c6f23a7b4df3cf640e9a2.md) **>** [**acc\_cf64\_core.h**](acc__cf64__core_8h.md)

[Go to the documentation of this file](acc__cf64__core_8h.md)


```C++

#ifndef DP_ACC_CF64_CORE_H
#define DP_ACC_CF64_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/dp_state.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    double _Complex acc;
  } dp_acc_cf64_state_t;

  dp_acc_cf64_state_t *dp_acc_cf64_create (double _Complex acc);

  void dp_acc_cf64_destroy (dp_acc_cf64_state_t *state);

  void dp_acc_cf64_reset (dp_acc_cf64_state_t *state);

  JM_FORCEINLINE JM_HOT void
  dp_acc_cf64_step (dp_acc_cf64_state_t *state, double _Complex x)
  {
    state->acc += x;
  }

  void dp_acc_cf64_steps (dp_acc_cf64_state_t *state, const double _Complex *input,
                       size_t n);

  double _Complex dp_acc_cf64_get_acc (const dp_acc_cf64_state_t *state);

  void dp_acc_cf64_set_acc (dp_acc_cf64_state_t *state, double _Complex value);

  double _Complex dp_acc_cf64_get (dp_acc_cf64_state_t *state);

  double _Complex dp_acc_cf64_dump (dp_acc_cf64_state_t *state);

  void dp_acc_cf64_madd (dp_acc_cf64_state_t *state, const double _Complex *x,
                      size_t x_len, const float *h, size_t h_len);

  void dp_acc_cf64_add2d (dp_acc_cf64_state_t *state, const double _Complex *x,
                       size_t x_len);

  void dp_acc_cf64_madd2d (dp_acc_cf64_state_t *state, const double _Complex *x,
                        size_t x_len, const float *h, size_t h_len);

  /* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
   * Whole-struct POD snapshot (pointer-free); the running accumulator resumes exactly into an
   * identically-built instance. */
#define ACC_CF64_STATE_MAGIC DP_FOURCC ('A', 'C', 'C', 'C')
#define ACC_CF64_STATE_VERSION 1u
  size_t dp_acc_cf64_state_bytes (const dp_acc_cf64_state_t *state);
  void    dp_acc_cf64_get_state (const dp_acc_cf64_state_t *state, void *blob);
  int     dp_acc_cf64_set_state (dp_acc_cf64_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ACC_CF64_CORE_H */
```


