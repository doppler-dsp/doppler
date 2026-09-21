

# File i16\_buffer\_core.h

[**File List**](files.md) **>** [**i16\_buffer**](dir_214119e05624f58881fdbfa30e65f3ff.md) **>** [**i16\_buffer\_core.h**](i16__buffer__core_8h.md)

[Go to the documentation of this file](i16__buffer__core_8h.md)


```C++

#ifndef I16_BUFFER_CORE_H
#define I16_BUFFER_CORE_H

#include "clib_common.h"

#include "buffer/buffer.h"
#include "dp_interrupt_guard/dp_interrupt_guard_core.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct
{
  int16_t i; 
  int16_t q; 
} dp_iq16_t;

typedef dp_i16_t i16_buffer_state_t;

static inline dp_i16_t *dp_i16_create (size_t capacity);

static inline bool dp_i16_write_view (dp_i16_t *state, const dp_iq16_t *x,
                                      size_t x_len);

static inline size_t dp_i16_write_some_view (dp_i16_t *state, const dp_iq16_t *x,
                                             size_t x_len);

static inline dp_iq16_t *dp_i16_wait_view (dp_i16_t *state, size_t n);

static inline dp_iq16_t *dp_i16_peek_view (dp_i16_t *state, size_t n);

static inline void dp_i16_consume (dp_i16_t *state, size_t n);

static inline void dp_i16_close (dp_i16_t *state);

static inline void dp_i16_reset (dp_i16_t *state);

static inline void dp_i16_destroy (dp_i16_t *state);

static inline size_t
i16_buffer_get_capacity (const i16_buffer_state_t *state)
{
  return state->capacity;
}

static inline size_t
i16_buffer_get_available (const i16_buffer_state_t *state)
{
  return dp_i16_available (state);
}

static inline size_t
i16_buffer_get_space (const i16_buffer_state_t *state)
{
  return dp_i16_space (state);
}

static inline size_t
i16_buffer_get_dropped (const i16_buffer_state_t *state)
{
  return state->dropped;
}

static inline bool
i16_buffer_get_closed (const i16_buffer_state_t *state)
{
  return dp_i16_closed (state);
}

DECLARE_DP_BUFFER_VIEW (i16, int16_t, dp_iq16_t)

#ifdef __cplusplus
}
#endif

#endif /* I16_BUFFER_CORE_H */
```


