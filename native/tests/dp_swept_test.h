/**
 * @file dp_swept_test.h
 * @brief A swept carrier for C tests: rotate a block by a chirp about a
 *        stated center.
 *
 * `test_acq_core.c`, `test_burst_capture_core.c` and
 * `native/validation/acq_rate_pd.c` each carried a private
 * `exp(j*2*pi*(f_mid*u + 0.5*rate*u*u))` phase ramp.  This is the one copy,
 * built on the library's own operator -- a chirp `wfm_synth` and
 * `dp_wfm_synth_prod()` -- and it is the C twin of Python's
 * `doppler.wfm.rotate`.
 *
 * ## The phase it applies
 *
 * With `u = i - center` (the sample index about the stated center):
 *
 *     x[i] *= exp(j*2*pi*(f_mid/fs*u + 0.5*(rate/fs^2)*u*u))
 *
 * `f_mid` is the carrier at `u = 0` in Hz, `rate` the sweep in Hz/s.  Pass
 * `fs = 1` and `f_mid`, `rate` in cycles/sample and cycles/sample^2 for the
 * normalized form.  `center` is a parameter, not "the middle of the buffer":
 * a burst whose preamble is swept about its OWN middle while the payload
 * continues the same sweep wants `center` at the preamble's middle.
 *
 * ## What it differs by
 *
 * The synth's chirp phase is 0 at sample 0; this ramp is 0 at `u = 0`.  The
 * two differ by a constant phase, which no detector in this tree depends on
 * (acquisition correlates non-coherently across the dwell).  The slope and
 * the instantaneous frequency match exactly; the start and end frequencies
 * come from dp_wfm_synth_sweep_ends(), the one home of that arithmetic.
 */
#ifndef DP_SWEPT_TEST_H
#define DP_SWEPT_TEST_H

#include "doppler/clib_common.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include <stddef.h>

/**
 * Rotate `x[0..n-1]` in place by a swept carrier about sample `center`.
 * A synth that cannot be built aborts (`dp_xnn`): a harness has no recovery.
 */
static inline void
dp_swept_rotate (float _Complex *x, size_t n, double fs, double f_mid,
                 double rate, double center)
{
  double    f0 = f_mid, f_end = 0.0;
  const int swept = rate != 0.0;
  if (swept)
    dp_wfm_synth_sweep_ends (fs, f_mid, rate, n, center, &f0, &f_end);
  dp_wfm_synth_state_t *s = dp_xnn (
      dp_wfm_synth_create (swept ? WFM_SYNTH_CHIRP : WFM_SYNTH_TONE, fs, f0,
                           WFM_SYNTH_SNR_CLEAN, 0, 1, 8, 7, 0, 0, f_end));
  if (swept)
    dp_wfm_synth_set_chirp_span (s, n);
  dp_wfm_synth_prod (s, x, n, x);
  dp_wfm_synth_destroy (s);
}

#endif /* DP_SWEPT_TEST_H */
