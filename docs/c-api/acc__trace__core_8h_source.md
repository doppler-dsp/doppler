

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

int dp_acc_trace_set_alpha(dp_acc_trace_state_t *state, double alpha);

void dp_acc_trace_destroy(dp_acc_trace_state_t *state);

void dp_acc_trace_reset(dp_acc_trace_state_t *state);

void dp_acc_trace_accumulate(dp_acc_trace_state_t *state, const float *p,
                          size_t p_len);

size_t dp_acc_trace_value_max_out(dp_acc_trace_state_t *state);

size_t dp_acc_trace_value(dp_acc_trace_state_t *state, size_t n, float *out,
                       size_t max_out);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Field-wise: [u32 mode][u64 count][f64 alpha][running trace]; n is restored
 * by create.  mode is a reject key: a blob from a trace in another mode is
 * refused.  alpha travels because dp_acc_trace_set_alpha() can change it after
 * create (version 2; version 1 blobs, without mode or alpha, are refused).
 * Version 3: a mean trace holds the per-bin sum, not the mean, so version 2
 * blobs are refused too, and so is any blob that nests one (PSD's, and
 * through it Specan's, CarrierAcquisition's and the AsyncDsss receivers').
 * set_state refuses an alpha the setter would refuse, and a count of 0 with
 * a trace that is not all +0.0 bits (which reset() would have left); any
 * refusal leaves the state untouched.  What accumulate can reach is never
 * refused: a non-finite frame makes a non-finite trace, which a blob
 * carries and restores. */
#define ACC_TRACE_STATE_MAGIC DP_FOURCC ('A','T','R','C')
#define ACC_TRACE_STATE_VERSION 3u
size_t dp_acc_trace_state_bytes (const dp_acc_trace_state_t *state);
void dp_acc_trace_get_state (const dp_acc_trace_state_t *state, void *blob);
int dp_acc_trace_set_state (dp_acc_trace_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* ACC_TRACE_CORE_H */
```


