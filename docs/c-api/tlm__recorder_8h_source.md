

# File tlm\_recorder.h

[**File List**](files.md) **>** [**inc**](dir_5029b6cdea6e9b25321183da44d91d43.md) **>** [**telemetry**](dir_d4543964ddc0423cd91d16ab74a4089e.md) **>** [**tlm\_recorder.h**](tlm__recorder_8h.md)

[Go to the documentation of this file](tlm__recorder_8h.md)


```C++

#ifndef DP_TLM_RECORDER_H
#define DP_TLM_RECORDER_H

#include "telemetry/telemetry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dp_tlm_recorder dp_tlm_recorder_t;

size_t dp_tlm_block_bound (const dp_tlm_t *t, size_t block_samples);

dp_tlm_recorder_t *dp_tlm_recorder_create (dp_tlm_t *t, const char *path,
                                           size_t block_samples);

int dp_tlm_recorder_tick (dp_tlm_recorder_t *r);

int dp_tlm_recorder_finish (dp_tlm_recorder_t *r);

uint64_t dp_tlm_recorder_count (const dp_tlm_recorder_t *r);

uint64_t dp_tlm_recorder_dropped (const dp_tlm_recorder_t *r);

size_t dp_tlm_recorder_ring_records (const dp_tlm_recorder_t *r);

void dp_tlm_recorder_destroy (dp_tlm_recorder_t *r);

#ifdef __cplusplus
}
#endif

#endif /* DP_TLM_RECORDER_H */
```


