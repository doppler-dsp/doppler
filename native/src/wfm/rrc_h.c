/*
 * rrc_h.c — wfm module-level function.
 *
 * Public alias exposing the ANALYTIC root-raised-cosine pulse under the wfm
 * module namespace, evaluated at arbitrary (non-grid) times. The kernel lives
 * once in wfm_dsp.h as the header-inline `wfm_rrc_h` (C-first: the algorithm
 * is not duplicated here); this is the thin glue that gives it a stable wfm
 * public name and a vectorised Python binding.
 *
 * WHY THIS EXISTS, when `rrc_taps` already ships. `rrc_taps` returns taps on
 * an INTEGER sample grid, which is all a filter needs. A stimulus builder
 * needs the pulse at arbitrary real times: a stream at a non-integer samples
 * per symbol (17.33389 — an ADC clock with no rational relationship to the
 * symbol clock) with a fractional timing offset has no grid to sample. C had
 * that primitive all along and Python did not, so every Python harness that
 * needed it transcribed the formula instead — five private copies at the time
 * this was written, which is what `scripts/check_stimulus_sources.py` counts.
 * A gate that says "use the library" has to be answerable in the language it
 * is scolding.
 */
#include "wfm/wfm_core.h"
#include "wfm/wfm_dsp.h" /* wfm_rrc_h — header-inline, no link edge */

void
rrc_h (const double *t, size_t t_len, double *out, double beta)
{
  /* Defensive guard, mirroring rrc_taps.c: the generated binding does not
     range-check beta, and the kernel's closed-form limits are derived for
     beta in [0, 1]. Outside it the singularity branches are simply wrong
     rather than merely inaccurate, so refuse the degenerate pulse and zero
     the binding-allocated output instead of returning plausible nonsense. */
  if (!(beta >= 0.0 && beta <= 1.0))
    {
      for (size_t i = 0; i < t_len; i++)
        out[i] = 0.0;
      return;
    }
  for (size_t i = 0; i < t_len; i++)
    out[i] = wfm_rrc_h (t[i], beta);
}
