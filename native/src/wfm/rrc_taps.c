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
  wfm_rrc_taps (beta, sps, span, out);
}
