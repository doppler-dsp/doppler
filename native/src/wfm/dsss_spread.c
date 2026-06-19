/*
 * dsss_spread.c — wfm module-level function.
 *
 * Public alias of the direct-sequence-spread kernel under the wfm namespace.
 * The kernel (wfm_dsp.c) reads the first `sf` chips of `code` and writes
 * `syms_len * sf` chips; the caller is expected to pass a code of length >= sf
 * (as the Python type hint states). This thin alias is the seam that sees both
 * `sf` and `code_len`, so it carries the bounds guards the generated binding
 * does not (gh-178 review #2).
 */
#include "wfm/wfm_core.h"
#include "wfm/wfm_dsp.h" /* wfm_dsss_spread */

void
dsss_spread (const float complex *syms, size_t syms_len, const uint8_t *code,
             size_t code_len, int sf, float complex *out)
{
  /* Defensive guards: the generated binding leaves sf / code_len unchecked, so
     a misuse the retired hand binding rejected with a ValueError would drive
     the kernel into a heap over-read/-write:
       - sf < 1      : the kernel casts sf to size_t; a negative sf wraps to a
                       huge count and the write loop runs unbounded.
       - code_len<sf : the kernel reads code[code_len .. sf-1] past the buffer.
     We do not raise (jm bindings stay unchecked by design); we refuse the
     unsafe spread and zero the (binding-allocated, syms_len * sf) output so no
     out-of-bounds or uninitialised heap memory is touched / returned. */
  if (sf < 1 || code_len < (size_t)sf)
    {
      size_t n = syms_len * (size_t)(sf > 0 ? sf : 0);
      for (size_t k = 0; k < n; k++)
        out[k] = 0.0f;
      return;
    }
  wfm_dsss_spread (syms, syms_len, code, (size_t)sf, out);
}
