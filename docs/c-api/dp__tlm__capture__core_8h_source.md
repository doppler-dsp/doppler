

# File dp\_tlm\_capture\_core.h

[**File List**](files.md) **>** [**dp\_tlm\_capture**](dir_c53721efa35f9e05ec164f1aacd6bf30.md) **>** [**dp\_tlm\_capture\_core.h**](dp__tlm__capture__core_8h.md)

[Go to the documentation of this file](dp__tlm__capture__core_8h.md)


```C++

#ifndef DP_TLM_CAPTURE_H
#define DP_TLM_CAPTURE_H

#include "dp_tlm/dp_tlm_core.h"
#include "timing/timing_core.h" /* dp_sample_clock_t — the ONE time base */

#ifdef __cplusplus
extern "C" {
#endif

/* dp_tlm_capture_t and dp_tlm_capture_block() are declared in telemetry.h so
 * the inline dp_tlm_set_now() can delegate; the dependency runs one way. */

dp_tlm_capture_t *dp_tlm_capture_open (dp_tlm_t *t, size_t block_samples,
                                       const char             *path,
                                       const dp_sample_clock_t *clock);

dp_tlm_capture_t *dp_tlm_capture_open_memory (dp_tlm_t *t,
                                              size_t     block_samples,
                                              const dp_sample_clock_t *clock);

int dp_tlm_capture_block (dp_tlm_capture_t *c);

int dp_tlm_capture_close (dp_tlm_capture_t *c);

size_t dp_tlm_capture_count (const dp_tlm_capture_t *c);

const dp_tlm_rec_t *dp_tlm_capture_records (const dp_tlm_capture_t *c);

size_t dp_tlm_capture_read_max_out (const dp_tlm_capture_t *c);

size_t dp_tlm_capture_read (const dp_tlm_capture_t *c, size_t n,
                            dp_tlm_rec_t *out, size_t max_out);

uint64_t dp_tlm_capture_dropped (const dp_tlm_capture_t *c);

int dp_tlm_capture_destroy (dp_tlm_capture_t *c);

typedef dp_tlm_capture_t dp_tlm_capture_state_t;

#ifdef __cplusplus
}
#endif

#endif /* DP_TLM_CAPTURE_H */
```


