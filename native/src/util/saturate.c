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
 * Every function in util has one body, emitted this way. A hand-copied
 * second body would make the function every C caller inlines and the
 * function the Python binding calls two pieces of source kept in agreement
 * by hand, and a divergence would be silent: the C tests exercise the inline
 * and the binding calls the copy. This file is the shape to follow.
 */
#include "doppler/util/util_core.h"

extern double dp_saturate (double v, double lo, double hi, double nan_to);
