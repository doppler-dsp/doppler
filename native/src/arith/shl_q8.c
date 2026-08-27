/*
 * shl_q8.c — arith module-level function.
 */
#include "arith/arith_core.h"

void
shl_q8 (const int8_t *a, size_t a_len, int8_t *out, int n)
{
  for (size_t i = 0; i < a_len; i++)
    {
      if (n <= 0)
        out[i] = a[i];
      else if (n >= 8)
        out[i] = a[i] > 0 ? 127 : a[i] < 0 ? -128 : 0;
      else
        /* Multiply rather than shift, for the reason shl_q15.c gives:
           `<< n` on a negative value is undefined. n is in [1, 7], so
           the product is at most 128 * 128 and fits int16_t.

           Here the multiply is REAL -- gcc emits `pmullw`/`imull` rather
           than folding it back -- and that is why this kernel is worth a
           note. It is not a cost, it is a 4.5x SPEEDUP: 16-bit packed
           multiply vectorises eight lanes at a time, where the variable
           shift the original used does not vectorise nearly as well.

             measured 2026-08-27, gcc -O3 -march=x86-64-v2, 8192 samples
             x 20000 reps, median of 10, half the inputs negative:

               `<< n`  (the original, undefined)   0.0607 s
               `* (1 << n)` (this)                 0.0135 s
               `(int32_t)((uint32_t)a[i] << n)`    0.0389 s

           So the obvious "fix" -- shift in unsigned to dodge the UB, the
           idiom shl_i64.c uses -- is nearly 3x slower here. It was
           written, measured, and thrown away. Reproduce before changing
           this line; the numbers are from one machine (WSL2) and the
           ratio is what matters, not the absolute times. */
        out[i] = sat8 ((int16_t)(a[i] * (int16_t)(1 << n)));
    }
}
