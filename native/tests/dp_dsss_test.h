/**
 * @file dp_dsss_test.h
 * @brief Stimulus for the DSSS receivers: one code-spread BPSK capture.
 *
 * `test_dsss_receiver_core.c` and `test_async_dsss_receiver_core.c` each
 * carried a private copy of both builders below, and the copies were
 * **byte-identical** — 1430 and 1490 characters, matching character for
 * character, one of them carrying a comment that pointed at the other
 * ("mirrors test_dsss_receiver_core.c's own _make_signal()"). A comment
 * naming the file you were copied from is the clearest possible statement
 * that the copy should not exist.
 *
 * ## Why these two and not the other six
 *
 * `make_signal` is defined in eight files in this directory, and six of the
 * eight are NOT duplication — they are different signals that happen to share
 * a name:
 *
 * | file                        | what its `make_signal` builds            |
 * | --------------------------- | ---------------------------------------- |
 * | test_carrier_acq_core.c     | BPSK NRZ times a tone, noise-free        |
 * | test_carrier_mpsk_core.c    | M-PSK at symbol rate, residual + ramp    |
 * | test_costas_core.c          | the BPSK case of the same                |
 * | test_despreader_core.c      | DSSS with a data bit every N code periods|
 * | test_dll_core.c             | carrier-free, at code rate (1 + delta)   |
 * | test_symsync_core.c         | RC-shaped BPSK at a timing offset        |
 *
 * Folding those onto one builder would not remove duplication; it would
 * delete the differences that each test exists to drive, in the same way
 * consolidating the six `test_state_roundtrip` bodies would. Only the pair
 * that was genuinely one function written twice moves here.
 *
 * ## What stayed behind, and why
 *
 * `_best_ber` also appears in both files and is NOT here: their lag search
 * windows differ (+-20 against +-250), and the wide one carries a paragraph
 * explaining that the asynchronous receiver's settling delay grows as Es/N0
 * falls. That is a real difference with a stated reason.
 *
 * `CODE7`, the length-7 m-sequence, stays a `static const` in each of the
 * three files that use it. Measured rather than assumed: a `static const`
 * array in a header draws `-Wunused-const-variable` from every includer that
 * does not touch it, and these builders take `code` as a parameter, so a
 * future caller can legitimately want the builder without the fixture.
 *
 * ## Convention
 *
 * Both take `cn0_dbhz` and derive the noise amplitude from it against `fs`,
 * so the level is stated in the unit the receivers are specified in rather
 * than as a bare sigma.
 *
 * ## KNOWN DEFECT: these captures are 3.01 dB quieter than they claim (#689)
 *
 * The noise line scales `dp_cgauss` by `sigma / sqrt(2)` — the factor for the
 * OTHER complex-Gaussian convention, the one carrying unit variance PER
 * COMPONENT. `dp_cgauss` carries `E|z|^2 = 1` (see dp_rng_test.h), so the
 * injected power is `sigma^2 / 2`: **a capture asking for 60.0 dB-Hz really
 * delivers 63.01.** Measured over 2e6 draws, `E|n|^2 = 0.4996` against a
 * target of 1.0.
 *
 * The corroboration is physical rather than algebraic, and it is what makes
 * this certain: these tests print EVM about 3 dB BETTER than their nominal
 * Es/N0, which no receiver can do — impairments only move EVM the wrong way.
 *
 * It is pre-existing and faithfully reproduced. The private `cgauss` these
 * builders were copied from had the same `E|z|^2 = 1`, so the factor was
 * already wrong on `main`; the migration is byte-identical, defect included.
 * What made it findable is that the convention is now written down once
 * instead of being re-derived per file behind the phrase "unit-variance
 * complex Gaussian", which is ambiguous between precisely these two readings.
 *
 * **It is not fixed here, and the reason is a finding of its own.** Removing
 * the `/sqrt(2)` makes `test_async_dsss_receiver_core` fail, and not in the
 * way a 3 dB correction should. Its BER sweep goes non-monotonic — 6.0 dB
 * Es/N0 decodes at 0.0029, 8.0 dB fails at 0.4389, 10.0 dB decodes at
 * 0.0000 — which is not a threshold, it is acquisition succeeding or failing
 * per point. Raising the sweep would not fix it. So the honest reading is
 * that these BER assertions have been passing on 3 dB of noise they were
 * never supposed to have, and correcting the level is a receiver
 * investigation rather than a constant. That is #689, not this file.
 *
 * Both allocate. The caller frees `*x_out` and `*data_out`.
 */
#ifndef DP_DSSS_TEST_H
#define DP_DSSS_TEST_H

#include "dp_rng_test.h"
#include <complex.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/** pi, spelled once for the phase accumulators below. */
#define DP_DSSS_PI 3.14159265358979323846

/**
 * A continuous, code-spread BPSK capture with a FIXED residual Doppler:
 * `pre_silence` samples of noise, then
 * `data[si] * code[cph] * exp(j*2*pi*doppler_hz/fs*idx)` plus AWGN.
 *
 * Simplified relative to the repo's Python story's `make_signal()`: no
 * asynchronous symbol/code-epoch clock stress, because that physics is
 * already validated at the Acquisition and Dll level. This is about the
 * composed object's wiring, and it is sized for a fast C test.
 *
 * @param code        the spreading code, `sf` chips of 0/1.
 * @param sf          spreading factor (chips per symbol).
 * @param spc         samples per chip.
 * @param fs          sample rate, Hz.
 * @param tsym        symbol period, in samples.
 * @param doppler_hz  fixed carrier residual, Hz.
 * @param cn0_dbhz    carrier-to-noise density; sets the noise amplitude.
 * @param n_sym       data symbols to generate.
 * @param pre_silence noise-only samples prepended before the signal.
 * @param seed        RNG seed; the data bits and the noise share one stream.
 * @param x_out       receives the malloc'd capture. Caller frees.
 * @param n_out       receives the total sample count.
 * @param data_out    receives the malloc'd +-1 data symbols. Caller frees.
 */
static inline void
dp_dsss_capture (const uint8_t *code, size_t sf, size_t spc, double fs,
                 double tsym, double doppler_hz, double cn0_dbhz, size_t n_sym,
                 size_t pre_silence, uint32_t seed, float complex **x_out,
                 size_t *n_out, double **data_out)
{
  float *csign = malloc (sf * sizeof *csign);
  for (size_t i = 0; i < sf; i++)
    csign[i] = code[i] & 1 ? -1.0f : 1.0f;

  double  *data = malloc ((n_sym + 4) * sizeof *data);
  uint32_t st   = seed;
  for (size_t i = 0; i < n_sym + 4; i++)
    data[i] = (dp_xs32 (&st) & 1u) ? 1.0 : -1.0;

  size_t         n   = (size_t)((double)n_sym * tsym) + 4 * sf * spc;
  size_t         tot = pre_silence + n;
  float complex *x   = calloc (tot, sizeof *x);

  double amp_snr = sqrt (pow (10.0, cn0_dbhz / 10.0) / fs);
  double sigma   = 1.0 / amp_snr;
  for (size_t i = 0; i < tot; i++)
    x[i] = (float complex) (sigma / sqrt (2.0)) * dp_cgauss (&st);

  for (size_t idx = 0; idx < n; idx++)
    {
      size_t si = (size_t)((double)idx / tsym);
      if (si >= n_sym + 4)
        si = n_sym + 3;
      size_t        cph     = (idx / spc) % sf;
      double        ph      = 2.0 * DP_DSSS_PI * doppler_hz / fs * (double)idx;
      float complex carrier = (float complex) (cos (ph) + I * sin (ph));
      x[pre_silence + idx] += (float)(data[si] * csign[cph]) * carrier;
    }

  free (csign);
  *x_out    = x;
  *n_out    = tot;
  *data_out = data;
}

/**
 * As dp_dsss_capture(), but with a linear Doppler RAMP (a chirp) instead of
 * a fixed residual: `f(t) = rate_hz_per_s * t`, and the phase is its
 * integral, `pi * rate * t^2`.
 *
 * @param rate_hz_per_s  the Doppler rate, Hz per second. Every other
 *                       parameter is as dp_dsss_capture().
 */
static inline void
dp_dsss_ramp_capture (const uint8_t *code, size_t sf, size_t spc, double fs,
                      double tsym, double rate_hz_per_s, double cn0_dbhz,
                      size_t n_sym, size_t pre_silence, uint32_t seed,
                      float complex **x_out, size_t *n_out, double **data_out)
{
  float *csign = malloc (sf * sizeof *csign);
  for (size_t i = 0; i < sf; i++)
    csign[i] = code[i] & 1 ? -1.0f : 1.0f;

  double  *data = malloc ((n_sym + 4) * sizeof *data);
  uint32_t st   = seed;
  for (size_t i = 0; i < n_sym + 4; i++)
    data[i] = (dp_xs32 (&st) & 1u) ? 1.0 : -1.0;

  size_t         n   = (size_t)((double)n_sym * tsym) + 4 * sf * spc;
  size_t         tot = pre_silence + n;
  float complex *x   = calloc (tot, sizeof *x);

  double amp_snr = sqrt (pow (10.0, cn0_dbhz / 10.0) / fs);
  double sigma   = 1.0 / amp_snr;
  for (size_t i = 0; i < tot; i++)
    x[i] = (float complex) (sigma / sqrt (2.0)) * dp_cgauss (&st);

  for (size_t idx = 0; idx < n; idx++)
    {
      size_t si = (size_t)((double)idx / tsym);
      if (si >= n_sym + 4)
        si = n_sym + 3;
      size_t        cph     = (idx / spc) % sf;
      double        t       = (double)idx / fs;
      double        ph      = 2.0 * DP_DSSS_PI * (0.5 * rate_hz_per_s * t * t);
      float complex carrier = (float complex) (cos (ph) + I * sin (ph));
      x[pre_silence + idx] += (float)(data[si] * csign[cph]) * carrier;
    }

  free (csign);
  *x_out    = x;
  *n_out    = tot;
  *data_out = data;
}

#endif /* DP_DSSS_TEST_H */
