/*
 * saturate.c — util module-level function.
 *
 * The body lives ONCE, as the JM_FORCEINLINE definition in util_core.h.
 * JM_FORCEINLINE expands to a plain `inline` (not `static inline`), so the
 * `extern` declaration below is the C99 idiom that asks this translation
 * unit — and only this one — to emit an out-of-line copy of that same
 * definition, for the Python binding and for any caller that reaches the
 * function through the library rather than the header.
 *
 * square_clip.c, next door, predates this and instead carries a hand-copied
 * second body. That works, but it means the function every C caller inlines
 * and the function the Python binding calls are two separate pieces of
 * source that must be kept in agreement by hand — and a divergence would be
 * silent, since the C tests exercise the inline and only Python would see
 * the stale copy. Do not extend that pattern; this file is the shape to
 * follow.
 */
#include "util/util_core.h"

extern double saturate (double v, double lo, double hi, double nan_to);
