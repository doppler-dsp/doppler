/*
 * square_clip.c — util module-level function.
 *
 * The body lives ONCE, as the JM_FORCEINLINE definition in util_core.h.
 * JM_FORCEINLINE expands to a plain `inline` (not `static inline`), so the
 * `extern` declaration below is the C99 idiom that asks this translation
 * unit — and only this one — to emit an out-of-line copy of that same
 * definition: for the Python binding, and for a C++ caller, to which
 * util_core.h gives only the prototype (doppler#3108). The shape is
 * saturate.c's; see there for why a second body is not written by hand.
 */
#include "doppler/util/util_core.h"

extern float _Complex dp_square_clip (float _Complex y, float lin);
