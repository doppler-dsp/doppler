

# File acc\_trace\_core.h

[**File List**](files.md) **>** [**acc\_trace**](dir_51e33d48c4bde6f60a2f27e75677a784.md) **>** [**acc\_trace\_core.h**](acc__trace__core_8h.md)

[Go to the documentation of this file](acc__trace__core_8h.md)


```C++

#ifndef ACC_TRACE_CORE_H
#define ACC_TRACE_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
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
} acc_trace_state_t;

acc_trace_state_t *acc_trace_create(size_t n, int mode, double alpha);

void acc_trace_destroy(acc_trace_state_t *state);

void acc_trace_reset(acc_trace_state_t *state);

void acc_trace_accumulate(acc_trace_state_t *state, const float *p,
                          size_t p_len);

size_t acc_trace_value_max_out(acc_trace_state_t *state);

size_t acc_trace_value(acc_trace_state_t *state, size_t n, float *out);
#ifdef __cplusplus
}
#endif

#endif /* ACC_TRACE_CORE_H */
```


