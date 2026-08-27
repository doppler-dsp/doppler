/*
 * shl_i64.c — arith module-level function.
 */
#include "arith/arith_core.h"

void
shl_i64 (const int64_t *a, size_t a_len, int64_t *out, int n)
{
  for (size_t i = 0; i < a_len; i++)
    {
      if (n <= 0)
        out[i] = a[i];
      else if (n >= 63)
        out[i] = 0;
      else
        /* Shift in UNSIGNED and convert back: `<< n` on a negative value
           is undefined in C99. Unlike the Q formats above there is no
           saturation here -- no Q format, so no ceiling to clamp to --
           and the intended semantics is precisely the truncating wrap
           the unsigned shift gives. Measured level with the original at
           64-bit width, unlike shl_q8.c, where this same idiom would
           cost 3x -- the narrow packed multiply has no analogue here. */
        out[i] = (int64_t)((uint64_t)a[i] << n);
    }
}
