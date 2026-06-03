/*
 * shr_i64.c — arith module-level function.
 */
#include "arith/arith_core.h"

void
shr_i64(const int64_t *a, size_t a_len, int64_t *out, int n)
{
    for (size_t i = 0; i < a_len; i++) {
        if (n <= 0) {
            out[i] = a[i];
        } else {
            int64_t v = a[i];
            if (n < 63)
                v += (int64_t)1 << (n - 1);
            out[i] = v >> (n < 63 ? n : 63);
        }
    }
}
