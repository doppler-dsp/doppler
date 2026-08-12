/*
 * ema_alpha_decim.c — util module-level function.
 *
 * The body lives ONCE, as the JM_FORCEINLINE definition in util_core.h;
 * this translation unit emits the single out-of-line copy via the C99
 * `extern inline` idiom. See saturate.c for the full rationale.
 */
#include "util/util_core.h"

extern double ema_alpha_decim (double alpha, size_t d);
