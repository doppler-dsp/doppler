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
 * ## The noise is the library's (#689)
 *
 * Every capture's noise comes from the shipped `awgn` object, sized by
 * dp_awgn_amplitude_for_snr() for a unit-power signal at `cn0_dbhz` over
 * `fs` -- the one answer to "per rail or total" -- through
 * dp_dsss_add_awgn() below, on a stream of its own. Until #689 the builders
 * scaled `dp_cgauss` (`E|z|^2 = 1`) by a hand `sigma / sqrt(2)`, the factor
 * for the other complex-Gaussian convention, so every capture ran 3.01 dB
 * quieter than its label; test_dp_dsss.c now asserts the label's power, both
 * rails counted.
 *
 * Both allocate. The caller frees `*x_out` and `*data_out`.
 */
#ifndef DP_DSSS_TEST_H
#define DP_DSSS_TEST_H

#include "doppler/awgn/awgn_core.h"
#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/dp_complex.h"
#include "doppler/gold/gold_core.h"
#include "doppler/wfm_synth/wfm_synth_core.h"
#include "dp_rng_test.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * Add complex AWGN at `cn0_dbhz` dB-Hz over `fs`, against a unit-power
 * signal, to `x[0..n)`: the shipped generator, sized by
 * dp_awgn_amplitude_for_snr() (the per-sample SNR is `cn0_dbhz - 10 log10
 * fs`), never a hand sigma (#689). `seed` picks the stream. Aborts on an
 * allocation failure, as a test fixture may.
 */
static inline void
dp_dsss_add_awgn (float _Complex *x, size_t n, double fs, double cn0_dbhz,
                  uint32_t seed)
{
  dp_awgn_state_t *g = dp_awgn_create (
      (uint64_t)seed,
      dp_awgn_amplitude_for_snr ((float)(cn0_dbhz - 10.0 * log10 (fs)), 1.0f));
  float _Complex *nz = malloc ((n ? n : 1) * sizeof *nz);
  if (!g || !nz)
    abort ();
  (void)dp_awgn_generate (g, n, nz, n);
  for (size_t i = 0; i < n; i++)
    x[i] += nz[i];
  free (nz);
  dp_awgn_destroy (g);
}

/** pi, spelled once for the phase accumulators below. */
#define DP_DSSS_PI 3.14159265358979323846

/**
 * The 1023-chip Gold code the continuous-receiver validators run on: CCSDS
 * code #365, `gold_core.h`'s own worked example, as 0/1 chips.
 *
 * Ten validators under `native/validation/` carried this as a private
 * `gold_1023()`, nine of them byte-identical and the tenth differing only by
 * a comment; this is the one spelling.
 *
 * @param code  receives 1023 chips.
 */
static inline void
dp_dsss_gold_1023 (uint8_t code[1023])
{
  dp_gold_state_t *gd = dp_gold_create (934, 350, 567, 73, 10);
  dp_gold_generate (gd, 1023, code, 1023);
  dp_gold_destroy (gd);
}

/**
 * The continuous asynchronous-data DSSS emitter, clean, from the shipped
 * generator: `wfm_synth` continuous DSSS, the generator wfmgen renders the
 * waveform with, at carrier `freq_hz`.
 *
 * The validators of the continuous receiver render their emitter this way
 * and add noise as a separate stream, so an event -- a switch-off, a fade, a
 * dropout -- is an edit to the signal alone. Six carried this as private
 * copies (two byte-identical, four the same two calls inline); this is the
 * one spelling. A user-facing example keeps its own, since an example
 * cannot reach `native/tests/`.
 *
 * The data is the caller's `data_mode`, passed through: the synth's own
 * seeded PRBS (`WFM_DSSS_DATA_PRBS`, `bits` NULL), or `n_bits` 0/1 bits sent
 * once, one per symbol (`WFM_DSSS_DATA_BITS`), for a harness that must know
 * where the data transitions -- a lock measured mid-data, never in a
 * data-free stretch. The mode is the caller's to name, not this helper's,
 * because a pattern sent once runs dry into silence (doppler#1718): the file
 * that chooses one must check `dp_wfm_synth_data_ended()` after its run, and
 * `check_stimulus_sources.py` finds that file by the mode it names. No window
 * is set: the stream is data throughout. Add one with
 * `dp_wfm_synth_set_dsss_window()`.
 *
 * @param code              the spreading code, `sf` chips of 0/1.
 * @param sf                chips per code period.
 * @param spc               samples per chip.
 * @param fs                sample rate, Hz.
 * @param freq_hz           the carrier, Hz.
 * @param chips_per_symbol  chip rate over symbol rate; non-integer is what
 *                          makes the data asynchronous to the code.
 * @param seed              the synth's seed (its PRBS, when it draws one).
 * @param data_mode         `WFM_DSSS_DATA_PRBS`, or `_BITS` with `bits`.
 * @param bits              the data, 0/1 per symbol, or NULL for the PRBS.
 * @param n_bits            how many; the stream is silent after the last.
 * @return the synth, or NULL on a refused configuration. Caller destroys.
 */
static inline dp_wfm_synth_state_t *
dp_dsss_cont_emitter (const uint8_t *code, size_t sf, size_t spc, double fs,
                      double freq_hz, double chips_per_symbol, uint32_t seed,
                      int data_mode, const uint8_t *bits, size_t n_bits)
{
  dp_wfm_synth_state_t *syn
      = dp_wfm_synth_create (WFM_SYNTH_DSSS, fs, freq_hz, WFM_SYNTH_SNR_CLEAN,
                             1, seed, (int)spc, 15, 0, 0, 0.0);
  if (syn
      && dp_wfm_synth_set_dsss_cont (syn, code, sf, chips_per_symbol,
                                     data_mode, bits, bits ? n_bits : 0)
             != 0)
    {
      dp_wfm_synth_destroy (syn);
      syn = NULL;
    }
  return syn;
}

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
 * @param w_sym       code-only symbols opening every frame (+1 data): the
 *                    window a block-coherent searcher aligns inside.
 * @param f_sym       the frame, symbols; 0 = no window (dp_dsss_capture()).
 * @param x_out       receives the malloc'd capture. Caller frees.
 * @param n_out       receives the total sample count.
 * @param data_out    receives the malloc'd +-1 data symbols. Caller frees.
 */
static inline void
dp_dsss_windowed_capture (const uint8_t *code, size_t sf, size_t spc,
                          double fs, double tsym, double doppler_hz,
                          double cn0_dbhz, size_t n_sym, size_t pre_silence,
                          uint32_t seed, size_t w_sym, size_t f_sym,
                          float _Complex **x_out, size_t *n_out,
                          double **data_out)
{
  float *csign = malloc (sf * sizeof *csign);
  for (size_t i = 0; i < sf; i++)
    csign[i] = code[i] & 1 ? -1.0f : 1.0f;

  /* The data: random +-1, except the code-only window -- the first
     `w_sym` symbols of every `f_sym` are +1 (the synth's rule,
     wfm_synth_cont_dsss_chip), which is what a block-coherent searcher
     aligns its epochs inside. `f_sym` 0 is no window. */
  double  *data = malloc ((n_sym + 4) * sizeof *data);
  uint32_t st   = seed;
  for (size_t i = 0; i < n_sym + 4; i++)
    data[i] = (f_sym && i % f_sym < w_sym) ? 1.0
              : (dp_xs32 (&st) & 1u)       ? 1.0
                                           : -1.0;

  size_t          n   = (size_t)((double)n_sym * tsym) + 4 * sf * spc;
  size_t          tot = pre_silence + n;
  float _Complex *x   = calloc (tot, sizeof *x);

  dp_dsss_add_awgn (x, tot, fs, cn0_dbhz, seed ^ 0x6a09e667u);

  for (size_t idx = 0; idx < n; idx++)
    {
      size_t si = (size_t)((double)idx / tsym);
      if (si >= n_sym + 4)
        si = n_sym + 3;
      size_t cph = (idx / spc) % sf;
      double ph  = 2.0 * DP_DSSS_PI * doppler_hz / fs * (double)idx;
      float _Complex carrier = (float _Complex) (cos (ph) + I * sin (ph));
      x[pre_silence + idx] += (float)(data[si] * csign[cph]) * carrier;
    }

  free (csign);
  *x_out    = x;
  *n_out    = tot;
  *data_out = data;
}

/** dp_dsss_windowed_capture() with no window: every symbol is data. */
static inline void
dp_dsss_capture (const uint8_t *code, size_t sf, size_t spc, double fs,
                 double tsym, double doppler_hz, double cn0_dbhz, size_t n_sym,
                 size_t pre_silence, uint32_t seed, float _Complex **x_out,
                 size_t *n_out, double **data_out)
{
  dp_dsss_windowed_capture (code, sf, spc, fs, tsym, doppler_hz, cn0_dbhz,
                            n_sym, pre_silence, seed, 0, 0, x_out, n_out,
                            data_out);
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
                      float _Complex **x_out, size_t *n_out, double **data_out)
{
  float *csign = malloc (sf * sizeof *csign);
  for (size_t i = 0; i < sf; i++)
    csign[i] = code[i] & 1 ? -1.0f : 1.0f;

  double  *data = malloc ((n_sym + 4) * sizeof *data);
  uint32_t st   = seed;
  for (size_t i = 0; i < n_sym + 4; i++)
    data[i] = (dp_xs32 (&st) & 1u) ? 1.0 : -1.0;

  size_t          n   = (size_t)((double)n_sym * tsym) + 4 * sf * spc;
  size_t          tot = pre_silence + n;
  float _Complex *x   = calloc (tot, sizeof *x);

  dp_dsss_add_awgn (x, tot, fs, cn0_dbhz, seed ^ 0x6a09e667u);

  for (size_t idx = 0; idx < n; idx++)
    {
      size_t si = (size_t)((double)idx / tsym);
      if (si >= n_sym + 4)
        si = n_sym + 3;
      size_t cph = (idx / spc) % sf;
      double t   = (double)idx / fs;
      double ph  = 2.0 * DP_DSSS_PI * (0.5 * rate_hz_per_s * t * t);
      float _Complex carrier = (float _Complex) (cos (ph) + I * sin (ph));
      x[pre_silence + idx] += (float)(data[si] * csign[cph]) * carrier;
    }

  free (csign);
  *x_out    = x;
  *n_out    = tot;
  *data_out = data;
}

/**
 * As dp_dsss_capture(), but through the shipped `doppler_channel`: the
 * clean signal is resampled by `1/(1 + d(t))` and its carrier moved by
 * `carrier_hz * d(t)`, `d` in ppm ramping `ppm_s` per second -- the chip,
 * symbol AND carrier clocks all dilated together, as a real link's are --
 * and the noise is added after the channel, at the receiver, where it
 * belongs (sized exactly as dp_dsss_capture() sizes it). The channel's
 * resampler delays the signal by a few chips, so the code phase on the
 * first signal sample is NOT zero here: a receiver must find it, as the
 * searching flavor does.
 * @param carrier_hz  the RF carrier the ppm scale to (0 dilates the clocks
 *                    and never moves the carrier).
 * @param ppm         initial Doppler, ppm of the time base (positive =
 *                    closing, clocks fast, carrier up).
 * @param ppm_s       its ramp, ppm per second. Every other parameter is as
 *                    dp_dsss_capture().
 */
static inline void
dp_dsss_dilated_capture (const uint8_t *code, size_t sf, size_t spc, double fs,
                         double tsym, double carrier_hz, double ppm,
                         double ppm_s, double cn0_dbhz, size_t n_sym,
                         size_t pre_silence, uint32_t seed,
                         float _Complex **x_out, size_t *n_out,
                         double **data_out)
{
  /* Clean render (a C/N0 no noise survives), then the channel. */
  float _Complex *clean;
  size_t          n;
  dp_dsss_capture (code, sf, spc, fs, tsym, 0.0, 300.0, n_sym, pre_silence,
                   seed, &clean, &n, data_out);
  dp_doppler_channel_state_t *ch
      = dp_doppler_channel_create (fs, carrier_hz, ppm, ppm_s);
  float _Complex *x   = malloc ((n + DOPPLER_CHANNEL_MAX_BLOCK) * sizeof *x);
  size_t          out = 0;
  for (size_t pos = 0; pos < n; pos += DOPPLER_CHANNEL_MAX_BLOCK)
    {
      size_t take = n - pos < DOPPLER_CHANNEL_MAX_BLOCK
                        ? n - pos
                        : DOPPLER_CHANNEL_MAX_BLOCK;
      out += dp_doppler_channel_execute (ch, clean + pos, take, x + out,
                                         n + DOPPLER_CHANNEL_MAX_BLOCK - out);
    }
  dp_doppler_channel_destroy (ch);
  free (clean);

  /* The data's stream, decorrelated. */
  dp_dsss_add_awgn (x, out, fs, cn0_dbhz, seed ^ 0x9e3779b9u);

  *x_out = x;
  *n_out = out;
}

#endif /* DP_DSSS_TEST_H */
