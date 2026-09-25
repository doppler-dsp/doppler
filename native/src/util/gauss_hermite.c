/*
 * gauss_hermite.c — util module-level function.
 *
 * The body lives ONCE, as the JM_FORCEINLINE definition in util_core.h;
 * this translation unit emits the single out-of-line copy via the C99
 * `extern inline` idiom. See saturate.c for the full rationale.
 */
#include "util/util_core.h"

extern int gauss_hermite (double *z, size_t z_len, double *p, size_t p_len);
