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

/* And the destroy half of the pair.
 *
 * `create_fn` names the C jm CALLS to construct, but an object has no
 * `destroy_fn` -- it is a capsule-module key -- so jm emits `<comp>_destroy`
 * for the dealloc path regardless. Naming `dp_f32_create` as the creator
 * therefore pairs it with a destroyer nothing defines, and if one did exist
 * under that name it would be jm's scaffolded `free(state)`, which never
 * unmaps the mirrored region.
 *
 * An earlier probe hid this behind `#define f32_buffer_destroy
 * dp_f32_destroy`. A `#define` in the harness is not the proposal: it makes
 * the compile pass while leaving the declaration asymmetric. Forwarding it
 * here keeps create and destroy symmetric by construction, which is the
 * property that has to hold at adoption. Filed upstream. */
static inline void
f32_buffer_destroy (dp_f32_t *ab)
{
  dp_f32_destroy (ab);
}
#endif
