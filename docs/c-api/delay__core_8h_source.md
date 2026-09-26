

# File delay\_core.h

[**File List**](files.md) **>** [**delay**](dir_e9520af345bba2408e131802acc7e37b.md) **>** [**delay\_core.h**](delay__core_8h.md)

[Go to the documentation of this file](delay__core_8h.md)


```C++

#ifndef DP_DELAY_CORE_H
#define DP_DELAY_CORE_H

#include "doppler/clib_common.h"
#include "doppler/jm_perf.h"
#include "doppler/dp_state.h"
#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct
  {
    double _Complex *buf; /* 2*capacity elements; second half mirrors first */
    size_t head;          /* write pointer; decrements mod capacity */
    size_t mask;          /* capacity - 1 (power-of-two bitmask) */
    size_t num_taps;      /* window length requested at construction */
    size_t capacity;      /* smallest power-of-two >= num_taps */
  } dp_delay_state_t;

dp_delay_state_t *dp_delay_create(size_t num_taps);

void dp_delay_destroy(dp_delay_state_t *state);

void dp_delay_reset(dp_delay_state_t *state);

void dp_delay_push(dp_delay_state_t *state, double _Complex x);

size_t dp_delay_ptr_max_out(dp_delay_state_t *state, size_t n);

size_t dp_delay_ptr(dp_delay_state_t *state, size_t n, double _Complex *out, size_t max_out);

size_t dp_delay_push_ptr_max_out(dp_delay_state_t *state);

size_t dp_delay_push_ptr(dp_delay_state_t *state, double _Complex x,
                      double _Complex *out, size_t max_out);

void dp_delay_write(dp_delay_state_t *state, double _Complex x);

/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Field-wise: pack running ring buffer + head; capacity/mask/num_taps restored by create. */
#define DELAY_STATE_MAGIC DP_FOURCC ('D','L','A','Y')
#define DELAY_STATE_VERSION 1u
size_t dp_delay_state_bytes (const dp_delay_state_t *state);
void dp_delay_get_state (const dp_delay_state_t *state, void *blob);
int dp_delay_set_state (dp_delay_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DELAY_CORE_H */
```


