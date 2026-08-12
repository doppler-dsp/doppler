/*
 * ema_step.c — util module-level function.
 *
 * The body lives ONCE, as the JM_FORCEINLINE definition in util_core.h.
 * JM_FORCEINLINE expands to a plain `inline` (not `static inline`), so the
 * `extern` declaration below is the C99 idiom that asks this translation
 * unit — and only this one — to emit an out-of-line copy of that same
 * definition, for the Python binding and for any caller that reaches the
 * function through the library rather than the header.
 *
 * The shape is saturate.c's, deliberately, and not square_clip.c's — that
 * one carries a hand-copied second body, which is precisely the duplication
 * this function exists to end. An EMA with two source copies would be the
 * joke telling itself.
 */
#include "util/util_core.h"

extern double ema_step (double state, double x, double alpha);
