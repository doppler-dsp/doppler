/*
 * rc_h.c — wfm module-level function.
 *
 * Public alias exposing the ANALYTIC full raised-cosine pulse under the wfm
 * module namespace, evaluated at arbitrary (non-grid) times. The kernel lives
 * once in wfm_dsp.h as the header-inline `wfm_rc_h`; this is the thin glue
 * that gives it a stable wfm public name and a vectorised Python binding.
 *
 * The ROOT raised cosine (`rrc_h`) is the transmit half of a matched-filter
 * pair — cascading TX and RX gives the Nyquist response. This one is that
 * Nyquist response already, so it is what a harness wants when it models the
 * matched-filter OUTPUT directly rather than filtering: a timing-detector
 * S-curve reference, or a receiver test whose front end is collapsed away.
 * `test_symsync_core.c` and `native/validation/symsync_lock.c` each carry a
 * private copy of exactly this function.
 */
#include "wfm/wfm_core.h"
#include "wfm/wfm_dsp.h" /* wfm_rc_h — header-inline, no link edge */

void
rc_h (const double *t, size_t t_len, double *out, double beta)
{
  /* Same guard and same reasoning as rrc_h.c: beta outside [0, 1] makes the
     closed-form limit branches wrong rather than imprecise. */
  if (!(beta >= 0.0 && beta <= 1.0))
    {
      for (size_t i = 0; i < t_len; i++)
        out[i] = 0.0;
      return;
    }
  for (size_t i = 0; i < t_len; i++)
    out[i] = wfm_rc_h (t[i], beta);
}
