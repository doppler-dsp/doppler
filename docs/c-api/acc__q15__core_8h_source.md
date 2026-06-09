

# File acc\_q15\_core.h

[**File List**](files.md) **>** [**acc\_q15**](dir_df770d8a485da99b359af14931eaacf8.md) **>** [**acc\_q15\_core.h**](acc__q15__core_8h.md)

[Go to the documentation of this file](acc__q15__core_8h.md)


```C++

#ifndef ACC_Q15_CORE_H
#define ACC_Q15_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int64_t acc;
} acc_q15_state_t;

acc_q15_state_t *acc_q15_create(int64_t acc);

void acc_q15_destroy(acc_q15_state_t *state);

void acc_q15_reset(acc_q15_state_t *state);

JM_FORCEINLINE JM_HOT void
acc_q15_step(acc_q15_state_t *state, int16_t x)
{
    state->acc += (int64_t)x;
}

void acc_q15_steps(
    acc_q15_state_t *state,
    const int16_t    *input,
    size_t               n);

int64_t acc_q15_get_acc(const acc_q15_state_t *state);

void acc_q15_set_acc(acc_q15_state_t *state, int64_t val);



int64_t acc_q15_get(acc_q15_state_t *state);

int64_t acc_q15_dump(acc_q15_state_t *state);

void acc_q15_madd(acc_q15_state_t *state, const int16_t *a, size_t a_len, const int16_t *b, size_t b_len);
#ifdef __cplusplus
}
#endif

#endif /* ACC_Q15_CORE_H */
```


