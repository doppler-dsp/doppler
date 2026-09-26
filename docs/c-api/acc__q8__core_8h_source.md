

# File acc\_q8\_core.h

[**File List**](files.md) **>** [**acc\_q8**](dir_ea0f50795da7b2248dba00cbaac7e694.md) **>** [**acc\_q8\_core.h**](acc__q8__core_8h.md)

[Go to the documentation of this file](acc__q8__core_8h.md)


```C++

#ifndef DP_ACC_Q8_CORE_H
#define DP_ACC_Q8_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/dp_state.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t acc;
} dp_acc_q8_state_t;

dp_acc_q8_state_t *dp_acc_q8_create(int32_t acc);

void dp_acc_q8_destroy(dp_acc_q8_state_t *state);

void dp_acc_q8_reset(dp_acc_q8_state_t *state);

JM_FORCEINLINE JM_HOT void
dp_acc_q8_step(dp_acc_q8_state_t *state, int8_t x)
{
    state->acc += (int32_t)x;
}

void dp_acc_q8_steps(
    dp_acc_q8_state_t *state,
    const int8_t    *input,
    size_t               n);

int32_t dp_acc_q8_get_acc(const dp_acc_q8_state_t *state);

void dp_acc_q8_set_acc(dp_acc_q8_state_t *state, int32_t val);



int32_t dp_acc_q8_get(dp_acc_q8_state_t *state);

int32_t dp_acc_q8_dump(dp_acc_q8_state_t *state);

void dp_acc_q8_madd(dp_acc_q8_state_t *state, const int8_t *a, size_t a_len, const int8_t *b, size_t b_len);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the running 32-bit accumulator resumes exactly into an
 * identically-built instance. */
#define ACC_Q8_STATE_MAGIC DP_FOURCC ('A', 'C', 'C', '8')
#define ACC_Q8_STATE_VERSION 1u
size_t dp_acc_q8_state_bytes (const dp_acc_q8_state_t *state);
void   dp_acc_q8_get_state (const dp_acc_q8_state_t *state, void *blob);
int    dp_acc_q8_set_state (dp_acc_q8_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ACC_Q8_CORE_H */
```


