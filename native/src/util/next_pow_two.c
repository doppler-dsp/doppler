/*
 * next_pow_two.c — util module-level function.
 *
 * The body lives ONCE, as the JM_FORCEINLINE definition in util_core.h;
 * this `extern` declaration is the C99 idiom asking this translation unit —
 * and only this one — to emit the out-of-line copy the Python binding and
 * any library-reaching caller need. See saturate.c for the full note on why
 * this shape rather than square_clip.c's hand-copied second body.
 */
#include "util/util_core.h"

extern size_t next_pow_two (size_t n);
