/**
 * @file dp_preamble_test.h
 * @brief A PN code as preamble SAMPLES, for the burst engines' tests.
 *
 * The burst constructors -- acq_create_burst(), burst_acq_create(),
 * burst_capture_create() -- take a preamble as its samples (doppler#1470):
 * one period, at `fs`. A PN code is one such preamble, built the way a
 * caller builds it: chips mapped by bin_to_nrz(), the library's one
 * chip -> +-1 rule, each held `spc` samples, at `fs = chip_rate * spc`.
 *
 * One builder, so a test cannot quietly grow its own mapping: before this
 * the chip -> sign rule was written inline dozens of times across these
 * files, each free to disagree with the library's.
 *
 * Link `cvt` (bin_to_nrz) into any test that includes this, and `fft` into
 * one that calls dp_preamble_shift().
 */
#ifndef DP_PREAMBLE_TEST_H
#define DP_PREAMBLE_TEST_H

#include "cvt/cvt_core.h"
#include "fft/fft_core.h"
#include "util/util_core.h"
#include <complex.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @brief One period of @p code, mapped by bin_to_nrz() and held @p spc
 *        samples a chip: `code_len * spc` samples, heap-allocated.
 *
 * @param code      PN chips (0/1), length @p code_len.
 * @param code_len  Chips in one period (>= 1).
 * @param spc       Samples per chip (>= 1).
 * @return The samples; the caller frees them.
 */
static inline float _Complex *
dp_code_preamble (const uint8_t *code, size_t code_len, size_t spc)
{
  float          *nrz = dp_xmalloc (code_len * sizeof *nrz);
  float _Complex *pre = dp_xmalloc (code_len * spc * sizeof *pre);
  (void)bin_to_nrz (code, code_len, nrz, code_len);
  for (size_t i = 0; i < code_len * spc; i++)
    pre[i] = nrz[i / spc];
  free (nrz);
  return pre;
}

/**
 * @brief One period of a preamble delayed by @p tau samples, band-limited.
 *
 * Multiplies bin k of the period's FFT by `e^{-j 2 pi f_k tau / n}` over the
 * signed bin f_k; an even length's Nyquist bin takes `cos(pi tau)`. For a
 * periodic signal this is the exact fractional delay, and it is the kernel
 * the engine's delay straddle (acq_shape_t) is built on.
 *
 * A harness that puts a burst on a WHOLE sample hands the engine a code
 * phase with no straddle at all, which the Pd model averages over half a
 * sample: measured at Zadoff-Chu 127, D = 8, Pd 0.848 against 0.661 at a
 * continuous delay (doppler#1498). A real signal's delay is continuous.
 *
 * @param t    One period, @p n samples.
 * @param n    Samples in the period (>= 1).
 * @param tau  Delay in samples; any real value.
 * @param out  Written with @p n samples; may not alias @p t.
 */
static inline void
dp_preamble_shift (const float _Complex *t, size_t n, double tau,
                   float _Complex *out)
{
  double _Complex *in   = dp_xmalloc (n * sizeof *in);
  double _Complex *spec = dp_xmalloc (n * sizeof *spec);
  fft_state_t     *fwd  = dp_xnn (fft_create (n, -1, 1));
  fft_state_t     *inv  = dp_xnn (fft_create (n, +1, 1));
  for (size_t i = 0; i < n; i++)
    in[i] = (double)crealf (t[i]) + I * (double)cimagf (t[i]);
  fft_execute_cf64 (fwd, in, n, spec, n);
  for (size_t k = 0; k < n; k++)
    {
      if ((n & 1u) == 0 && k == n / 2)
        {
          spec[k] *= cos (M_PI * tau);
          continue;
        }
      double f = (k <= n / 2) ? (double)k : (double)k - (double)n;
      spec[k] *= cexp (-I * 2.0 * M_PI * f * tau / (double)n);
    }
  fft_execute_cf64 (inv, spec, n, in, n);
  for (size_t i = 0; i < n; i++)
    out[i] = (float _Complex) (in[i] / (double)n);
  fft_destroy (fwd);
  fft_destroy (inv);
  free (spec);
  free (in);
}

#endif /* DP_PREAMBLE_TEST_H */
