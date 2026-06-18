/*
 * dsss_spread.c — wfm module-level function.
 *
 * Public alias of the direct-sequence-spread kernel under the wfm namespace.
 * The kernel (wfm_dsp.c) reads the first `sf` chips of `code`; the binding's
 * generated `code_len` is unused (kept for the jm array-param ABI — the caller
 * is expected to pass a code of length >= sf, as the Python type hint states).
 */
#include "wfm/wfm_core.h"
#include "wfm/wfm_dsp.h" /* wfm_dsss_spread */

void
dsss_spread (const float complex *syms, size_t syms_len, const uint8_t *code,
             size_t code_len, int sf, float complex *out)
{
  (void)code_len;
  wfm_dsss_spread (syms, syms_len, code, (size_t)sf, out);
}
