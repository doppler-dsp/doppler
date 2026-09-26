

# File acc\_q15\_core.h

[**File List**](files.md) **>** [**acc\_q15**](dir_2344cd9a4aadb833503124e5257faf5b.md) **>** [**acc\_q15\_core.h**](acc__q15__core_8h.md)

[Go to the documentation of this file](acc__q15__core_8h.md)


```C++

#ifndef DP_ACC_Q15_CORE_H
#define DP_ACC_Q15_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/dp_state.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int64_t acc;
} dp_acc_q15_state_t;

dp_acc_q15_state_t *dp_acc_q15_create(int64_t acc);

void dp_acc_q15_destroy(dp_acc_q15_state_t *state);

void dp_acc_q15_reset(dp_acc_q15_state_t *state);

JM_FORCEINLINE JM_HOT void
dp_acc_q15_step(dp_acc_q15_state_t *state, int16_t x)
{
    state->acc += (int64_t)x;
}

void dp_acc_q15_steps(
    dp_acc_q15_state_t *state,
    const int16_t    *input,
    size_t               n);

int64_t dp_acc_q15_get_acc(const dp_acc_q15_state_t *state);

void dp_acc_q15_set_acc(dp_acc_q15_state_t *state, int64_t val);



int64_t dp_acc_q15_get(dp_acc_q15_state_t *state);

int64_t dp_acc_q15_dump(dp_acc_q15_state_t *state);

void dp_acc_q15_madd(dp_acc_q15_state_t *state, const int16_t *a, size_t a_len, const int16_t *b, size_t b_len);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Whole-struct POD snapshot (pointer-free); the running 64-bit accumulator resumes exactly into an
 * identically-built instance. */
#define ACC_Q15_STATE_MAGIC DP_FOURCC ('A', 'C', '1', '5')
#define ACC_Q15_STATE_VERSION 1u
size_t dp_acc_q15_state_bytes (const dp_acc_q15_state_t *state);
void   dp_acc_q15_get_state (const dp_acc_q15_state_t *state, void *blob);
int    dp_acc_q15_set_state (dp_acc_q15_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ACC_Q15_CORE_H */
```


