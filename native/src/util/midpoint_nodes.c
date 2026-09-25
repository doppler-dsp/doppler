/*
 * midpoint_nodes.c — util module-level function.
 *
 * The body lives ONCE, as the JM_FORCEINLINE definition in util_core.h;
 * this translation unit emits the single out-of-line copy via the C99
 * `extern inline` idiom. See saturate.c for the full rationale.
 */
#include "util/util_core.h"

extern void midpoint_nodes (double *u, size_t u_len);
