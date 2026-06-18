/*
 * rrc_taps.c — wfm module-level function.
 *
 * Public alias exposing the root-raised-cosine tap generator under the wfm
 * module namespace.  The kernel lives once in wfm_dsp.c (C-first: the
 * algorithm is not duplicated here); this is the thin glue that gives it a
 * stable wfm public name with a self-sizing (variable_output) Python binding.
 */
#include "wfm/wfm_core.h"
#include "wfm/wfm_dsp.h" /* wfm_rrc_taps */

void
rrc_taps (double beta, int sps, int span, float *out)
{
  /* Defensive guard (gh-178 review #4): the generated binding no longer
     enforces sps, span >= 1 (the retired hand binding raised "sps and span
     must be >= 1"). With sps < 1 the kernel divides each tap time by sps,
     yielding inf/NaN taps. We do not raise (jm bindings stay unchecked); we
     refuse the degenerate filter and zero the binding-allocated output. Its
     length is the binding's 2*span*sps + 1, mirrored here with the signed
     inputs so we never write past it. */
  if (sps < 1 || span < 1)
    {
      long n = 2L * span * sps + 1;
      for (long i = 0; i < n; i++)
        out[i] = 0.0f;
      return;
    }
  wfm_rrc_taps (beta, sps, span, out);
}
