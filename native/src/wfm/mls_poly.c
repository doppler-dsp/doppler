/*
 * mls_poly.c — wfm module-level function.
 *
 * Public alias exposing the synth engine's maximal-length-sequence (MLS)
 * primitive-polynomial table under the wfm module namespace.  The table itself
 * lives once in wfm_synth_core (C-first: the algorithm is not duplicated
 * here); this is the thin glue that gives it a stable wfm public name.
 */
#include "wfm/wfm_core.h"
#include "wfm_synth/wfm_synth_core.h" /* wfm_synth_mls_poly */

uint64_t
mls_poly (uint32_t n)
{
  return wfm_synth_mls_poly (n);
}
