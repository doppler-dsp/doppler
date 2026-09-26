

# File acc\_trace\_core.h

[**File List**](files.md) **>** [**acc\_trace**](dir_ea4259ba3dd1c044f0efb519286a18a5.md) **>** [**acc\_trace\_core.h**](acc__trace__core_8h.md)

[Go to the documentation of this file](acc__trace__core_8h.md)


```C++

#ifndef DP_ACC_TRACE_CORE_H
#define DP_ACC_TRACE_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/dp_state.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ACC_TRACE_MEAN = 0,    
    ACC_TRACE_EXP = 1,     
    ACC_TRACE_MAXHOLD = 2, 
    ACC_TRACE_MINHOLD = 3, 
} acc_trace_mode_t;

typedef struct {
    double *acc;            
    size_t n;               
    acc_trace_mode_t mode;  
    double alpha;           
    uint64_t count;         
} dp_acc_trace_state_t;

dp_acc_trace_state_t *dp_acc_trace_create(size_t n, int mode, double alpha);

void dp_acc_trace_destroy(dp_acc_trace_state_t *state);

void dp_acc_trace_reset(dp_acc_trace_state_t *state);

void dp_acc_trace_accumulate(dp_acc_trace_state_t *state, const float *p,
                          size_t p_len);

size_t dp_acc_trace_value_max_out(dp_acc_trace_state_t *state);

size_t dp_acc_trace_value(dp_acc_trace_state_t *state, size_t n, float *out,
                       size_t max_out);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Field-wise: pack running trace + fold count; n/mode/alpha restored by create. */
#define ACC_TRACE_STATE_MAGIC DP_FOURCC ('A','T','R','C')
#define ACC_TRACE_STATE_VERSION 1u
size_t dp_acc_trace_state_bytes (const dp_acc_trace_state_t *state);
void dp_acc_trace_get_state (const dp_acc_trace_state_t *state, void *blob);
int dp_acc_trace_set_state (dp_acc_trace_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ACC_TRACE_CORE_H */
```


