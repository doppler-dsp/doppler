

# File acc\_q8\_core.h

[**File List**](files.md) **>** [**acc\_q8**](dir_af45fd7415a1bcf5c13e14c3d63a83bf.md) **>** [**acc\_q8\_core.h**](acc__q8__core_8h.md)

[Go to the documentation of this file](acc__q8__core_8h.md)


```C++

#ifndef ACC_Q8_CORE_H
#define ACC_Q8_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t acc;
} acc_q8_state_t;

acc_q8_state_t *acc_q8_create(int32_t acc);

void acc_q8_destroy(acc_q8_state_t *state);

void acc_q8_reset(acc_q8_state_t *state);

JM_FORCEINLINE JM_HOT void
acc_q8_step(acc_q8_state_t *state, int8_t x)
{
    state->acc += (int32_t)x;
}

void acc_q8_steps(
    acc_q8_state_t *state,
    const int8_t    *input,
    size_t               n);

int32_t acc_q8_get_acc(const acc_q8_state_t *state);

void acc_q8_set_acc(acc_q8_state_t *state, int32_t val);



int32_t acc_q8_get(acc_q8_state_t *state);

int32_t acc_q8_dump(acc_q8_state_t *state);

void acc_q8_madd(acc_q8_state_t *state, const int8_t *a, size_t a_len, const int8_t *b, size_t b_len);
#ifdef __cplusplus
}
#endif

#endif /* ACC_Q8_CORE_H */
```


