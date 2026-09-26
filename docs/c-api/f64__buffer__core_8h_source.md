

# File f64\_buffer\_core.h

[**File List**](files.md) **>** [**doppler**](dir_c8eead50fa73fbaea26b38d49c33a8a7.md) **>** [**f64\_buffer**](dir_d7320aa8439c659edb0fbbfd27af0c55.md) **>** [**f64\_buffer\_core.h**](f64__buffer__core_8h.md)

[Go to the documentation of this file](f64__buffer__core_8h.md)


```C++

#ifndef DP_F64_BUFFER_CORE_H
#define DP_F64_BUFFER_CORE_H

#include "doppler/clib_common.h"

#include "doppler/buffer/buffer.h"
#include "doppler/dp_interrupt_guard/dp_interrupt_guard_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef dp_f64_t dp_f64_buffer_state_t;

static inline dp_f64_t *dp_f64_create (size_t capacity);

static inline bool
dp_f64_write_view (dp_f64_t *state, const double _Complex *x, size_t x_len);

static inline size_t
dp_f64_write_some_view (dp_f64_t *state, const double _Complex *x,
                        size_t x_len);

static inline double _Complex *dp_f64_wait_view (dp_f64_t *state, size_t n);

static inline double _Complex *dp_f64_peek_view (dp_f64_t *state, size_t n);

static inline int dp_f64_consume (dp_f64_t *state, size_t n);

static inline void dp_f64_close (dp_f64_t *state);

static inline void dp_f64_reset (dp_f64_t *state);

static inline void dp_f64_destroy (dp_f64_t *state);

static inline size_t
dp_f64_buffer_get_capacity (const dp_f64_buffer_state_t *state)
{
  return state->capacity;
}

static inline size_t
dp_f64_buffer_get_available (const dp_f64_buffer_state_t *state)
{
  return dp_f64_available (state);
}

static inline size_t
dp_f64_buffer_get_space (const dp_f64_buffer_state_t *state)
{
  return dp_f64_space (state);
}

static inline size_t
dp_f64_buffer_get_dropped (const dp_f64_buffer_state_t *state)
{
  return state->dropped;
}

static inline bool
dp_f64_buffer_get_closed (const dp_f64_buffer_state_t *state)
{
  return dp_f64_closed (state);
}

DECLARE_DP_BUFFER_VIEW (f64, double, double _Complex)

#ifdef __cplusplus
}
#endif

#endif /* F64_BUFFER_CORE_H */
```


