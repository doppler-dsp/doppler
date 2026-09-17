#ifndef DP_JM_SHIM_H
#define DP_JM_SHIM_H
#include "buffer/buffer.h"

/* What this proposal asks doppler to add to buffer.h.
 *
 * The ring stores SCALARS (`type *data`) while a jm borrow declares the
 * ELEMENT the Python view has. So every instance -- f32, f64 and i16 alike --
 * needs the same pair; i16 is not special, it is only the one whose element
 * type is unfamiliar. Both are pure casts: `n` is already in complex samples,
 * because wait() returns &data[(t & mask) * 2].
 *
 * That "all three, same reason" is why this is a MACRO beside
 * DECLARE_DP_BUFFER rather than three hand-written pairs. Three copies of a
 * cast is three places for the element type, the count convention or the
 * constness to drift, and drift between instances of one ring is the defect
 * this whole proposal exists to end -- doppler#1346 is exactly that, caught
 * late because nothing compared the three. One declaration per instance,
 * stamped from one body.
 *
 * The suffix is `_view`, not `_cf`: the f32 and f64 elements are complex
 * float and complex double, but the i16 element is a two-field RECORD, so a
 * name saying "complex float" would be wrong on the instance that needs it
 * most. `_view` is what all three actually are. */

/* -------------------------------------------------------------------------
 * Compat: compile-time "two stored scalars to the element" assert
 *
 * Guarded exactly like buffer.h's own DP_ASSERT_PWR2 above it, because
 * doppler is CMAKE_C_STANDARD 99 -- so the typedef fallback, not
 * _Static_assert, is the branch that actually compiles in the real build.
 * ---------------------------------------------------------------------- */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define DP_ASSERT_2X(tag, elem, type)                                         \
  _Static_assert (sizeof (elem) == 2 * sizeof (type),                         \
                  "view element must span two stored scalars")
#else
#define DP_ASSERT_2X(tag, elem, type)                                         \
  typedef char dp_assert_2x_##tag[sizeof (elem) == 2 * sizeof (type) ? 1 : -1]
#endif

/**
 * @def DECLARE_DP_BUFFER_VIEW(name, type, elem)
 * @brief Declares the element-typed face of the @p name ring.
 *
 * `dp_<name>_wait_view()` and `dp_<name>_write_view()` are the same buffer
 * addressed one element per SAMPLE instead of one per stored scalar. Each is
 * a cast and nothing else -- the count is already in samples -- so the view
 * stays zero-copy and no length arithmetic is introduced that could disagree
 * with the scalar face.
 *
 * Siblings rather than changes to `dp_<name>_wait()`, because the scalar face
 * is what the C tests address and what `dp_<name>_write()` mirrors. Both
 * faces over one buffer, neither reimplementing the other.
 *
 * @param name  Ring instance suffix, as passed to #DECLARE_DP_BUFFER.
 * @param type  Stored scalar type (`float`, `int16_t`, ...).
 * @param elem  Element type spanning two scalars.
 *
 * @code
 * dp_f32_t *ab = dp_f32_create (1024);
 * float _Complex *v = dp_f32_wait_view (ab, 4);   // 4 samples, not 8 floats
 * dp_f32_consume (ab, 4);
 * dp_f32_destroy (ab);
 * @endcode
 */
#define DECLARE_DP_BUFFER_VIEW(name, type, elem)                              \
                                                                              \
  /* Two scalars to the element, or both casts read the wrong span. */        \
  DP_ASSERT_2X (name, elem, type);                                            \
                                                                              \
  static inline elem *dp_##name##_wait_view (dp_##name##_t *ab, size_t n)     \
  {                                                                           \
    return (elem *)dp_##name##_wait (ab, n);                                  \
  }                                                                           \
                                                                              \
  static inline bool dp_##name##_write_view (dp_##name##_t *ab,               \
                                             const elem *src, size_t n)       \
  {                                                                           \
    return dp_##name##_write (ab, (const type *)src, n);                      \
  }

/** @brief One q15 IQ sample: the element the i16 ring's view hands back. */
typedef struct
{
  int16_t i; /**< In-phase component. */
  int16_t q; /**< Quadrature component. */
} dp_iq16_t;

DECLARE_DP_BUFFER_VIEW (f32, float, float _Complex)
DECLARE_DP_BUFFER_VIEW (f64, double, double _Complex)
DECLARE_DP_BUFFER_VIEW (i16, int16_t, dp_iq16_t)

/* --- what jm's binding names, over the ring it actually is --------------- */

typedef dp_f32_t f32_buffer_state_t;

#endif
