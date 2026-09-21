

# File f32\_buffer\_core.h

[**File List**](files.md) **>** [**f32\_buffer**](dir_73bc8939a0d066ce4b56550e20e88de7.md) **>** [**f32\_buffer\_core.h**](f32__buffer__core_8h.md)

[Go to the documentation of this file](f32__buffer__core_8h.md)


```C++

#ifndef F32_BUFFER_CORE_H
#define F32_BUFFER_CORE_H

#include "clib_common.h"

#include "buffer/buffer.h"
#include "dp_interrupt_guard/dp_interrupt_guard_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef dp_f32_t f32_buffer_state_t;

static inline dp_f32_t *dp_f32_create (size_t capacity);

static inline bool dp_f32_write_view (dp_f32_t *state, const float _Complex *x,
                                      size_t x_len);

static inline size_t dp_f32_write_some_view (dp_f32_t *state, const float _Complex *x,
                                             size_t x_len);

static inline float _Complex *dp_f32_wait_view (dp_f32_t *state, size_t n);

static inline float _Complex *dp_f32_peek_view (dp_f32_t *state, size_t n);

static inline void dp_f32_consume (dp_f32_t *state, size_t n);

static inline void dp_f32_close (dp_f32_t *state);

static inline void dp_f32_reset (dp_f32_t *state);

static inline void dp_f32_destroy (dp_f32_t *state);

static inline size_t
f32_buffer_get_capacity (const f32_buffer_state_t *state)
{
  return state->capacity;
}

static inline size_t
f32_buffer_get_available (const f32_buffer_state_t *state)
{
  return dp_f32_available (state);
}

static inline size_t
f32_buffer_get_space (const f32_buffer_state_t *state)
{
  return dp_f32_space (state);
}

static inline size_t
f32_buffer_get_dropped (const f32_buffer_state_t *state)
{
  return state->dropped;
}

static inline bool
f32_buffer_get_closed (const f32_buffer_state_t *state)
{
  return dp_f32_closed (state);
}

DECLARE_DP_BUFFER_VIEW (f32, float, float _Complex)

#ifdef __cplusplus
}
#endif

#endif /* F32_BUFFER_CORE_H */
```


