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
 * the instantaneous frequency match exactly: the synth's sample-i phase is
 * `f0*i + k*i*(i-1)/2` (the increment is applied after the sample), so
 * `f0 = f_mid - k*(center - 0.5)`.
 */
#ifndef DP_SWEPT_TEST_H
#define DP_SWEPT_TEST_H

#include "doppler/wfm_synth/wfm_synth_core.h"
#include <stddef.h>

/**
 * Rotate `x[0..n-1]` in place by a swept carrier about sample `center`.
 *
 * @return 0 on success, -1 if the synth could not be built.
 */
static inline int
dp_swept_rotate (float _Complex *x, size_t n, double fs, double f_mid,
                 double rate, double center)
{
  const double          k     = rate / fs; /* Hz per sample */
  const double          f0    = f_mid - k * (center - 0.5);
  const int             swept = rate != 0.0;
  dp_wfm_synth_state_t *s     = dp_wfm_synth_create (
      swept ? WFM_SYNTH_CHIRP : WFM_SYNTH_TONE, fs, swept ? f0 : f_mid,
      WFM_SYNTH_SNR_CLEAN, 0, 1, 8, 7, 0, 0, swept ? f0 + k * (double)n : 0.0);
  if (!s)
    return -1;
  if (swept)
    dp_wfm_synth_set_chirp_span (s, n);
  dp_wfm_synth_prod (s, x, x, n);
  dp_wfm_synth_destroy (s);
  return 0;
}

#endif /* DP_SWEPT_TEST_H */
