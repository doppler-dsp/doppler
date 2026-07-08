

# File delay\_core.h

[**File List**](files.md) **>** [**delay**](dir_01f4b6965a2181d172634d6670b32dc1.md) **>** [**delay\_core.h**](delay__core_8h.md)

[Go to the documentation of this file](delay__core_8h.md)


```C++

#ifndef DELAY_CORE_H
#define DELAY_CORE_H

#include "clib_common.h"
#include "jm_perf.h"
#include "dp_state.h"
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
  } delay_state_t;

delay_state_t *delay_create(size_t num_taps);

void delay_destroy(delay_state_t *state);

void delay_reset(delay_state_t *state);

void delay_push(delay_state_t *state, double complex x);

size_t delay_ptr_max_out(delay_state_t *state);

size_t delay_ptr(delay_state_t *state, size_t n, double complex *out);

size_t delay_push_ptr_max_out(delay_state_t *state);

  size_t delay_push_ptr (delay_state_t *state, double complex x,
                         double complex *out);

void delay_write(delay_state_t *state, double complex x);

size_t delay_push_ptr(delay_state_t *state, double complex x, double complex *out);
/* ── Serializable state (standard bytes interface; see dp_state.h) ──────────
 * Field-wise: pack running ring buffer + head; capacity/mask/num_taps restored by create. */
#define DELAY_STATE_MAGIC DP_FOURCC ('D','L','A','Y')
#define DELAY_STATE_VERSION 1u
size_t delay_state_bytes (const delay_state_t *state);
void delay_get_state (const delay_state_t *state, void *blob);
int delay_set_state (delay_state_t *state, const void *blob);

#ifdef __cplusplus
}
#endif

#endif /* DELAY_CORE_H */
```


