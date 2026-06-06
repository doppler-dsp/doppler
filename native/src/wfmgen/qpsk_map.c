/*
 * qpsk_map.c — wfmgen module-level function.
 */
#include "wfmgen/wfmgen_core.h"

/*
 * Gray-coded QPSK from symbol indices {0,1,2,3}: bit 0 is the I bit, bit 1 the
 * Q bit.  I = (1 - 2*b_i)/sqrt2,  Q = (1 - 2*b_q)/sqrt2  (unit energy). Gray
 * coding means adjacent constellation points differ in one bit.
 */
void
qpsk_map(const uint8_t *syms, size_t syms_len, float complex *out)
{
    const float s = (float)(1.0 / 1.4142135623730951);
    for (size_t i = 0; i < syms_len; i++) {
        float ii = (1.0f - 2.0f * (float)(syms[i] & 1u)) * s;
        float qq = (1.0f - 2.0f * (float)((syms[i] >> 1) & 1u)) * s;
        out[i] = ii + qq * I;
    }
}
