#ifndef DP_JM_SHIM_H
#define DP_JM_SHIM_H
#include "buffer/buffer.h"

/* The two siblings this proposal asks doppler to add.
 *
 * The ring stores SCALARS (`type *data`) while a jm borrow declares the
 * ELEMENT the Python view has. So every instance -- f32, f64 and i16 alike --
 * needs the same pair; i16 is not special, it is only the one whose element
 * type is unfamiliar. Both are pure casts: `n` is already in complex samples,
 * because wait() returns &data[(t & mask) * 2]. */
typedef dp_f32_t f32_buffer_state_t;

static inline float _Complex *
dp_f32_wait_cf (dp_f32_t *ab, size_t n)
{
  return (float _Complex *)dp_f32_wait (ab, n);
}

static inline int
dp_f32_write_cf (dp_f32_t *ab, const float _Complex *src, size_t n)
{
  return dp_f32_write (ab, (const float *)src, n);
}
#endif
