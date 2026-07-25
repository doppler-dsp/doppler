/**
 * @file dp_mf_test.h
 * @brief Shared matched-filter test fixtures: RRC-BPSK on a carrier + EVM.
 *
 * Used by the down-converter suites (test_ddc_core.c, test_ddcr_core.c),
 * which measure the same signal through the complex and real chains.
 *
 * TWO MEASUREMENT TRAPS, documented once here rather than rediscovered:
 *
 * 1. **The transmit phase grid must be finer than the strobe grid**, or the
 *    sweep is what gets measured instead of the filter. At 16 samples per
 *    symbol an eighth-symbol step is TWO whole input samples, which leaves up
 *    to a sixteenth of a symbol of residual timing error and reads -24 dB;
 *    sixteen phases (and thirty-two) both read -45 dB, i.e. converged.
 * 2. **Keep the amplitude inside the CIC's +-1.0 input bound.** A CIC
 *    quantizes at that boundary and clips silently past it, costing ~25 dB of
 *    EVM for reasons that have nothing to do with the matched filter.
 */
#ifndef DP_MF_TEST_H
#define DP_MF_TEST_H

#include "wfm/wfm_dsp.h" /* wfm_rrc_h — static inline, no link edge */
#include <complex.h>
#include <math.h>
#include <stdlib.h>

#define MF_NSYM 400
#define MF_SPAN 8
#define MF_BETA 0.35

/* Deterministic +-1 BPSK. A one-shot LCG is not random across consecutive k,
 * which starves a *timing* detector — irrelevant here, where nothing is
 * driven by symbol transitions. */
static inline int
mf_bit (int k)
{
  unsigned x = (unsigned)k * 1103515245u + 12345u;
  return ((x >> 16) & 1) ? 1 : -1;
}

/* RRC-shaped BPSK at `sps` input samples/symbol, timing phase `phi` symbols,
 * on a carrier at normalised frequency `fc`. Amplitude 0.25 (see trap 2). */
static inline float _Complex *
mf_tx (double sps, double phi, double fc, size_t *n_out)
{
  size_t          n = (size_t)(MF_NSYM * sps) + 64;
  float _Complex *x = calloc (n, sizeof *x);
  if (!x)
    return NULL;
  for (size_t i = 0; i < n; i++)
    {
      double a = 0.0;
      for (int k = 0; k < MF_NSYM; k++)
        {
          double t = ((double)i - (k + MF_SPAN) * sps) / sps - phi;
          if (fabs (t) > MF_SPAN)
            continue;
          a += mf_bit (k) * wfm_rrc_h (t, MF_BETA);
        }
      double ph = 2.0 * M_PI * fc * (double)i;
      x[i]      = (float _Complex) (0.25 * a * (cos (ph) + I * sin (ph)));
    }
  *n_out = n;
  return x;
}

/* Best EVM (dB) over strobe alignment at two samples/symbol. The complex gain
 * is fitted, so a constant rotation (the real chain's halfband leaves one) is
 * normalised out and only the error vector is measured. Open loop: the
 * cascade's strobe phase is arbitrary until a timing loop steers it, so take
 * the minimum. */
static inline double
mf_evm_db (const float _Complex *y, size_t ny)
{
  double best = 1e9;
  for (int par = 0; par < 2; par++)
    for (int lag = 0; lag < 140; lag++)
      {
        double _Complex num = 0.0;
        int cnt             = 0;
        for (int k = 40; k < MF_NSYM - 40; k++)
          {
            size_t i = (size_t)(lag + par + 2 * k);
            if (i >= ny)
              break;
            num += mf_bit (k) * y[i];
            cnt++;
          }
        if (cnt < 100)
          continue;
        double _Complex g = num / cnt;
        double e = 0.0, p = 0.0;
        for (int k = 40; k < MF_NSYM - 40; k++)
          {
            size_t i = (size_t)(lag + par + 2 * k);
            if (i >= ny)
              break;
            double _Complex d = y[i] - g * mf_bit (k);
            e += creal (d) * creal (d) + cimag (d) * cimag (d);
            p += creal (g) * creal (g) + cimag (g) * cimag (g);
          }
        double v = sqrt (e / p);
        if (v < best)
          best = v;
      }
  return 20.0 * log10 (best);
}

#endif /* DP_MF_TEST_H */
