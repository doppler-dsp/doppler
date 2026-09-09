/*
 * test_async_dsss_receiver_core.c — composed AsyncDsssReceiver C-level
 * tests.
 *
 * Covers: argument validation, a real Acquisition -> refine
 * (CarrierAcquisition) -> Dll/Costas/RateConverter/MpskReceiver run over
 * synthetic continuous DSSS-BPSK signals (searching -> refining ->
 * tracking transitions, correct decode), the refine stage's give-up cap
 * (pure noise never fires CarrierAcquisition -- must still reach
 * tracking with the unrefined coarse estimate, never stall), the
 * state-serialization round trip in all three phases, an envelope-reject
 * check, and SPEC's own combined-scenario geometry (500 Hz/s Doppler
 * ramp, Gold-1023-style code / async BPSK data clock) at two operating
 * points: a moderately-stressed Es/N0 where full decode is verified
 * (_test_spec_ramp_decode(), proving this object's own new machinery --
 * the refine stage and its per-code-period carrier cadence -- works
 * correctly under a real ramp), and SPEC's own literal 5dB floor
 * (_test_spec_combined_scenario_at_spec_floor(), which only checks the
 * state machine doesn't stall and produces a finite estimate -- see its
 * own comment for the decisive finding that full decode failure at this
 * exact operating point is a PRE-EXISTING limitation shared by the
 * already-shipped `DsssReceiver`, not a defect in this object). Uses the
 * same signal-generation helpers test_dsss_receiver_core.c's own
 * _test_sustained_doppler_rate() uses, ported to this object's API (no
 * shared test-utils header exists yet for these generators, matching
 * this project's own established per-test-file convention).
 *
 * The multi-emitter additions (docs/design/async-dsss-receiver.md section
 * 11.1-11.2): hand-off mode (no search; idle -> seed -> refine -> track,
 * assigned once, reset() to idle), seed() on the searching flavor and its
 * range checks, the release rule (both flags down past lost_confirm_s ->
 * lost, not before, never at 0) and the flavor-keyed state round trip.
 */
#include "async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler_channel/doppler_channel_core.h"
#include "dp_dsss_test.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_sym_test.h"
#include "dp_test.h"
#include "gold/gold_core.h" /* SPEC Gold-1023 for the Es/N0-floor sweep   */

#include "wfm/wfm_dsp.h" /* wfm_cont_dsss_chips: the wfmgen C API       */
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A length-7 maximal-length sequence -- same fixture test_dsss_receiver_
 * core.c/test_acq_core.c use for fast, real (not mocked) unit tests. */
static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

/* Stream `x` through `rx` in fixed-size chunks, collecting every emitted
 * symbol; return the symbol count and fill `*syms_out` (caller frees). */
static size_t
_stream (async_dsss_receiver_state_t *rx, const float _Complex *x, size_t n,
         size_t chunk, float _Complex **syms_out)
{
  float _Complex *syms = malloc (n * sizeof *syms); /* generous upper bound */
  size_t          n_syms = 0;
  for (size_t pos = 0; pos < n; pos += chunk)
    {
      size_t take = (pos + chunk <= n) ? chunk : (n - pos);
      n_syms += async_dsss_receiver_steps (rx, x + pos, take, syms + n_syms,
                                           n - n_syms);
    }
  *syms_out = syms;
  return n_syms;
}

/* Best-lag BER over the back half of the recovered symbols against the
 * known data -- mirrors test_dsss_receiver_core.c's own _best_ber(). */
static double
_best_ber (const float _Complex *syms, size_t n_syms, const double *data,
           size_t n_sym)
{
  if (n_syms < 20)
    return 1.0;
  size_t lo = n_syms / 2, hi = n_syms;
  double best = 1.0;
  /* Wide lag search: the acquisition/refine settling delay GROWS as Es/N0
     drops (tens of symbols near the floor), so a narrow window gives false
     "chance" BER at low SNR even when the receiver decodes perfectly -- the
     exact BER fragility the self-referenced EVM/M2M4 validators guard
     against. Over a several-hundred-symbol back half, a spurious sub-0.05
     alignment is statistically impossible, so a wide search stays honest. */
  for (int lag = -250; lag <= 250; lag++)
    {
      size_t errs = 0, cnt = 0;
      for (size_t i = lo; i < hi; i++)
        {
          long di = (long)i + lag;
          if (di < 0 || (size_t)di >= n_sym)
            continue;
          double bit = crealf (syms[i]) > 0 ? 1.0 : -1.0;
          if (bit != data[(size_t)di])
            errs++;
          cnt++;
        }
      if (cnt < (hi - lo) / 2)
        continue;
      double ber = (double)errs / (double)cnt;
      if (ber < best)
        best = ber;
      if (1.0 - ber < best)
        best = 1.0 - ber;
    }
  return best;
}

/* Both truth-free validators live in dp_sym_test.h so every receiver test
 * gets them; see that header for why a BER on its own is not evidence. */

static int
_test_arg_validation (void)
{
  DP_CHECK (async_dsss_receiver_create (NULL, 0, 1e6, 1e3, 2, 2, 55.0, 1e-3,
                                        0.9, 100.0, 4, 8, 0, 0.5, 4, 14.0, 64,
                                        8, false, 100000, 0.0, 0.0)
            == NULL);
  DP_CHECK (async_dsss_receiver_create (CODE7, 7, 0.0, 1e3, 2, 2, 55.0, 1e-3,
                                        0.9, 100.0, 4, 8, 0, 0.5, 4, 14.0, 64,
                                        8, false, 100000, 0.0, 0.0)
            == NULL); /* chip_rate <= 0 */
  DP_CHECK (async_dsss_receiver_create (CODE7, 7, 1e6, 1e3, 2, 3, 55.0, 1e-3,
                                        0.9, 100.0, 4, 8, 0, 0.5, 4, 14.0, 64,
                                        8, false, 100000, 0.0, 0.0)
            == NULL); /* m not in {2,4,8} */
  DP_CHECK (async_dsss_receiver_create (CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3,
                                        0.9, 100.0, 0, 8, 0, 0.5, 4, 14.0, 64,
                                        8, false, 100000, 0.0, 0.0)
            == NULL); /* segments < 1 */
  DP_CHECK (async_dsss_receiver_create (CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3,
                                        0.9, 100.0, 4, 8, 0, 0.5, 4, 14.0, 64,
                                        8, false, 100000, -1.0, 0.0)
            == NULL); /* carrier_freq_hz < 0 */
  DP_CHECK (async_dsss_receiver_create (CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3,
                                        0.9, 100.0, 4, 8, 0, 0.5, 4, 14.0, 64,
                                        8, false, 100000, 0.0, -1.0)
            == NULL); /* lost_confirm_s < 0 */
  DP_CHECK (async_dsss_receiver_create_handoff (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 4, 8, 0, 0.5, 4,
                14.0, 64, 8, false, 100000, 0.0, NAN)
            == NULL); /* lost_confirm_s NaN, hand-off flavor */

  async_dsss_receiver_state_t *rx = async_dsss_receiver_create (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5, 4,
      14.0, 64, 8, false, 100000, 0.0, 0.0);
  DP_CHECK (rx != NULL);
  if (rx)
    {
      DP_CHECK (async_dsss_receiver_get_tracking (rx) == 0);
      DP_CHECK (async_dsss_receiver_get_refining (rx) == 0);
      DP_CHECK (async_dsss_receiver_get_segments (rx) == 4);
      DP_CHECK (async_dsss_receiver_get_sps (rx) == 8);
      /* `n` is MpskReceiver's m_out (terminal outputs per symbol) since the
         cascade rebuild, so it derives to the coherent-bound default rather
         than the retired arm rule's "largest divisor of sps in {4,2,1}". */
      DP_CHECK (async_dsss_receiver_get_n (rx) == MPSK_RX_M_OUT_DEFAULT);
      DP_CHECK (async_dsss_receiver_get_chip_phase (rx) == 0.0);
      DP_CHECK (async_dsss_receiver_get_code_rate (rx) == 1.0);
      async_dsss_receiver_destroy (rx);
    }
  return 0;
}

static int
_test_acquire_and_decode (void)
{

  const size_t sf          = 7;
  const size_t spc         = 4;
  const double fs          = 1.0e6 * (double)spc;
  const double sym_rate    = 35714.29;
  const double tsym        = fs / sym_rate;
  const size_t te          = sf * spc;
  const size_t n_sym       = 400;
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 70.0;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 7,
                   &x, &n, &data);

  async_dsss_receiver_state_t *rx = async_dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }

  float _Complex *syms;
  size_t          n_syms = _stream (rx, x, n, te, &syms);

  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (async_dsss_receiver_get_refining (rx) == 0);
  DP_CHECK (n_syms > 20);
  DP_CHECK (async_dsss_receiver_get_cn0_dbhz_est (rx) > 0.0);

  double ber = _best_ber (syms, n_syms, data, n_sym + 4);
  DP_CHECK (ber < 0.05);
  /* Truth-free corroboration: a real lock, not a lucky BER lag/polarity. */
  DP_CHECK (dp_test_evm_db_hard (syms, n_syms) < -8.0);
  DP_CHECK (dp_test_m2m4_snr_db (syms, n_syms) > 8.0);

  /* ── state-serialization round trip, while tracking ─────────────────── */
  size_t cb   = async_dsss_receiver_state_bytes (rx);
  void  *blob = malloc (cb);
  async_dsss_receiver_get_state (rx, blob);

  async_dsss_receiver_state_t *rx2 = async_dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0);
  DP_CHECK (rx2 != NULL);
  if (rx2)
    {
      DP_CHECK (async_dsss_receiver_set_state (rx2, blob) == DP_OK);
      DP_CHECK (async_dsss_receiver_get_tracking (rx2) == 1);
      DP_CHECK (fabs (async_dsss_receiver_get_chip_phase (rx2)
                      - async_dsss_receiver_get_chip_phase (rx))
                < 1e-9);

      /* a corrupted envelope must be rejected, not reinterpreted. */
      ((char *)blob)[0] ^= (char)0xFF;
      DP_CHECK (async_dsss_receiver_set_state (rx2, blob) == DP_ERR_INVALID);
      async_dsss_receiver_destroy (rx2);
    }
  free (blob);

  /* ── state-serialization round trip, while searching ─────────────────── */
  async_dsss_receiver_state_t *rx3 = async_dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0);
  DP_CHECK (rx3 != NULL);
  if (rx3)
    {
      size_t cb3   = async_dsss_receiver_state_bytes (rx3);
      void  *blob3 = malloc (cb3);
      async_dsss_receiver_get_state (rx3, blob3);

      async_dsss_receiver_state_t *rx4 = async_dsss_receiver_create (
          CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0,
          0.5, 4, 14.0, 32, 8, false, 100000, 0.0, 0.0);
      DP_CHECK (rx4 != NULL);
      if (rx4)
        {
          DP_CHECK (async_dsss_receiver_set_state (rx4, blob3) == DP_OK);
          DP_CHECK (async_dsss_receiver_get_tracking (rx4) == 0);
          DP_CHECK (async_dsss_receiver_get_refining (rx4) == 0);
          async_dsss_receiver_destroy (rx4);
        }
      free (blob3);
      async_dsss_receiver_destroy (rx3);
    }

  /* ── reset() returns to searching ─────────────────────────────────────── */
  async_dsss_receiver_reset (rx);
  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 0);
  DP_CHECK (async_dsss_receiver_get_refining (rx) == 0);
  DP_CHECK (async_dsss_receiver_get_chip_phase (rx) == 0.0);

  free (syms);
  free (x);
  free (data);
  async_dsss_receiver_destroy (rx);
  return 0;
}

/* The refine stage's own give-up cap: CarrierAcquisition cannot possibly
 * reach a detection off a SINGLE block (`refine_max_n_blocks=1`, `sequential
 * =true` so this is genuinely the CFAR test's own give-up bound, not
 * dwell_target's separate fixed-wait count) regardless of how strong the
 * underlying signal is -- so this is a direct, deterministic test of the
 * object's give-up path itself (task #99's design doc: "never stalls the
 * receiver forever waiting on a refinement that won't arrive"), verified
 * empirically (a Python probe across max_n_blocks in {1,2,3} confirmed
 * exactly 1 forces give-up on this fixture; >=2 lets a real signal reach
 * ready instead). */
static int
_test_give_up_cap (void)
{

  const size_t sf          = 7;
  const size_t spc         = 4;
  const double fs          = 1.0e6 * (double)spc;
  const double sym_rate    = 35714.29;
  const double tsym        = fs / sym_rate;
  const size_t te          = sf * spc;
  const size_t n_sym       = 300;
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 70.0;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 9,
                   &x, &n, &data);

  async_dsss_receiver_state_t *rx = async_dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 16, 4, true, 1 /* refine_max_n_blocks: forces give-up */,
      0.0 /* carrier_freq_hz: aiding off */, 0.0);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }

  float _Complex *syms; /* _stream() allocates it; see its three siblings */
  size_t          n_syms = _stream (rx, x, n, te, &syms);

  DP_CHECK (async_dsss_receiver_get_refining (rx) == 0);
  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);
  /* Give-up: doppler estimate stays the UNREFINED coarse handoff value --
   * 0.0 Hz here, since _make_signal() injects no real Doppler offset. */
  DP_CHECK (fabs (async_dsss_receiver_get_doppler_hz (rx) - 0.0) < 1e-6);
  /* Still tracking/decoding despite the unrefined seed -- the give-up
   * path must not otherwise break the object. */
  DP_CHECK (n_syms > 20);

  free (syms);
  free (x);
  free (data);
  async_dsss_receiver_destroy (rx);
  return 0;
}

/* The full Acquisition -> CarrierAcquisition refine -> Costas/Dll/
 * RateConverter/MpskReceiver chain, at SPEC's own real operating-point
 * RATIOS (chip_rate=3.069e6, code_len=1023, symbol_rate=2700 -- CCSDS
 * Gold-1023 @ 3.069 Mcps / async BPSK @ 2700 bps) against a 500 Hz/s
 * Doppler ramp (SPEC's corrected worst case), run once at a moderately
 * stressed Es/N0 where the shared MpskReceiver/Dll chain is known able
 * to lock at all (see _test_spec_combined_scenario_at_spec_floor()'s own
 * comment for why SPEC's literal 5dB floor is NOT used here). Proves
 * THIS object's own new machinery -- the refine stage's frequency
 * estimate and the per-code-period (not per-partial -- see
 * adr_build_track_chain()'s comment) carrier cadence -- correctly closes
 * the loop and decodes under a real Doppler RAMP, the scenario the
 * reverted C-port attempt (FINISHING_PLAN.md's "C port attempt #1")
 * never got this far with. */
static int
_test_spec_ramp_decode (void)
{

  const size_t sf        = 1023;
  const size_t spc       = 2;
  const double chip_rate = 3.069e6;
  const double fs        = chip_rate * (double)spc;
  const double sym_rate  = 2700.0; /* chip_rate/(sf*sym_rate) ~= 1.111
                                       periods/symbol -- deliberately NOT
                                       an integer, the genuinely-async
                                       clock relationship SPEC's own
                                       waveform has. */
  const double tsym          = fs / sym_rate;
  const size_t te            = sf * spc;
  const double rate_hz_per_s = 500.0;
  const size_t n_sym         = 2430;
  const size_t pre_silence   = te * 5 + 3;
  const double esn0_db       = 30.0; /* moderately stressed -- see the
                                         comment above; not SPEC's own
                                         5dB floor. */
  const double cn0
      = esn0_db + 10.0 * log10 (sym_rate); /* es_n0_to_cn0_dbhz() */

  uint8_t *code = malloc (sf);
  uint32_t cst  = 13;
  for (size_t i = 0; i < sf; i++)
    code[i] = (uint8_t)(dp_bit (&cst) > 0 ? 0u : 1u);

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_ramp_capture (code, sf, spc, fs, tsym, rate_hz_per_s, cn0, n_sym,
                        pre_silence, 21, &x, &n, &data);

  async_dsss_receiver_state_t *rx = async_dsss_receiver_create (
      code, sf, chip_rate, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0,
      0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (code);
      free (x);
      free (data);
      return 1;
    }

  float _Complex *syms;
  size_t          n_syms = _stream (rx, x, n, te, &syms);

  double ber = _best_ber (syms, n_syms, data, n_sym + 4);

  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (n_syms > (n_sym / 2));
  DP_CHECK (ber < 0.05);
  /* Truth-free corroboration under the ramp: a real lock, not a lucky lag. */
  DP_CHECK (dp_test_evm_db_hard (syms, n_syms) < -8.0);
  DP_CHECK (dp_test_m2m4_snr_db (syms, n_syms) > 8.0);

  free (syms);
  free (x);
  free (data);
  free (code);
  async_dsss_receiver_destroy (rx);
  return 0;
}

/* Both hand-overs under a CLOCK OFFSET, at two operating points.
 *
 * doppler#1249, the refine -> track hand-over: the live chain used to
 * start from the seed's code phase, rounded to whole code periods -- zero
 * net advance only on an undilated clock. At 20 ppm the code runs
 * 100 chips/s ahead, 4 chips over the 42 ms refine the floor's C/N0
 * sizes, and a Dll seeded 4 chips off never pulls in: the receiver
 * reported tracking, code lock never came, and after `lost_confirm_s` it
 * released an emitter that was there all along. The hand-over now
 * advances the seed's phase by the refined Doppler's dilation over the
 * refine's whole periods. Sabotage: seed the live chain with the original
 * phase -> every seed red.
 *
 * doppler#1254, the search -> refine hand-off, one hand-over earlier: the
 * searcher decides a hit on a non-coherent sum over its dwell, and the
 * code phase it reports is that sum's peak -- the phase at the MIDDLE of
 * the dwell -- while the seed is applied at its end. At 45 dB-Hz the dwell
 * is 15 epochs (0.15 chip of drift, inside the refine Dll's pull-in); at
 * the 40 dB-Hz floor it is 88 epochs, 0.9 chip, past it: the refine Dll
 * sat where it was seeded, its detector saw no lobe, and the hand-over
 * was the unrefined seed, 1.1 kHz off, which the carrier loop can never
 * acquire. acq_build_handoff() now advances the phase by the drift over
 * half the dwell. Sabotage: pass 0.0 for the carrier at the receiver's
 * call site -> the floor's seeds red (the 45 dB-Hz ones survive it, which
 * is why the floor is in the test).
 *
 * The stimulus is the shipped channel at SPEC's 20 ppm of a 2.5 GHz
 * carrier with no rate (SPEC's two worst cases do not coincide); three
 * seeds closing and one opening, every one must lock the code and decode,
 * at 45 dB-Hz with the margin the old test used and at the floor with the
 * shipped one. */
static int
_test_handover_under_clock_offset (void)
{
  const size_t sf        = 1023;
  const size_t spc       = 2;
  const double chip_rate = 5.0e6;
  const double fs        = chip_rate * (double)spc;
  const double sym_rate  = 2700.0;
  const double tsym      = fs / sym_rate;
  const size_t te        = sf * spc;
  const double carrier   = 2.5e9;
  /* SPEC's 20 ppm: 50 kHz, 100 chips/s of dilation. Two operating points:
     45 dB-Hz with a design margin of 19 dB, which sizes the same 7-block,
     42 ms refine dwell that the shipped 14 dB does at 40 dB-Hz
     (validate_refine_bias prints the margin -> dwell table) -- so #1249's
     old hand-over is 4 chips off, unambiguously outside the Dll's pull-in
     (over its 12 ms dwell it was 1.2 chips, a coin toss) -- and the
     40 dB-Hz floor itself with the shipped margin, where the searcher's
     dwell is long enough for #1254's smear to bite. */
  const double ppm         = 20.0;
  const size_t n_sym       = 2700; /* one second */
  const size_t pre_silence = te * 5 + 3;
  const double cn0s[2]     = { 45.0, 40.0 }; /* Es/N0 10.7 and 5.7 dB */
  const double margins[2]  = { 19.0, 14.0 };

  uint8_t *code = malloc (sf);
  uint32_t cst  = 13;
  for (size_t i = 0; i < sf; i++)
    code[i] = (uint8_t)(dp_bit (&cst) > 0 ? 0u : 1u);

  /* Three seeds closing (the code clock fast, the phase advancing), one
     opening (negative ppm: the clock slow, the phase retreating), at each
     operating point. */
  int decoded = 0;
  for (uint32_t trial = 0; trial < 8; trial++)
    {
      const uint32_t  seed      = 100 + trial % 4;
      const double    cn0       = cn0s[trial / 4];
      const double    margin_db = margins[trial / 4];
      const double    sign      = seed == 103 ? -1.0 : 1.0;
      float _Complex *x;
      size_t          n;
      double         *data;
      dp_dsss_dilated_capture (code, sf, spc, fs, tsym, carrier, sign * ppm,
                               0.0, cn0, n_sym, pre_silence, seed, &x, &n,
                               &data);
      async_dsss_receiver_state_t *rx = async_dsss_receiver_create (
          code, sf, chip_rate, sym_rate, spc, 2, cn0, 1e-2, 0.9,
          1.2 * ppm * 1e-6 * carrier, 4, 8, 0, 0.5, 4, margin_db, 64, 8, false,
          100000, carrier, 0.0);
      DP_REQUIRE (rx != NULL);
      float _Complex *syms;
      size_t          n_syms = _stream (rx, x, n, te, &syms);
      double          ber    = _best_ber (syms, n_syms, data, n_sym + 4);
      printf ("  hand-over at %+.0f ppm, %.0f dB-Hz, seed %u: tracking %d, "
              "code %d, symbol %d, %zu symbols, BER %.3f, Doppler est "
              "%.0f Hz (truth %.0f), chip %.2f\n",
              sign * ppm, cn0, seed, async_dsss_receiver_get_tracking (rx),
              async_dsss_receiver_get_code_locked (rx),
              async_dsss_receiver_get_locked (rx), n_syms, ber,
              async_dsss_receiver_get_doppler_hz (rx),
              sign * ppm * 1e-6 * carrier,
              async_dsss_receiver_get_chip_phase (rx));
      /* The hand-over's own claim, per seed: the live chain locks the
         dilated code. The carrier and the decode ride on the refine's
         Doppler; on the shipped look-back (0.5 dB, eleven dumps per
         epoch) it lands within tens of Hz and every seed decodes. On
         the retired 100 dB look-back this test used to pass, the
         estimate keeps a third of the seed's error (#1252, measured by
         validate_refine_bias) and only a majority decoded. */
      DP_CHECK_MSG (async_dsss_receiver_get_tracking (rx) == 1
                        && async_dsss_receiver_get_code_locked (rx) == 1,
                    "the live chain locks the dilated code from the "
                    "hand-over");
      decoded += async_dsss_receiver_get_locked (rx) == 1 && n_syms > n_sym / 2
                 && ber < 0.05;
      free (syms);
      free (x);
      free (data);
      async_dsss_receiver_destroy (rx);
    }
  DP_CHECK_MSG (decoded == 8, "and the carrier locks and decodes on "
                              "every seed at both operating points");
  free (code);
  return 0;
}

/* SPEC's own literal Es/N0=5dB floor, same geometry/ramp as
 * _test_spec_ramp_decode() above. task #99's own cliff at this exact
 * operating point was ALREADY characterized as "pure-SNR, rate-
 * independent" with "Acquisition's own hit quality/reliability... the
 * leading remaining candidate, not yet directly inspected" (FINISHING_
 * PLAN.md). Direct measurement while building this object CONFIRMED that
 * diagnosis and ruled out this object's own refine stage as the fix:
 * the ALREADY-SHIPPED, already-validated `DsssReceiver` fails to decode
 * (BER~0.43, lock~0.55) at this exact Es/N0 even given a trivial STATIC
 * ZERO Doppler offset -- no frequency estimation error at all, coarse or
 * refined. So this object cannot be expected to decode at SPEC's literal
 * floor either (task #99's real fix is elsewhere -- see the comment
 * above); this test instead checks the two things THIS object's own
 * refine stage is actually responsible for: the state machine reaches
 * tracking (doesn't stall) and the refined Doppler estimate is finite/
 * sane (not NaN/garbage), not full decode. */
static int
_test_spec_combined_scenario_at_spec_floor (void)
{

  const size_t sf            = 1023;
  const size_t spc           = 2;
  const double chip_rate     = 3.069e6;
  const double fs            = chip_rate * (double)spc;
  const double sym_rate      = 2700.0;
  const double tsym          = fs / sym_rate;
  const size_t te            = sf * spc;
  const double rate_hz_per_s = 500.0;
  const size_t n_sym         = 2430;
  const size_t pre_silence   = te * 5 + 3;
  const double esn0_db       = 5.0; /* SPEC's own floor */
  const double cn0
      = esn0_db + 10.0 * log10 (sym_rate); /* es_n0_to_cn0_dbhz() */

  uint8_t *code = malloc (sf);
  uint32_t cst  = 13;
  for (size_t i = 0; i < sf; i++)
    code[i] = (uint8_t)(dp_bit (&cst) > 0 ? 0u : 1u);

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_ramp_capture (code, sf, spc, fs, tsym, rate_hz_per_s, cn0, n_sym,
                        pre_silence, 21, &x, &n, &data);

  async_dsss_receiver_state_t *rx = async_dsss_receiver_create (
      code, sf, chip_rate, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0,
      0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (code);
      free (x);
      free (data);
      return 1;
    }

  float _Complex *syms;
  size_t          n_syms = _stream (rx, x, n, te, &syms);
  (void)n_syms;

  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);
  double dh = async_dsss_receiver_get_doppler_hz (rx);
  DP_CHECK (isfinite (dh));

  free (syms);
  free (x);
  free (data);
  free (code);
  async_dsss_receiver_destroy (rx);
  return 0;
}

/* AWGN-only Es/N0 decode floor at SPEC's own geometry (Gold-1023, 3.069
 * Mcps, 2700 bps asynchronous BPSK, spc=2), ZERO Doppler -- characterizes
 * the decode floor, independent of Doppler and of the aiding path.
 * Generated with the wfmgen C API (wfm_cont_dsss_chips) so this test
 * exercises the same continuous-DSSS builder the wfmgen tool ships. Sweeps
 * Es/N0 and prints three metrics (visible under `ctest -V`) so the floor is
 * a tracked, inspectable quantity: the truth-referenced BER PLUS two
 * truth-free validators that cannot be fooled by a lucky BER lag/polarity --
 * the self-referenced EVM (each symbol vs its OWN hard decision, no lag) and
 * the blind M2M4 SNR (snr_m2m4_db, moment-based, no reference symbols).
 * AWGN-only, this receiver essentially MEETS SPEC's 5 dB floor: it decodes
 * cleanly at 5 dB and only fails at 4 dB. (An earlier "~12 dB floor" was a
 * pure BER-lag artifact -- the settling delay grows toward the floor, so a
 * fixed short lag window reports chance while the receiver actually decodes;
 * the EVM/M2M4 corroboration is what caught and corrected it.)
 */
static int
_test_awgn_esn0_floor (void)
{
  const size_t sf = 1023, spc = 2;
  const double chip_rate = 3.069e6, sym_rate = 2700.0;
  const double fs    = chip_rate * (double)spc;
  const double cps   = chip_rate / sym_rate; /* 1136.67, non-integer */
  const size_t n_sym = 1500;

  uint8_t      *code = malloc (sf);
  gold_state_t *g    = gold_create (934, 350, 567, 73, 10);
  gold_generate (g, sf, code, sf);
  gold_destroy (g);
  size_t ones = 0;
  for (size_t i = 0; i < sf; i++)
    ones += code[i];
  DP_CHECK (ones > 480 && ones < 544); /* a valid Gold-1023 is ~balanced */

  printf ("  AWGN-only Es/N0 floor (Gold-1023, 3.069 Mcps, 2700 bps, "
          "no Doppler):\n");
  const double esn0_pts[] = { 4.0, 5.0, 6.0, 8.0, 10.0 };
  const size_t n_pts      = sizeof esn0_pts / sizeof esn0_pts[0];
  double       ber_at[5]  = { 1, 1, 1, 1, 1 };
  double       evm_at[5]  = { 0, 0, 0, 0, 0 };
  double       snr_at[5]  = { 0, 0, 0, 0, 0 };

  for (size_t p = 0; p < n_pts; p++)
    {
      double   cn0    = esn0_pts[p] + 10.0 * log10 (sym_rate);
      size_t   n_data = n_sym + 8;
      uint8_t *dbits  = malloc (n_data);
      double  *dsym   = malloc (n_data * sizeof *dsym);
      uint32_t st     = 0x51ced00du + (uint32_t)p;
      for (size_t i = 0; i < n_data; i++)
        {
          dbits[i] = (uint8_t)(dp_xs32 (&st) & 1u);
          dsym[i]  = 1.0 - 2.0 * (double)dbits[i]; /* transmitted BPSK sym */
        }

      size_t   n_chips = (size_t)((double)n_sym * cps) + 2 * sf;
      uint8_t *chips   = malloc (n_chips);
      wfm_cont_dsss_chips (code, sf, dbits, n_data, cps, n_chips, chips);

      size_t          pre   = sf * spc * 5 + 3;
      size_t          n     = n_chips * spc;
      size_t          tot   = pre + n;
      float _Complex *x     = calloc (tot, sizeof *x);
      double          amp   = sqrt (pow (10.0, cn0 / 10.0) / fs);
      double          sigma = 1.0 / amp;
      for (size_t i = 0; i < tot; i++)
        x[i] = (float _Complex) (sigma / sqrt (2.0)) * dp_cgauss (&st);
      for (size_t i = 0; i < n; i++)
        x[pre + i] += (float)(1.0 - 2.0 * (double)chips[i / spc]);

      async_dsss_receiver_state_t *rx = async_dsss_receiver_create (
          code, sf, chip_rate, sym_rate, spc, 2, cn0, 1e-3, 0.9, 100.0, 4, 8,
          0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0);
      float _Complex *syms   = NULL;
      size_t          n_syms = rx ? _stream (rx, x, tot, sf * spc, &syms) : 0;
      double          ber    = _best_ber (syms, n_syms, dsym, n_data);
      double evm = dp_test_evm_db_hard (syms, n_syms); /* no lag/truth */
      double snr = dp_test_m2m4_snr_db (syms, n_syms); /* blind M2M4   */
      ber_at[p]  = ber;
      evm_at[p]  = evm;
      snr_at[p]  = snr;
      /* Print all three: ber is the truth-referenced number; evm (self-
         referenced, no lag) and the blind M2M4 SNR are the independent
         validators that can't be fooled by a lucky-lag false pass. */
      printf ("    Es/N0=%4.1f dB  cn0=%5.1f dB-Hz  tracking=%d  ber=%.4f  "
              "evm=%6.1f dB  m2m4_snr=%5.1f dB\n",
              esn0_pts[p], cn0, rx ? async_dsss_receiver_get_tracking (rx) : 0,
              ber, evm, snr);

      free (syms);
      async_dsss_receiver_destroy (rx);
      free (x);
      free (chips);
      free (dsym);
      free (dbits);
    }
  free (code);

  /* AWGN-only, this receiver essentially MEETS SPEC's 5 dB floor: it decodes
     cleanly at 5 dB and only fails at 4 dB. (An earlier characterization put
     the floor ~12 dB -- that was purely a narrow BER lag-search artifact:
     the settling delay grows toward the floor, so a fixed short lag window
     misses the alignment and reports chance even while the receiver decodes
     perfectly. Widening the lag AND cross-checking with the truth-free
     EVM/M2M4 validators is what corrected it.) */
  DP_CHECK (ber_at[4] < 0.05); /* 10 dB: clean */
  DP_CHECK (ber_at[3] < 0.05); /* 8 dB: clean */
  DP_CHECK (ber_at[1] < 0.05); /* 5 dB: MEETS SPEC's 5 dB floor (AWGN-only) */
  DP_CHECK (ber_at[0] > 0.10); /* 4 dB: below the floor */

  /* Independent, truth-free corroboration (no lag, no reference symbols):
     the self-referenced EVM and blind M2M4 SNR must AGREE with the BER
     verdict and degrade monotonically toward the floor -- this is what makes
     the floor call robust rather than a BER-lag artifact. A locked
     constellation is tight (EVM ~ -Es/N0) with real symbol SNR; toward the
     floor both worsen. */
  DP_CHECK (evm_at[4] < -8.0); /* 10 dB locked: tight */
  DP_CHECK (snr_at[4] > 10.0); /* 10 dB locked: real symbol SNR */
  DP_CHECK (evm_at[0]
            > evm_at[3] + 3.0); /* 4 dB EVM >= 3 dB worse than 8 dB */
  DP_CHECK (snr_at[0]
            < snr_at[3] - 2.0); /* 4 dB blind SNR collapses vs 8 dB */
  return 0;
}

/* Exercise every read-only accessor and the three raw sub-loop reconfigure
 * entry points (search grid / lock detector / track chain), including the
 * chain-reconfigure rejection paths. These are thin delegations the algorithm
 * tests above never call, so they carry no signal dependence — a freshly
 * created (pre-lock) receiver reaches all of them. */
/* Noise alone at the capture's own sigma for `cn0_dbhz` -- the emitter
 * switched off, the channel still there. */
static float _Complex *
_noise_tail (size_t n, double fs, double cn0_dbhz, uint32_t seed)
{
  float _Complex *x     = malloc (n * sizeof *x);
  double          sigma = 1.0 / sqrt (pow (10.0, cn0_dbhz / 10.0) / fs);
  uint32_t        st    = seed;
  for (size_t i = 0; i < n; i++)
    x[i] = (float _Complex) (sigma / sqrt (2.0)) * dp_cgauss (&st);
  return x;
}

/* The CODE7 fixture's hand-off receiver: every number the searching-flavor
 * tests use, minus the search half-range, plus the release interval. */
static async_dsss_receiver_state_t *
_handoff_rx (double cn0, double lost_confirm_s)
{
  return async_dsss_receiver_create_handoff (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, cn0, 1e-2, 0.9, 4, 8, 0, 0.5, 4, 14.0,
      32, 8, false, 100000, 0.0, lost_confirm_s);
}

/* Hand-off mode, section 11.1: no search of its own. Idle consumes and
 * discards; a seed -- here the truth, since the capture puts chip 0 on its
 * first signal sample with no Doppler -- starts the refine -> track chain
 * the searching flavor runs; a second seed is refused until reset(), which
 * returns to idle, and the object decodes again from the next seed. */
static int
_test_handoff_seed_and_decode (void)
{
  const size_t sf = 7, spc = 4;
  const double fs          = 1.0e6 * (double)spc;
  const double sym_rate    = 35714.29;
  const double tsym        = fs / sym_rate;
  const size_t te          = sf * spc;
  const size_t n_sym       = 400;
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 70.0;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 7,
                   &x, &n, &data);

  async_dsss_receiver_state_t *rx = _handoff_rx (cn0, 0.0);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }
  DP_CHECK (async_dsss_receiver_get_idle (rx) == 1);
  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 0);
  DP_CHECK (async_dsss_receiver_get_refining (rx) == 0);
  DP_CHECK (async_dsss_receiver_get_lost (rx) == 0);
  DP_CHECK (rx->acq == NULL); /* no engine was built */
  DP_CHECK (async_dsss_receiver_configure_search_raw (rx, 1, 1) == -1);

  /* Idle: the whole capture -- the emitter included, at chip 0 -- fed in
   * odd-sized blocks, changes nothing and emits nothing; a receiver that
   * were quietly running its chain would emit symbols from the signal. */
  float _Complex tmp[1021];
  size_t n_idle = 0;
  for (size_t pos = 0; pos < n; pos += 1021)
    {
      size_t take = (pos + 1021 <= n) ? 1021 : n - pos;
      n_idle += async_dsss_receiver_steps (rx, x + pos, take, tmp, 1021);
    }
  DP_CHECK (n_idle == 0);
  DP_CHECK (async_dsss_receiver_get_idle (rx) == 1);
  DP_CHECK (async_dsss_receiver_get_chip_phase (rx) == 0.0);
  DP_CHECK (rx->state_samples == (uint64_t)n);

  DP_CHECK (async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  DP_CHECK (async_dsss_receiver_get_idle (rx) == 0);
  DP_CHECK (async_dsss_receiver_get_refining (rx) == 1);
  DP_CHECK (async_dsss_receiver_get_doppler_hz (rx) == 0.0);
  DP_CHECK (async_dsss_receiver_get_cn0_dbhz_est (rx) == cn0);
  /* Assigned once: refused while refining ... */
  DP_CHECK (async_dsss_receiver_seed (rx, 1.0, 0.0, cn0) == DP_ERR_INVALID);
  DP_CHECK (async_dsss_receiver_get_refining (rx) == 1);
  DP_CHECK (async_dsss_receiver_get_doppler_hz (rx) == 0.0);

  float _Complex *syms;
  size_t n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (n_syms > 20);
  DP_CHECK (_best_ber (syms, n_syms, data, n_sym + 4) < 0.05);
  DP_CHECK (dp_test_evm_db_hard (syms, n_syms) < -8.0);
  DP_CHECK (dp_test_m2m4_snr_db (syms, n_syms) > 8.0);
  /* ... and while tracking. */
  DP_CHECK (async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_ERR_INVALID);
  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);
  free (syms);

  /* reset() is the release: back to idle, not to a search it has not got,
   * and the same object takes its next seed. */
  async_dsss_receiver_reset (rx);
  DP_CHECK (async_dsss_receiver_get_idle (rx) == 1);
  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 0);
  DP_CHECK (async_dsss_receiver_get_chip_phase (rx) == 0.0);
  DP_CHECK (async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (_best_ber (syms, n_syms, data, n_sym + 4) < 0.05);
  free (syms);

  free (x);
  free (data);
  async_dsss_receiver_destroy (rx);
  return 0;
}

/* seed() is a method of the searching flavor too -- an outside hit beats
 * its own search and skips it -- and it checks its arguments: the phase is
 * a code phase, so [0, code_len), and nothing non-finite. */
static int
_test_seed_on_searching_flavor (void)
{
  const size_t sf = 7, spc = 4;
  const double fs          = 1.0e6 * (double)spc;
  const double sym_rate    = 35714.29;
  const double tsym        = fs / sym_rate;
  const size_t te          = sf * spc;
  const size_t n_sym       = 400;
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 70.0;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 7,
                   &x, &n, &data);

  async_dsss_receiver_state_t *rx = async_dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }
  DP_CHECK (async_dsss_receiver_get_idle (rx) == 0); /* searching, not idle */

  /* Range: the code has 7 chips, so 7.0 is one past the end. */
  DP_CHECK (async_dsss_receiver_seed (rx, 7.0, 0.0, cn0) == DP_ERR_INVALID);
  DP_CHECK (async_dsss_receiver_seed (rx, -0.5, 0.0, cn0) == DP_ERR_INVALID);
  DP_CHECK (async_dsss_receiver_seed (rx, NAN, 0.0, cn0) == DP_ERR_INVALID);
  DP_CHECK (async_dsss_receiver_seed (rx, 0.0, INFINITY, cn0)
            == DP_ERR_INVALID);
  DP_CHECK (async_dsss_receiver_get_refining (rx) == 0); /* untouched */

  DP_CHECK (async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  DP_CHECK (async_dsss_receiver_get_refining (rx) == 1);

  float _Complex *syms;
  size_t n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (n_syms > 20);
  DP_CHECK (_best_ber (syms, n_syms, data, n_sym + 4) < 0.05);
  DP_CHECK (dp_test_evm_db_hard (syms, n_syms) < -8.0);

  /* reset() on this flavor is still the search. */
  async_dsss_receiver_reset (rx);
  DP_CHECK (async_dsss_receiver_get_idle (rx) == 0);
  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 0);

  free (syms);
  free (x);
  free (data);
  async_dsss_receiver_destroy (rx);
  return 0;
}

/* Feed `x` in `chunk`-sample blocks to a tracking receiver, keeping the
 * release clock's own book: the longest run of both flags down, in samples,
 * at the moment lost first reads 1 (or at the end). Returns 1 if lost
 * fired. */
static int
_feed_until_lost (async_dsss_receiver_state_t *rx, const float _Complex *x,
                  size_t n, size_t chunk, uint64_t *both_down_run_at_lost,
                  uint64_t *fed_at_lost)
{
  float _Complex *tmp  = malloc (chunk * sizeof *tmp);
  uint64_t        run  = 0;
  uint64_t        fed  = 0;
  int             lost = 0;
  for (size_t pos = 0; pos < n && !lost; pos += chunk)
    {
      size_t take = (pos + chunk <= n) ? chunk : n - pos;
      (void)async_dsss_receiver_steps (rx, x + pos, take, tmp, chunk);
      fed += take;
      lost = async_dsss_receiver_get_lost (rx);
      /* Read AFTER the call, as the object does; a lost receiver's flags
       * are frozen where they were. */
      if (async_dsss_receiver_get_code_locked (rx)
          || async_dsss_receiver_get_locked (rx))
        run = 0;
      else
        run += take;
    }
  free (tmp);
  *both_down_run_at_lost = run;
  *fed_at_lost           = fed;
  return lost;
}

/* The release rule, section 11.2: both flags down, without a break, for
 * longer than lost_confirm_s -> lost; not a sample before; never at 0; and
 * lost is inert until reset(), which hands the object back idle. */
static int
_test_lost_after_switch_off (void)
{
  const size_t sf = 7, spc = 4;
  const double fs          = 1.0e6 * (double)spc;
  const double sym_rate    = 35714.29;
  const double tsym        = fs / sym_rate;
  const size_t te          = sf * spc;
  const size_t n_sym       = 400;
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 70.0;
  const double confirm_s   = 0.02;                /* 80 000 samples at fs */
  const size_t tail_n      = (size_t)(0.25 * fs); /* 0.25 s of "off"  */
  const size_t chunk       = 1024;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 7,
                   &x, &n, &data);
  float _Complex *off = _noise_tail (tail_n, fs, cn0, 99);

  /* Three receivers on the same capture: the rule armed, the rule off, and
   * the rule armed past the tail's length. */
  const double intervals[3] = { confirm_s, 0.0, 10.0 };
  const int    expect[3]    = { 1, 0, 0 };
  for (int k = 0; k < 3; k++)
    {
      async_dsss_receiver_state_t *rx = _handoff_rx (cn0, intervals[k]);
      DP_CHECK (rx != NULL);
      if (!rx)
        continue;
      DP_CHECK (async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
      float _Complex *syms;
      size_t          n_syms
          = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
      free (syms);
      DP_CHECK (n_syms > 20);
      DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);
      DP_CHECK (async_dsss_receiver_get_code_locked (rx) == 1);
      DP_CHECK (async_dsss_receiver_get_locked (rx) == 1);
      DP_CHECK (async_dsss_receiver_get_lost (rx) == 0);

      uint64_t run = 0, fed = 0;
      int      lost = _feed_until_lost (rx, off, tail_n, chunk, &run, &fed);
      DP_CHECK (lost == expect[k]);
      if (k == 0)
        {
          /* Not a sample early: the run of both-down at the transition is
           * longer than the interval, and the interval is what it was
           * asked to be. */
          DP_CHECK (rx->lost_confirm_samples
                    == (uint64_t)llround (confirm_s * fs));
          DP_CHECK (run > rx->lost_confirm_samples);
          DP_CHECK (fed < (uint64_t)tail_n); /* well inside the tail */
          DP_CHECK (async_dsss_receiver_get_tracking (rx) == 0);

          /* Lost is inert: samples are discarded, a seed is refused, the
           * blob carries the state, and reset() gives the object back. */
          float _Complex tmp[64];
          double chip_before = async_dsss_receiver_get_chip_phase (rx);
          DP_CHECK (
              async_dsss_receiver_steps (rx, x + pre_silence, 64, tmp, 64)
              == 0);
          DP_CHECK (async_dsss_receiver_get_lost (rx) == 1);
          DP_CHECK (async_dsss_receiver_get_chip_phase (rx) == chip_before);
          DP_CHECK (async_dsss_receiver_seed (rx, 0.0, 0.0, cn0)
                    == DP_ERR_INVALID);

          size_t cb   = async_dsss_receiver_state_bytes (rx);
          void  *blob = malloc (cb);
          async_dsss_receiver_get_state (rx, blob);
          async_dsss_receiver_state_t *rx2 = _handoff_rx (cn0, intervals[k]);
          DP_CHECK (rx2 != NULL);
          if (rx2)
            {
              DP_CHECK (async_dsss_receiver_set_state (rx2, blob) == DP_OK);
              DP_CHECK (async_dsss_receiver_get_lost (rx2) == 1);
              async_dsss_receiver_destroy (rx2);
            }
          free (blob);

          async_dsss_receiver_reset (rx);
          DP_CHECK (async_dsss_receiver_get_lost (rx) == 0);
          DP_CHECK (async_dsss_receiver_get_idle (rx) == 1);
          DP_CHECK (async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
        }
      else
        {
          /* Still tracking, on noise: the rule did not fire. */
          DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);
          DP_CHECK (async_dsss_receiver_get_lost (rx) == 0);
        }
      async_dsss_receiver_destroy (rx);
    }

  free (off);
  free (x);
  free (data);
  return 0;
}

/* One flag down is a degrade, not a release (section 11.2): with the code
 * detector pinned to a threshold no statistic reaches, code lock drops on a
 * healthy signal while symbol lock holds -- and the release clock, which
 * needs BOTH down, must never run out. */
static int
_test_one_flag_down_is_a_degrade (void)
{
  const size_t sf = 7, spc = 4;
  const double fs          = 1.0e6 * (double)spc;
  const double sym_rate    = 35714.29;
  const double tsym        = fs / sym_rate;
  const size_t te          = sf * spc;
  const size_t n_sym       = 4800; /* ~0.12 s of signal after the split  */
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 70.0;
  const double confirm_s   = 0.005; /* the 0.12 s that follow are 24x it */

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 7,
                   &x, &n, &data);

  async_dsss_receiver_state_t *rx = _handoff_rx (cn0, confirm_s);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }
  DP_CHECK (async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  const size_t    split = pre_silence + (size_t)(400.0 * tsym);
  float _Complex *syms;
  size_t          n_syms
      = _stream (rx, x + pre_silence, split - pre_silence, te, &syms);
  free (syms);
  DP_CHECK (n_syms > 20);
  DP_CHECK (async_dsss_receiver_get_code_locked (rx) == 1);
  DP_CHECK (async_dsss_receiver_get_locked (rx) == 1);

  /* Pin the code detector out of reach: it drops within its own down-count
   * and never comes back, while the symbol detector is untouched. */
  async_dsss_receiver_configure_lock_raw (rx, 1e30, 1e30, 8, 0.1, 1, 1);

  size_t code_down_blocks = 0, sym_up_blocks = 0, blocks = 0;
  float _Complex tmp[1024];
  for (size_t pos = split; pos < n; pos += 1024)
    {
      size_t take = (pos + 1024 <= n) ? 1024 : n - pos;
      (void)async_dsss_receiver_steps (rx, x + pos, take, tmp, 1024);
      blocks++;
      code_down_blocks += !async_dsss_receiver_get_code_locked (rx);
      sym_up_blocks += async_dsss_receiver_get_locked (rx);
      DP_CHECK (async_dsss_receiver_get_lost (rx) == 0);
      if (async_dsss_receiver_get_lost (rx))
        break;
    }
  /* The premise held: code lock was down for nearly all of it and symbol
   * lock up for all of it, over many confirm intervals. */
  DP_CHECK ((double)(n - split) / fs > 5.0 * confirm_s);
  DP_CHECK (code_down_blocks > blocks * 9 / 10);
  DP_CHECK (sym_up_blocks == blocks);
  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);

  free (x);
  free (data);
  async_dsss_receiver_destroy (rx);
  return 0;
}

/* The blob is keyed by flavor: a hand-off receiver's state resumes
 * bit-for-bit into another hand-off receiver, and neither flavor accepts
 * the other's blob (the search engine is in one and not the other). */
/* #1265: the refine's dwell is floored at refine_min_blocks whatever the
   detection sizing asks. At 45 dB-Hz with the shipped margin det_n_noncoh
   sizes two blocks -- 210 Hz of estimate noise, one hand-over in sixty
   outside the chain's pull-in -- and the floor makes it seven (77 Hz).
   Sabotage: drop the floor in adr_build_refine_chain() -> the default
   receiver's dwell reads 2 -> red. */
static int
_test_refine_dwell_floor (void)
{
  uint8_t  code[1023];
  uint32_t cst = 13;
  for (size_t i = 0; i < 1023; i++)
    code[i] = (uint8_t)(dp_bit (&cst) > 0 ? 0u : 1u);
  async_dsss_receiver_state_t *rx = async_dsss_receiver_create_handoff (
      code, 1023, 5.0e6, 2700.0, 2, 2, 45.0, 1e-3, 0.9, 4, 8, 0, 0.5, 4, 14.0,
      64, 8, false, 100000, 2.5e9, 2.0);
  DP_REQUIRE (rx != NULL);
  DP_CHECK (rx->refine_min_blocks == ASYNC_DSSS_RX_REFINE_MIN_BLOCKS);
  DP_CHECK (async_dsss_receiver_seed (rx, 0.0, 0.0, 45.0) == DP_OK);
  printf ("  refine dwell at 45 dB-Hz, margin 14: %zu blocks (floor %zu)\n",
          rx->ca->dwell_target, rx->refine_min_blocks);
  DP_CHECK_MSG (rx->ca->dwell_target >= ASYNC_DSSS_RX_REFINE_MIN_BLOCKS,
                "the dwell is floored at refine_min_blocks");
  /* Without the floor the detection sizing alone: two blocks here. */
  async_dsss_receiver_reset (rx);
  DP_CHECK (async_dsss_receiver_set_refine_min_blocks (rx, 0) == DP_OK);
  DP_CHECK (async_dsss_receiver_seed (rx, 0.0, 0.0, 45.0) == DP_OK);
  DP_CHECK_MSG (rx->ca->dwell_target < ASYNC_DSSS_RX_REFINE_MIN_BLOCKS,
                "with the floor removed the detection sizing is shorter -- "
                "the floor was binding");
  /* A floor above the give-up cap is clamped to it. */
  async_dsss_receiver_reset (rx);
  DP_CHECK (async_dsss_receiver_set_refine_min_blocks (rx, 1000000) == DP_OK);
  DP_CHECK (async_dsss_receiver_seed (rx, 0.0, 0.0, 45.0) == DP_OK);
  DP_CHECK (rx->ca->dwell_target == rx->ca->max_n_blocks);
  async_dsss_receiver_destroy (rx);
  return 0;
}

static int
_test_handoff_state_roundtrip (void)
{
  const size_t sf = 7, spc = 4;
  const double fs          = 1.0e6 * (double)spc;
  const double sym_rate    = 35714.29;
  const double tsym        = fs / sym_rate;
  const size_t te          = sf * spc;
  const size_t n_sym       = 400;
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 70.0;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 7,
                   &x, &n, &data);

  /* Idle round trip first: the cheapest blob, and the state the pool
   * checkpoints most. */
  async_dsss_receiver_state_t *ra = _handoff_rx (cn0, 2.0);
  async_dsss_receiver_state_t *rb = _handoff_rx (cn0, 2.0);
  DP_CHECK (ra != NULL && rb != NULL);
  if (!ra || !rb)
    {
      free (x);
      free (data);
      return 1;
    }
  {
    size_t cb   = async_dsss_receiver_state_bytes (ra);
    void  *blob = malloc (cb);
    async_dsss_receiver_get_state (ra, blob);
    DP_CHECK (((async_dsss_receiver_extra_t *)((char *)blob
                                               + sizeof (dp_state_hdr_t)))
                  ->handoff
              == 1);
    DP_CHECK (async_dsss_receiver_set_state (rb, blob) == DP_OK);
    DP_CHECK (async_dsss_receiver_get_idle (rb) == 1);
    free (blob);
  }

  /* Tracking: split the stream, resume the second receiver from the blob,
   * and require the two to emit identical symbols from there on. */
  DP_CHECK (async_dsss_receiver_seed (ra, 0.0, 0.0, cn0) == DP_OK);
  /* 900 epochs before the split: the refine's dwell is floored at seven
     blocks (#1265), 3 ms at this rate, and the split must find the
     receiver tracking with symbols already out. */
  const size_t    split = pre_silence + te * 900;
  float _Complex *syms;
  size_t n_a = _stream (ra, x + pre_silence, split - pre_silence, te, &syms);
  free (syms);
  DP_CHECK (n_a > 20);
  DP_CHECK (async_dsss_receiver_get_tracking (ra) == 1);

  size_t cb   = async_dsss_receiver_state_bytes (ra);
  void  *blob = malloc (cb);
  async_dsss_receiver_get_state (ra, blob);
  DP_CHECK (async_dsss_receiver_set_state (rb, blob) == DP_OK);
  DP_CHECK (async_dsss_receiver_get_tracking (rb) == 1);
  DP_CHECK (async_dsss_receiver_get_idle (rb) == 0);

  float _Complex *sa, *sb;
  size_t          na = _stream (ra, x + split, n - split, te, &sa);
  size_t          nb = _stream (rb, x + split, n - split, te, &sb);
  DP_CHECK (na == nb && na > 20);
  int same = (na == nb);
  for (size_t i = 0; same && i < na; i++)
    same = (sa[i] == sb[i]);
  DP_CHECK (same);
  free (sa);
  free (sb);

  /* Across flavors: refused both ways, and the envelope reject still holds. */
  async_dsss_receiver_state_t *rs = async_dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 2.0);
  DP_CHECK (rs != NULL);
  if (rs)
    {
      DP_CHECK (async_dsss_receiver_set_state (rs, blob) == DP_ERR_INVALID);
      DP_CHECK (async_dsss_receiver_get_tracking (rs) == 0);
      size_t cbs   = async_dsss_receiver_state_bytes (rs);
      void  *blobs = malloc (cbs);
      async_dsss_receiver_get_state (rs, blobs);
      DP_CHECK (cbs != cb); /* the search engine is in one, not the other */
      DP_CHECK (async_dsss_receiver_set_state (rb, blobs) == DP_ERR_INVALID);
      DP_CHECK (async_dsss_receiver_get_tracking (rb) == 1); /* untouched */
      free (blobs);
      async_dsss_receiver_destroy (rs);
    }
  ((char *)blob)[0] ^= (char)0xFF;
  DP_CHECK (async_dsss_receiver_set_state (rb, blob) == DP_ERR_INVALID);
  free (blob);

  free (x);
  free (data);
  async_dsss_receiver_destroy (ra);
  async_dsss_receiver_destroy (rb);
  return 0;
}

/* The status record (section 11.3) is the getters' other face: every field
 * equals its getter in every state, the live Doppler follows the loop that
 * owns it (nothing idle, the seed while refining, the live carrier loop
 * once tracking), and the two clocks are the ones the release rule runs. */
static int
_test_status_record (void)
{
  const size_t sf = 7, spc = 4;
  const double fs          = 1.0e6 * (double)spc;
  const double sym_rate    = 35714.29;
  const double tsym        = fs / sym_rate;
  const size_t te          = sf * spc;
  const size_t n_sym       = 400;
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 70.0;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 7,
                   &x, &n, &data);

  async_dsss_receiver_state_t *rx = _handoff_rx (cn0, 0.02);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }

  /* Idle: nothing to report but the state and the clock. */
  async_dsss_receiver_status_t st = async_dsss_receiver_status (rx);
  DP_CHECK (st.state == ASYNC_DSSS_RX_IDLE);
  DP_CHECK (st.doppler_hz == 0.0);
  DP_CHECK (st.code_locked == 0 && st.locked == 0);
  DP_CHECK (st.state_samples == 0 && st.both_down_samples == 0);
  float _Complex tmp[64];
  (void)async_dsss_receiver_steps (rx, x, 37, tmp, 64);
  DP_CHECK (async_dsss_receiver_status (rx).state_samples == 37);

  /* Refining: the seed IS the estimate, and the clock restarted. */
  DP_CHECK (async_dsss_receiver_seed (rx, 2.5, -1234.0, 51.0) == DP_OK);
  st = async_dsss_receiver_status (rx);
  DP_CHECK (st.state == ASYNC_DSSS_RX_REFINING);
  DP_CHECK (fabs (st.doppler_hz + 1234.0) < 1e-6);
  DP_CHECK (st.cn0_dbhz_est == 51.0);
  DP_CHECK (st.state_samples == 0);
  async_dsss_receiver_reset (rx);

  /* Tracking on the real capture: every field equals its getter, and the
   * live Doppler is the carrier loop's, in Hz at the front-end rate. */
  DP_CHECK (async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  float _Complex *syms;
  size_t n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  free (syms);
  DP_CHECK (n_syms > 20);
  st = async_dsss_receiver_status (rx);
  DP_CHECK (st.state == ASYNC_DSSS_RX_TRACKING);
  DP_CHECK (st.chip_phase == async_dsss_receiver_get_chip_phase (rx));
  DP_CHECK (st.code_rate == async_dsss_receiver_get_code_rate (rx));
  DP_CHECK (st.cn0_dbhz_est == async_dsss_receiver_get_cn0_dbhz_est (rx));
  DP_CHECK (st.code_locked == async_dsss_receiver_get_code_locked (rx));
  DP_CHECK (st.locked == async_dsss_receiver_get_locked (rx));
  DP_CHECK (st.code_locked == 1 && st.locked == 1);
  DP_CHECK (st.lock_metric == async_dsss_receiver_get_lock_metric (rx));
  DP_CHECK (st.lock_threshold == async_dsss_receiver_get_lock_threshold (rx));
  DP_CHECK (st.car_last_error == async_dsss_receiver_get_car_last_error (rx));
  DP_CHECK (st.mpsk_last_error
            == async_dsss_receiver_get_mpsk_last_error (rx));
  /* doppler#1261: the whole carrier estimate -- loop 1 plus what loop 2
     took up beyond it, the sum configure_chain_raw() re-seeds from. */
  DP_CHECK (fabs (st.doppler_hz
                  - (costas_get_norm_freq (&rx->car) * fs
                     + async_dsss_receiver_get_norm_freq (rx)
                           * ((double)rx->sps * rx->symbol_rate)))
            < 1e-9);
  DP_CHECK (fabs (st.doppler_hz) < 50.0); /* the capture has no Doppler */
  DP_CHECK (st.state_samples == rx->state_samples && st.state_samples > 0);
  DP_CHECK (st.both_down_samples == 0); /* both flags up */

  /* The two flags are two fields: pin the code detector out of reach and
   * feed more signal, so code lock is down while symbol lock holds. */
  float _Complex *x2;
  size_t          n2;
  double         *data2;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, 0, 11, &x2, &n2,
                   &data2);
  {
    async_dsss_receiver_state_t *rd = _handoff_rx (cn0, 0.0);
    DP_CHECK (rd != NULL);
    if (rd)
      {
        DP_CHECK (async_dsss_receiver_seed (rd, 0.0, 0.0, cn0) == DP_OK);
        float _Complex *sd;
        size_t n_sd = _stream (rd, x + pre_silence, n - pre_silence, te, &sd);
        free (sd);
        DP_CHECK (n_sd > 20);
        async_dsss_receiver_configure_lock_raw (rd, 1e30, 1e30, 8, 0.1, 1, 1);
        n_sd = _stream (rd, x2, n2, 1024, &sd);
        free (sd);
        async_dsss_receiver_status_t sd_st = async_dsss_receiver_status (rd);
        DP_CHECK (sd_st.state == ASYNC_DSSS_RX_TRACKING);
        DP_CHECK (sd_st.code_locked == 0 && sd_st.locked == 1);
        DP_CHECK (sd_st.code_locked
                  == async_dsss_receiver_get_code_locked (rd));
        DP_CHECK (sd_st.locked == async_dsss_receiver_get_locked (rd));
        async_dsss_receiver_destroy (rd);
      }
  }
  free (x2);
  free (data2);

  /* Lost: the record says so, the estimate is frozen where it was, and the
   * release clock reports how long it ran. */
  float _Complex *off = _noise_tail ((size_t)(0.25 * fs), fs, cn0, 99);
  uint64_t        run = 0, fed = 0;
  double          doppler_before = st.doppler_hz;
  DP_CHECK (_feed_until_lost (rx, off, (size_t)(0.25 * fs), 1024, &run, &fed));
  st = async_dsss_receiver_status (rx);
  DP_CHECK (st.state == ASYNC_DSSS_RX_LOST);
  DP_CHECK (st.both_down_samples > rx->lost_confirm_samples);
  DP_CHECK (st.both_down_samples == rx->both_down_samples);
  DP_CHECK (st.code_locked == 0 && st.locked == 0);
  /* Frozen: more input changes nothing the record reports. */
  (void)async_dsss_receiver_steps (rx, off, 1024, tmp, 64);
  async_dsss_receiver_status_t st2 = async_dsss_receiver_status (rx);
  DP_CHECK (st2.doppler_hz == st.doppler_hz);
  DP_CHECK (st2.both_down_samples == st.both_down_samples + 1024);
  DP_CHECK (st2.state_samples == st.state_samples + 1024);
  (void)doppler_before;

  free (off);
  free (x);
  free (data);
  async_dsss_receiver_destroy (rx);
  return 0;
}

static int
_test_accessor_coverage (void)
{
  async_dsss_receiver_state_t *rx = async_dsss_receiver_create (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5, 4,
      14.0, 64, 8, false, 100000, 0.0, 0.0);
  DP_CHECK (rx != NULL);
  if (!rx)
    return 1;

  /* Read-only accessors: each executes its one-line body on live state. */
  (void)async_dsss_receiver_get_lock (rx);
  (void)async_dsss_receiver_get_idle (rx);
  (void)async_dsss_receiver_get_lost (rx);
  (void)async_dsss_receiver_get_locked (rx);
  (void)async_dsss_receiver_get_code_locked (rx);
  (void)async_dsss_receiver_get_lock_metric (rx);
  DP_CHECK (async_dsss_receiver_get_lock_threshold (rx) > 0.0);
  (void)async_dsss_receiver_get_norm_freq (rx);
  (void)async_dsss_receiver_get_nco_freq (rx);
  (void)async_dsss_receiver_get_car_nco_freq (rx);
  (void)async_dsss_receiver_get_car_last_error (rx);
  (void)async_dsss_receiver_get_mpsk_last_error (rx);
  (void)async_dsss_receiver_steps_max_out (rx); /* 0 until first stream */

  /* Raw sub-loop reconfiguration: valid grids/detectors, then the chain's
   * accept + both reject branches (segments < 1, and sps not a multiple of n).
   */
  DP_CHECK (async_dsss_receiver_configure_search_raw (rx, 1, 1) == 0);
  DP_CHECK (async_dsss_receiver_configure_search_raw (rx, 100000, 1) == -1);
  async_dsss_receiver_configure_lock_raw (rx, 12.0, 6.0, 8, 0.1, 3, 3);
  DP_CHECK (async_dsss_receiver_configure_chain_raw (rx, 4, 8, 4) == 0);
  DP_CHECK (async_dsss_receiver_configure_chain_raw (rx, 0, 8, 4) == -1);
  DP_CHECK (async_dsss_receiver_configure_chain_raw (rx, 4, 8, 3) == -1);

  async_dsss_receiver_destroy (rx);
  return 0;
}

/* ------------------------------------------------------------------ *
 * The cell mode (docs/design/async-dsss-receiver.md section 12.22-12.24  *
 * as a mode of this receiver, #1283): the receiver a searcher's cell     *
 * drives -- no refine, the carrier frozen, the Dll held from the first   *
 * sample and put back once an interval at a held phase corrected by a  *
 * gain times its interval-mean discriminator.                           *
 * ------------------------------------------------------------------ */
static async_dsss_receiver_state_t *
_cell_rx (double cn0, double lost_confirm_s, double carrier_hz,
          size_t correct_periods, double gain, size_t pullin)
{
  return async_dsss_receiver_create_cell (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, cn0, 1e-2, 0.9, 4, 8, 0, carrier_hz,
      lost_confirm_s, correct_periods, gain, pullin);
}

/* The dilated capture's code phase at output sample k, chips, folded on
 * the code: the channel's mapping (output k carries input k(1+d) - delay,
 * chip 0 on the clean render's first sample). */
static double
_dilated_truth (double k, double ppm, double delay, size_t spc, size_t sf)
{
  double n_in = k * (1.0 + ppm * 1e-6) - delay;
  double c    = fmod (n_in / (double)spc, (double)sf);
  return c < 0.0 ? c + (double)sf : c;
}

static double
_wrap_chips (double e, size_t sf)
{
  e = fmod (e, (double)sf);
  if (e > 0.5 * (double)sf)
    e -= (double)sf;
  else if (e <= -0.5 * (double)sf)
    e += (double)sf;
  return e;
}

static int
_test_cell_lifecycle_and_args (void)
{
  const double cn0 = 70.0;
  DP_CHECK (_cell_rx (cn0, 0.0, 0.0, 0, 0.125, 4) == NULL); /* period */
  DP_CHECK (_cell_rx (cn0, 0.0, 0.0, 100, 0.0, 4) == NULL); /* gain 0 */
  DP_CHECK (_cell_rx (cn0, 0.0, 0.0, 100, 1.5, 4) == NULL); /* gain>1 */
  async_dsss_receiver_state_t *rx = _cell_rx (cn0, 0.0, 0.0, 100, 0.125, 4);
  DP_CHECK (rx != NULL);
  if (!rx)
    return 1;
  DP_CHECK (async_dsss_receiver_get_idle (rx) == 1);
  DP_CHECK (rx->acq == NULL && rx->ca == NULL && rx->refine_dll == NULL);
  DP_CHECK (rx->cell == 1);
  DP_CHECK (async_dsss_receiver_set_refine_min_blocks (rx, 3)
            == DP_ERR_INVALID);
  DP_CHECK (async_dsss_receiver_configure_search_raw (rx, 1, 1) == -1);
  DP_CHECK (async_dsss_receiver_seed (rx, 1.0, 0.0, cn0) == DP_OK);
  DP_CHECK (async_dsss_receiver_get_refining (rx) == 1); /* the pull-in */
  DP_CHECK (rx->held_phase == 1.0);
  DP_CHECK (rx->dll->coast == 1); /* held from the first sample */
  DP_CHECK (async_dsss_receiver_seed (rx, 2.0, 0.0, cn0) == DP_ERR_INVALID);
  /* Idle consumes and discards; refining decodes: fed a little noise,
     the pull-in receiver emits (garbage) symbols where idle emits none. */
  async_dsss_receiver_reset (rx);
  DP_CHECK (async_dsss_receiver_get_idle (rx) == 1);
  DP_CHECK (rx->held_phase == 0.0 && rx->intervals == 0);
  DP_CHECK (async_dsss_receiver_seed (rx, 6.5, -100.0, cn0) == DP_OK);
  DP_CHECK (async_dsss_receiver_get_doppler_hz (rx) == -100.0);
  async_dsss_receiver_destroy (rx);
  return 0;
}

/* Holds: seeded 0.4 chip and 5 Hz off the truth on a dilated stream (200
 * ppm of a coupled carrier -- 0.14 chip of dead reckoning per interval),
 * the held phase pulls in at gain 1 and then sits within 0.05 chip of the
 * truth at every interval, decoding; and at the design gain 1/8 the held
 * phase's scatter is under three quarters of gain 1's (12.24), the read
 * being white to that gain. Sabotaged red: the correction's sign; the dead
 * reckoning dropped (the phase falls 0.14 chip behind per interval, the
 * correction at 1/8 cannot carry it); the gain ignored. */
static int
_test_cell_holds_and_decodes (void)
{
  const size_t sf = 7, spc = 4;
  const double fs       = 1.0e6 * (double)spc;
  const double sym_rate = 35714.29;
  const double tsym     = fs / sym_rate;
  const size_t te       = sf * spc;
  /* SPEC's 20 ppm of a coupled carrier: 5 kHz of Doppler at this scale,
     0.014 chip of dead reckoning per interval. */
  const double carrier_hz = 2.5e8, ppm = 20.0;
  const double doppler_hz = carrier_hz * ppm * 1e-6; /* 5 kHz */
  const double cn0        = 58.0;                    /* Es/N0 12.5 dB */
  const size_t n_sym      = 12000;
  const size_t periods    = 100; /* 2800 samples, 25 symbols an interval */
  const size_t interval   = periods * te;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_dilated_capture (CODE7, sf, spc, fs, tsym, carrier_hz, ppm, 0.0, cn0,
                           n_sym, 0, 11, &x, &n, &data);
  doppler_channel_state_t *ch
      = doppler_channel_create (fs, carrier_hz, ppm, 0.0);
  const double delay = doppler_channel_get_delay_samples (ch);
  doppler_channel_destroy (ch);
  /* The truth is the channel's own mapping; the Dll's convention is the
     code phase at the next sample, so no constant is calibrated -- the
     coasting loop reads the mapping to a hundredth of a chip (the running
     hand-off loop, for the record, converges 0.24 chip off it here). */
  const double c = 0.0;

  const double gains[2] = { 0.125, 1.0 };
  double       sig[2]   = { 0.0, 0.0 };
  for (int g = 0; g < 2; g++)
    {
      async_dsss_receiver_state_t *rx
          = _cell_rx (cn0, 0.0, carrier_hz, periods, gains[g], 4);
      DP_CHECK (rx != NULL);
      double seed = _dilated_truth (0.0, ppm, delay, spc, sf) + c + 0.4;
      seed        = fmod (seed + 7.0 * 4.0, (double)sf);
      DP_CHECK (async_dsss_receiver_seed (rx, seed, doppler_hz + 5.0, cn0)
                == DP_OK);
      float _Complex *syms = malloc (n * sizeof *syms);
      size_t          ns = 0, k = 0, worst_i = 0;
      double          se = 0.0, se2 = 0.0, worst = 0.0;
      for (size_t pos = 0; pos + interval <= n; pos += interval)
        {
          ns += async_dsss_receiver_steps (rx, x + pos, interval, syms + ns,
                                           n - ns);
          /* Settled: past the pull-in and a few intervals of the design
             gain; the phase the receiver holds against the truth here. */
          size_t i = pos / interval;
          if (i < 30)
            continue;
          double e
              = _wrap_chips (async_dsss_receiver_get_chip_phase (rx)
                                 - _dilated_truth ((double)(pos + interval),
                                                   ppm, delay, spc, sf)
                                 - c,
                             sf);
          se += e;
          se2 += e * e;
          k++;
          if (fabs (e) > worst)
            {
              worst   = fabs (e);
              worst_i = i;
            }
        }
      DP_CHECK (k > 100);
      const double bias = se / (double)k;
      sig[g]            = sqrt (fmax (se2 / (double)k - bias * bias, 0.0));
      printf ("  cell: gain %.3f -- held phase bias %+.4f sigma %.4f worst "
              "%.3f (interval %zu) over %zu intervals; tracking %d code %d "
              "sym %d; %zu symbols\n",
              gains[g], bias, sig[g], worst, worst_i, k,
              async_dsss_receiver_get_tracking (rx),
              async_dsss_receiver_get_code_locked (rx),
              async_dsss_receiver_get_locked (rx), ns);
      DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);
      DP_CHECK (async_dsss_receiver_get_code_locked (rx) == 1);
      DP_CHECK (async_dsss_receiver_get_locked (rx) == 1);
      DP_CHECK (rx->intervals > 100);
      /* The Dll's own loop never closed: it coasted through the run, the
         interval correction its only steer (the design's claim, 12.22). */
      DP_CHECK_MSG (rx->dll->coast == 1,
                    "cell mode: the Dll coasts throughout; the correction is "
                    "its steer");
      if (g == 0)
        DP_CHECK_MSG (worst < 0.05 && fabs (bias) < 0.03,
                      "cell mode: the held phase sits on the truth at "
                      "every interval after the pull-in");
      DP_CHECK (_best_ber (syms, ns, data, n_sym + 4) < 0.05);
      DP_CHECK (dp_test_evm_db_hard (syms + ns / 2, ns - ns / 2) < -8.0);
      /* The seed is refused while tracking; a re-seed after reset works. */
      DP_CHECK (async_dsss_receiver_seed (rx, 0.0, 0.0, cn0)
                == DP_ERR_INVALID);
      free (syms);
      async_dsss_receiver_destroy (rx);
    }
  DP_CHECK_MSG (sig[0] < 0.75 * sig[1],
                "cell mode: gain 1/8 holds under three quarters of gain 1's "
                "scatter");
  free (x);
  free (data);
  return 0;
}

/* Switched off after a lock, the held phase dead-reckons on the held rate
 * and the correction stops with the code flag -- so the receiver, lost by
 * the release rule, is still where its emitter left it; reset() to idle
 * takes the next seed. Sabotaged red: correct on noise regardless of the
 * flag (the phase random-walks off the hold point). */
static int
_test_cell_holds_through_switch_off (void)
{
  const size_t sf = 7, spc = 4;
  const double fs         = 1.0e6 * (double)spc;
  const double sym_rate   = 35714.29;
  const double tsym       = fs / sym_rate;
  const size_t te         = sf * spc;
  const double carrier_hz = 2.5e8, ppm = 20.0;
  const double doppler_hz = carrier_hz * ppm * 1e-6;
  const double cn0        = 62.0;
  const size_t n_sym      = 4000;
  const size_t periods    = 100;
  const size_t interval   = periods * te;
  const double lost_s     = 0.02; /* 80000 samples, 28 intervals */

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_dilated_capture (CODE7, sf, spc, fs, tsym, carrier_hz, ppm, 0.0, cn0,
                           n_sym, 0, 13, &x, &n, &data);
  doppler_channel_state_t *ch
      = doppler_channel_create (fs, carrier_hz, ppm, 0.0);
  const double delay = doppler_channel_get_delay_samples (ch);
  doppler_channel_destroy (ch);
  const double c = 0.0;
  /* The tail: the emitter gone, the same noise at the receiver. */
  const size_t    n_tail = (size_t)(lost_s * fs) * 2;
  float _Complex *tail   = malloc (n_tail * sizeof *tail);
  double          sigma  = 1.0 / sqrt (pow (10.0, cn0 / 10.0) / fs);
  uint32_t        st     = 0xC0FFEEu;
  for (size_t i = 0; i < n_tail; i++)
    tail[i] = (float _Complex) (sigma / sqrt (2.0)) * dp_cgauss (&st);

  /* Gain 1: a correction taken on noise would move the phase by the whole
     read, so the gate on the code flag is what this test sees. */
  async_dsss_receiver_state_t *rx
      = _cell_rx (cn0, lost_s, carrier_hz, periods, 1.0, 4);
  DP_CHECK (rx != NULL);
  double seed = fmod (_dilated_truth (0.0, ppm, delay, spc, sf) + c + 28.0,
                      (double)sf);
  DP_CHECK (async_dsss_receiver_seed (rx, seed, doppler_hz, cn0) == DP_OK);
  float _Complex *syms = malloc ((n + n_tail) * sizeof *syms);
  size_t          ns = 0, pos = 0;
  for (; pos + interval <= n; pos += interval)
    ns += async_dsss_receiver_steps (rx, x + pos, interval, syms + ns,
                                     n + n_tail - ns);
  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (async_dsss_receiver_get_code_locked (rx) == 1
            && async_dsss_receiver_get_locked (rx) == 1);
  const double e_on = _wrap_chips (
      async_dsss_receiver_get_chip_phase (rx)
          - _dilated_truth ((double)pos, ppm, delay, spc, sf) - c,
      sf);
  DP_CHECK (fabs (e_on) < 0.05);
  /* Off: the flags drop, the clock runs out, the phase is where the
     emitter's would be -- dead-reckoned at the held rate, uncorrected. */
  const uint64_t intervals_on = rx->intervals;
  size_t         tp           = 0;
  int            lost_at      = -1;
  double         e_off        = 0.0;
  for (; tp + interval <= n_tail; tp += interval)
    {
      ns += async_dsss_receiver_steps (rx, tail + tp, interval, syms + ns,
                                       n + n_tail - ns);
      if (lost_at < 0 && async_dsss_receiver_get_lost (rx))
        {
          /* Lost: the receiver stops updating and its phase is where the
             emitter left it (section 10) -- read it at the interval the
             rule fired on, dead-reckoned to there. */
          lost_at = (int)(tp / interval);
          e_off   = _wrap_chips (
              async_dsss_receiver_get_chip_phase (rx)
                  - _dilated_truth ((double)(pos + tp + interval), ppm, delay,
                                    spc, sf)
                  - c,
              sf);
        }
    }
  DP_CHECK (async_dsss_receiver_get_lost (rx) == 1);
  DP_CHECK (lost_at > 0);
  printf ("  cell: switched off -- lost after %d intervals of %zu; held "
          "phase %+.3f chips from the hold point (%+.3f at switch-off), "
          "%llu intervals on\n",
          lost_at, tp / interval, e_off, e_on,
          (unsigned long long)intervals_on);
  DP_CHECK_MSG (fabs (e_off - e_on) < 0.03,
                "cell mode: switched off, the held phase dead-reckons and "
                "does not walk");
  async_dsss_receiver_reset (rx);
  DP_CHECK (async_dsss_receiver_get_idle (rx) == 1);
  DP_CHECK (async_dsss_receiver_seed (rx, seed, doppler_hz, cn0) == DP_OK);
  free (syms);
  free (tail);
  free (x);
  free (data);
  async_dsss_receiver_destroy (rx);
  return 0;
}

/* The ramp: the carrier loop of the hand-off flavor runs in the cell mode
 * too, so a ramping carrier is followed pre-despread and the symbol lock
 * never breaks. Sabotaged red: the carrier loop frozen in the cell mode
 * (the status Doppler stays at the seed and the lock drops). */
static int
_test_cell_ramp (void)
{
  const size_t sf = 7, spc = 4;
  const double fs         = 1.0e6 * (double)spc;
  const double sym_rate   = 35714.29;
  const double tsym       = fs / sym_rate;
  const size_t te         = sf * spc;
  const double carrier_hz = 2.5e7;
  const double ppm_s      = 16.0; /* 400 Hz/s at this carrier */
  const double cn0        = 62.0;
  const size_t n_sym      = 60000; /* 1.68 s: the residual crosses 200 Hz
                                      three times */
  const size_t periods  = 100;
  const size_t interval = periods * te;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_dilated_capture (CODE7, sf, spc, fs, tsym, carrier_hz, 0.0, ppm_s,
                           cn0, n_sym, 0, 17, &x, &n, &data);
  async_dsss_receiver_state_t *rx
      = _cell_rx (cn0, 0.0, carrier_hz, periods, 0.125, 4);
  DP_CHECK (rx != NULL);
  /* The truth's phase at sample 0 is the channel's delay; no ramp yet. */
  doppler_channel_state_t *ch
      = doppler_channel_create (fs, carrier_hz, 0.0, ppm_s);
  const double delay = doppler_channel_get_delay_samples (ch);
  doppler_channel_destroy (ch);
  const double seed
      = fmod (_dilated_truth (0.0, 0.0, delay, spc, sf) + 28.0, (double)sf);
  DP_CHECK (async_dsss_receiver_seed (rx, seed, 0.0, cn0) == DP_OK);
  float _Complex *syms = malloc (n * sizeof *syms);
  size_t          ns = 0, unlocked_after = 0, pos = 0;
  for (; pos + interval <= n; pos += interval)
    {
      ns += async_dsss_receiver_steps (rx, x + pos, interval, syms + ns,
                                       n - ns);
      if (pos > n / 4 && !async_dsss_receiver_get_locked (rx))
        unlocked_after++;
    }
  const double                 t_end  = (double)pos / fs;
  const double                 f_true = carrier_hz * ppm_s * 1e-6 * t_end;
  async_dsss_receiver_status_t st     = async_dsss_receiver_status (rx);
  printf ("  cell: ramp -- truth %.0f Hz at the end, status %.0f; intervals "
          "with the symbol flag down after the first quarter: %zu of %zu\n",
          f_true, st.doppler_hz, unlocked_after, pos / interval);
  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK_MSG (fabs (st.doppler_hz - f_true) < 60.0,
                "cell mode: the carrier loop follows the ramp");
  DP_CHECK_MSG (unlocked_after == 0,
                "cell mode: the symbol lock holds through the ramp");
  DP_CHECK (_best_ber (syms, ns, data, n_sym + 4) < 0.05);
  free (syms);
  free (x);
  free (data);
  async_dsss_receiver_destroy (rx);
  return 0;
}

/* The ramp at the design geometry (Gold-length code, 2700 symbols per
 * second, SPEC's 500 Hz/s): MpskReceiver's carrier loop is 27 Hz wide
 * here and cannot follow the ramp alone -- measured: with the carrier
 * frozen the symbol flag was down 40 intervals of 48 and the BER 0.45 --
 * so the cell mode keeps the pre-despread loop running. Sabotaged red: the
 * carrier loop frozen in the cell mode. */
static int
_test_cell_ramp_at_spec (void)
{
  const size_t sf        = 1023;
  const size_t spc       = 2;
  const double chip_rate = 3.069e6;
  const double fs        = chip_rate * (double)spc;
  const double sym_rate  = 2700.0;
  const double tsym      = fs / sym_rate;
  const size_t te        = sf * spc;
  const double rate_hz_s = 500.0;
  const size_t n_sym     = 6750; /* 2.5 s: the residual crosses 500 Hz at
                                     1 s and 2 s */
  const size_t pre      = te * 5 + 3;
  const double cn0      = 30.0 + 10.0 * log10 (sym_rate); /* Es/N0 30 dB */
  const size_t periods  = 154;
  const size_t interval = periods * te;

  uint8_t *code = malloc (sf);
  uint32_t cst  = 13;
  for (size_t i = 0; i < sf; i++)
    code[i] = (uint8_t)(dp_bit (&cst) > 0 ? 0u : 1u);
  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_ramp_capture (code, sf, spc, fs, tsym, rate_hz_s, cn0, n_sym, pre,
                        23, &x, &n, &data);
  async_dsss_receiver_state_t *rx = async_dsss_receiver_create_cell (
      code, sf, chip_rate, sym_rate, spc, 2, cn0, 1e-2, 0.9, 4, 8, 0, 0.0, 0.0,
      periods, ASYNC_DSSS_RX_CELL_GAIN, ASYNC_DSSS_RX_CELL_PULLIN);
  DP_CHECK (rx != NULL);
  /* Chip 0 on the first signal sample, the ramp from 0 Hz: the seed is the
     truth, as the hand-off tests seed. */
  DP_CHECK (async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  float _Complex *syms = malloc (n * sizeof *syms);
  size_t          ns = 0, down = 0, pos = pre, k = 0;
  for (; pos + interval <= n; pos += interval, k++)
    {
      ns += async_dsss_receiver_steps (rx, x + pos, interval, syms + ns,
                                       n - ns);
      if (k >= 8 && !async_dsss_receiver_get_locked (rx))
        down++;
    }
  const double                 t_end  = (double)(pos - pre) / fs;
  const double                 f_true = rate_hz_s * t_end;
  async_dsss_receiver_status_t st     = async_dsss_receiver_status (rx);
  printf ("  cell: ramp at SPEC -- truth %.0f Hz at the end, status %.0f; "
          "intervals with the symbol flag down after the pull-in: %zu of "
          "%zu; %zu symbols\n",
          f_true, st.doppler_hz, down, k, ns);
  DP_CHECK (async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK_MSG (fabs (st.doppler_hz - f_true) < 60.0,
                "cell mode at SPEC: the carrier loop follows the ramp");
  DP_CHECK_MSG (down == 0,
                "cell mode at SPEC: the symbol lock holds through the ramp");
  printf ("    BER %.4f, EVM %.1f dB\n", _best_ber (syms, ns, data, n_sym + 4),
          dp_test_evm_db_hard (syms + ns / 2, ns - ns / 2));
  DP_CHECK (_best_ber (syms, ns, data, n_sym + 4) < 0.05);
  free (syms);
  free (x);
  free (data);
  free (code);
  async_dsss_receiver_destroy (rx);
  return 0;
}

/* The blob: keyed on the mode (a hand-off blob is refused), no refine
 * children, the held phase and Doppler and the interval clocks in it -- a
 * receiver resumed mid-stream emits the hand-off's symbols and holds its
 * phase. Sabotaged red: held_phase left out of the extra record (the
 * resumed receiver's next correction puts its Dll at 0). */
static int
_test_cell_state_roundtrip (void)
{
  const size_t sf = 7, spc = 4;
  const double fs         = 1.0e6 * (double)spc;
  const double sym_rate   = 35714.29;
  const double tsym       = fs / sym_rate;
  const size_t te         = sf * spc;
  const double carrier_hz = 2.5e8, ppm = 20.0;
  const double doppler_hz = carrier_hz * ppm * 1e-6;
  const double cn0        = 62.0;
  const size_t n_sym      = 4000;
  const size_t periods    = 100;
  const size_t interval   = periods * te;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_dilated_capture (CODE7, sf, spc, fs, tsym, carrier_hz, ppm, 0.0, cn0,
                           n_sym, 0, 19, &x, &n, &data);
  doppler_channel_state_t *ch
      = doppler_channel_create (fs, carrier_hz, ppm, 0.0);
  const double delay = doppler_channel_get_delay_samples (ch);
  doppler_channel_destroy (ch);

  async_dsss_receiver_state_t *ra
      = _cell_rx (cn0, 2.0, carrier_hz, periods, 0.125, 4);
  async_dsss_receiver_state_t *rb
      = _cell_rx (cn0, 2.0, carrier_hz, periods, 0.125, 4);
  DP_CHECK (ra && rb);
  /* Idle: the mode in the blob; a hand-off blob is refused. */
  {
    size_t cb   = async_dsss_receiver_state_bytes (ra);
    void  *blob = malloc (cb);
    async_dsss_receiver_get_state (ra, blob);
    const async_dsss_receiver_extra_t *ex
        = (const async_dsss_receiver_extra_t *)((const char *)blob
                                                + sizeof (dp_state_hdr_t));
    DP_CHECK (ex->cell == 1 && ex->handoff == 1);
    DP_CHECK (async_dsss_receiver_set_state (rb, blob) == DP_OK);
    free (blob);
    async_dsss_receiver_state_t *rh   = _handoff_rx (cn0, 2.0);
    size_t                       ch_b = async_dsss_receiver_state_bytes (rh);
    void                        *hb   = malloc (ch_b);
    async_dsss_receiver_get_state (rh, hb);
    DP_CHECK (async_dsss_receiver_set_state (rb, hb) == DP_ERR_INVALID);
    free (hb);
    async_dsss_receiver_destroy (rh);
  }
  const double seed
      = fmod (_dilated_truth (0.0, ppm, delay, spc, sf) + 28.0, (double)sf);
  DP_CHECK (async_dsss_receiver_seed (ra, seed, doppler_hz, cn0) == DP_OK);
  float _Complex *sa = malloc (n * sizeof *sa), *sb = malloc (n * sizeof *sb);
  size_t          na = 0, nb = 0, pos = 0;
  /* Forty intervals in, tracking, mid-interval: the split. */
  const size_t split = 40 * interval + interval / 3;
  for (; pos + interval <= split; pos += interval)
    na += async_dsss_receiver_steps (ra, x + pos, interval, sa + na, n - na);
  na += async_dsss_receiver_steps (ra, x + pos, split - pos, sa + na, n - na);
  pos = split;
  DP_CHECK (async_dsss_receiver_get_tracking (ra) == 1);
  DP_STATE_ROUNDTRIP_TEST (async_dsss_receiver, ra, rb);
  DP_CHECK (async_dsss_receiver_get_tracking (rb) == 1);
  DP_CHECK (rb->held_phase == ra->held_phase
            && rb->period_count == ra->period_count
            && rb->intervals == ra->intervals);
  size_t na0 = na;
  nb         = 0;
  for (; pos + interval <= n; pos += interval)
    {
      na += async_dsss_receiver_steps (ra, x + pos, interval, sa + na, n - na);
      nb += async_dsss_receiver_steps (rb, x + pos, interval, sb + nb, n - nb);
    }
  DP_CHECK (na - na0 == nb && nb > 100);
  DP_CHECK (memcmp (sa + na0, sb, nb * sizeof *sb) == 0);
  async_dsss_receiver_status_t sta = async_dsss_receiver_status (ra),
                               stb = async_dsss_receiver_status (rb);
  DP_CHECK (sta.chip_phase == stb.chip_phase
            && sta.doppler_hz == stb.doppler_hz
            && sta.code_locked == stb.code_locked && sta.locked == stb.locked);
  free (sa);
  free (sb);
  free (x);
  free (data);
  async_dsss_receiver_destroy (ra);
  async_dsss_receiver_destroy (rb);
  return 0;
}

int
main (void)
{
  (void)_test_arg_validation ();
  (void)_test_acquire_and_decode ();
  (void)_test_give_up_cap ();
  (void)_test_spec_ramp_decode ();
  (void)_test_handover_under_clock_offset ();
  (void)_test_spec_combined_scenario_at_spec_floor ();
  (void)_test_awgn_esn0_floor ();
  (void)_test_accessor_coverage ();
  (void)_test_handoff_seed_and_decode ();
  (void)_test_seed_on_searching_flavor ();
  (void)_test_lost_after_switch_off ();
  (void)_test_one_flag_down_is_a_degrade ();
  (void)_test_refine_dwell_floor ();
  (void)_test_handoff_state_roundtrip ();
  (void)_test_status_record ();
  (void)_test_cell_lifecycle_and_args ();
  (void)_test_cell_holds_and_decodes ();
  (void)_test_cell_holds_through_switch_off ();
  (void)_test_cell_ramp ();
  (void)_test_cell_ramp_at_spec ();
  (void)_test_cell_state_roundtrip ();

  DP_TEST_END ("test_async_dsss_receiver_core");
}
