/*
 * util_core.c — Util module implementation.
 *
 * util's public functions are header-only (JM_FORCEINLINE in
 * util_core.h) so every caller inlines them.  This translation unit
 * exists only to give the util_core object library something to
 * compile; the functions themselves live entirely in the header.
 */
#include "util/util_core.h"
