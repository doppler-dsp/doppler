/*
 * dot_q15.c — arith module-level function.
 */
#include "arith/arith_core.h"
#include "q15_mac.h"

int64_t
dot_q15 (const int16_t *a, size_t a_len, const int16_t *b, size_t b_len)
{
  size_t n = a_len < b_len ? a_len : b_len;
#if defined(__AVX2__)
  return dot_q15_avx2 (a, b, n);
#else
  return dot_q15_scalar (a, b, n);
#endif
}
