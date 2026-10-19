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
 * 11.1-11.2): cell mode (no search; idle -> seed -> refine -> track,
 * assigned once, reset() to idle), seed() on the searching flavor and its
 * range checks, the release rule (both flags down past lost_confirm_s ->
 * lost, not before, never at 0) and the flavor-keyed state round trip.
 */
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/carrier_acq/carrier_acq_core.h" /* its blob magic, for forging */
#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/gold/gold_core.h" /* SPEC Gold-1023 for the Es/N0-floor sweep   */
#include "dp_dsss_test.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_sym_test.h"
#include "dp_test.h"

#include "doppler/dp_complex.h"
#include "doppler/wfm/wfm_dsp.h" /* dp_wfm_cont_dsss_chips: the wfmgen C API       */
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A length-7 maximal-length sequence -- same fixture test_dsss_receiver_
 * core.c/test_acq_core.c use for fast, real (not mocked) unit tests. */
static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

/* Stream `x` through `rx` in fixed-size chunks, collecting every emitted
 * symbol; return the symbol count and fill `*syms_out` (caller frees). */
static size_t
_stream (dp_async_dsss_receiver_state_t *rx, const float _Complex *x, size_t n,
         size_t chunk, float _Complex **syms_out)
{
  float _Complex *syms = malloc (n * sizeof *syms); /* generous upper bound */
  size_t          n_syms = 0;
  for (size_t pos = 0; pos < n; pos += chunk)
    {
      size_t take = (pos + chunk <= n) ? chunk : (n - pos);
      n_syms += dp_async_dsss_receiver_steps (rx, x + pos, take, syms + n_syms,
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
  DP_CHECK (dp_async_dsss_receiver_create (
                NULL, 0, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false)
            == NULL);
  /* A 1-chip code: refused, both flavours, where the Dll built under dp_xnn
     used to abort the process once it refused the code (doppler#2103). */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 1, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false)
            == NULL);
  DP_CHECK (dp_async_dsss_receiver_create_cell (CODE7, 1, 1e6, 1e3, 2, 2, 55.0,
                                                1e-3, 0.9, 4, 8, 0, 0.0, 1.0,
                                                100, 0.125, 4)
            == NULL);
  /* Refused by create itself, where each reached a dp_xnn build and
     aborted the process (doppler#2103): the searcher's own domain (a pfa of
     2), sps = 1, which carries no m_out, and a carrier so small that
     doppler / carrier overflows. The control is the same receiver at good
     values, so it is the one argument that is refused. */
  {
    dp_async_dsss_receiver_state_t *ok = dp_async_dsss_receiver_create (
        CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5, 4,
        14.0, 64, 8, false, 100000, 2.5e9, 0.0, false);
    DP_CHECK (ok != NULL);
    dp_async_dsss_receiver_destroy (ok);
    dp_async_dsss_receiver_state_t *cell = dp_async_dsss_receiver_create_cell (
        CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 4, 8, 0, 2.5e9, 1.0, 100,
        0.125, 4);
    DP_CHECK (cell != NULL);
    dp_async_dsss_receiver_destroy (cell);
  }
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 2.0, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 2.5e9, 0.0, false)
            == NULL); /* pfa */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 1, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 2.5e9, 0.0, false)
            == NULL); /* sps */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 1e-305, 0.0, false)
            == NULL); /* carrier */
  DP_CHECK (dp_async_dsss_receiver_create_cell (CODE7, 7, 1e6, 1e3, 2, 2, 55.0,
                                                1e-3, 0.9, 4, 1, 0, 2.5e9, 1.0,
                                                100, 0.125, 4)
            == NULL); /* sps */
  DP_CHECK (dp_async_dsss_receiver_create_cell (CODE7, 7, 1e6, 1e3, 2, 2, 55.0,
                                                1e-3, 0.9, 4, 8, 0, 1e-305,
                                                1.0, 100, 0.125, 4)
            == NULL); /* carrier */
  /* The cell flavour has no searcher, but builds a carrier estimator under
     dp_xnn that refuses a pfa or pd outside (0, 1). A symbol rate at either
     extreme: 5e-324 underflowed the chain's ratio to 0, and one past the
     chip rate sized the scratch at ~4e12 samples. The 2^20-partial cap
     holds at one segment too (doppler#2103). */
  DP_CHECK (dp_async_dsss_receiver_create_cell (CODE7, 7, 1e6, 1e3, 2, 2, 55.0,
                                                2.0, 0.9, 4, 8, 0, 2.5e9, 1.0,
                                                100, 0.125, 4)
            == NULL); /* pfa */
  DP_CHECK (dp_async_dsss_receiver_create_cell (CODE7, 7, 1e6, 1e3, 2, 2, 55.0,
                                                1e-3, 1.5, 4, 8, 0, 2.5e9, 1.0,
                                                100, 0.125, 4)
            == NULL); /* pd */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 5e-324, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0,
                0.5, 4, 14.0, 64, 8, false, 100000, 2.5e9, 0.0, false)
            == NULL); /* symbol_rate underflows the ratio */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e15, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0,
                0.5, 4, 14.0, 64, 8, false, 100000, 2.5e9, 0.0, false)
            == NULL); /* symbol_rate past the chip rate */
  DP_CHECK (dp_async_dsss_receiver_create_cell (CODE7, 7, 1e6, 0.1, 2, 2, 55.0,
                                                1e-3, 0.9, 1, 8, 0, 2.5e9, 1.0,
                                                100, 0.125, 4)
            == NULL); /* 1e6 / (7 * 0.1) partials at ONE segment */
  /* A symbol of more than 2^20 Dll partials -- here 4 * 1e6 / (7 * 0.1),
     about 5.7e6 -- is refused at create by the receiver's own chain-ratio
     clause, segments * chip_rate / (code_len * symbol_rate) <= 2^20. That
     is the same count the Dll's aid refuses past, so this case pins the
     receiver's clause, not the aid's: the tracker sets the period
     mid-stream, where a refusal used to drop the aid silently (#2103). */
  DP_CHECK (dp_async_dsss_receiver_create_cell (CODE7, 7, 1e6, 0.1, 2, 2, 55.0,
                                                1e-3, 0.9, 4, 8, 0, 0.0, 1.0,
                                                100, 0.125, 4)
            == NULL);
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 0.0, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false)
            == NULL); /* chip_rate <= 0 */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 3, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false)
            == NULL); /* m not in {2,4,8} */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 0, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false)
            == NULL); /* segments < 1 */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, -1.0, 0.0, false)
            == NULL); /* carrier_freq_hz < 0 */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 0.0, -1.0, false)
            == NULL); /* lost_confirm_s < 0 */
  DP_CHECK (dp_async_dsss_receiver_create_cell (CODE7, 7, 1e6, 1e3, 2, 2, 55.0,
                                                1e-3, 0.9, 4, 8, 0, 0.0, NAN,
                                                100, 0.125, 4)
            == NULL); /* lost_confirm_s NaN, cell flavor */

  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5, 4,
      14.0, 64, 8, false, 100000, 0.0, 0.0, false);
  DP_CHECK (rx != NULL);
  if (rx)
    {
      DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 0);
      DP_CHECK (dp_async_dsss_receiver_get_refining (rx) == 0);
      DP_CHECK (dp_async_dsss_receiver_get_segments (rx) == 4);
      DP_CHECK (dp_async_dsss_receiver_get_sps (rx) == 8);
      /* `n` is MpskReceiver's m_out (terminal outputs per symbol) since the
         cascade rebuild, so it derives to the coherent-bound default rather
         than the retired arm rule's "largest divisor of sps in {4,2,1}". */
      DP_CHECK (dp_async_dsss_receiver_get_n (rx) == MPSK_RX_M_OUT_DEFAULT);
      DP_CHECK (dp_async_dsss_receiver_get_chip_phase (rx) == 0.0);
      DP_CHECK (dp_async_dsss_receiver_get_code_rate (rx) == 1.0);
      dp_async_dsss_receiver_destroy (rx);
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

  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }

  float _Complex *syms;
  size_t          n_syms = _stream (rx, x, n, te, &syms);

  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (dp_async_dsss_receiver_get_refining (rx) == 0);
  DP_CHECK (n_syms > 20);
  DP_CHECK (dp_async_dsss_receiver_get_cn0_dbhz_est (rx) > 0.0);

  double ber = _best_ber (syms, n_syms, data, n_sym + 4);
  DP_CHECK (ber < 0.05);
  /* Truth-free corroboration: a real lock, not a lucky BER lag/polarity. */
  DP_CHECK (dp_test_evm_db_hard (syms, n_syms) < -8.0);
  DP_CHECK (dp_test_m2m4_snr_db (syms, n_syms) > 8.0);

  /* ── state-serialization round trip, while tracking ─────────────────── */
  size_t cb   = dp_async_dsss_receiver_state_bytes (rx);
  void  *blob = malloc (cb);
  dp_async_dsss_receiver_get_state (rx, blob);

  dp_async_dsss_receiver_state_t *rx2 = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false);
  DP_CHECK (rx2 != NULL);
  if (rx2)
    {
      DP_CHECK (dp_async_dsss_receiver_set_state (rx2, blob) == DP_OK);
      DP_CHECK (dp_async_dsss_receiver_get_tracking (rx2) == 1);
      DP_CHECK (fabs (dp_async_dsss_receiver_get_chip_phase (rx2)
                      - dp_async_dsss_receiver_get_chip_phase (rx))
                < 1e-9);

      /* a corrupted envelope must be rejected, not reinterpreted. */
      ((char *)blob)[0] ^= (char)0xFF;
      DP_CHECK (dp_async_dsss_receiver_set_state (rx2, blob)
                == DP_ERR_INVALID);
      dp_async_dsss_receiver_destroy (rx2);
    }
  free (blob);

  /* ── state-serialization round trip, while searching ─────────────────── */
  dp_async_dsss_receiver_state_t *rx3 = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false);
  DP_CHECK (rx3 != NULL);
  if (rx3)
    {
      size_t cb3   = dp_async_dsss_receiver_state_bytes (rx3);
      void  *blob3 = malloc (cb3);
      dp_async_dsss_receiver_get_state (rx3, blob3);

      dp_async_dsss_receiver_state_t *rx4 = dp_async_dsss_receiver_create (
          CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0,
          0.5, 4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false);
      DP_CHECK (rx4 != NULL);
      if (rx4)
        {
          DP_CHECK (dp_async_dsss_receiver_set_state (rx4, blob3) == DP_OK);
          DP_CHECK (dp_async_dsss_receiver_get_tracking (rx4) == 0);
          DP_CHECK (dp_async_dsss_receiver_get_refining (rx4) == 0);
          dp_async_dsss_receiver_destroy (rx4);
        }
      free (blob3);
      dp_async_dsss_receiver_destroy (rx3);
    }

  /* ── reset() returns to searching ─────────────────────────────────────── */
  dp_async_dsss_receiver_reset (rx);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 0);
  DP_CHECK (dp_async_dsss_receiver_get_refining (rx) == 0);
  DP_CHECK (dp_async_dsss_receiver_get_chip_phase (rx) == 0.0);

  free (syms);
  free (x);
  free (data);
  dp_async_dsss_receiver_destroy (rx);
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

  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 16, 4, true, 1 /* refine_max_n_blocks: forces give-up */,
      0.0 /* carrier_freq_hz: aiding off */, 0.0, false);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }

  float _Complex *syms; /* _stream() allocates it; see its three siblings */
  size_t          n_syms = _stream (rx, x, n, te, &syms);

  DP_CHECK (dp_async_dsss_receiver_get_refining (rx) == 0);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  /* Give-up: doppler estimate stays the UNREFINED coarse handoff value --
   * 0.0 Hz here, since _make_signal() injects no real Doppler offset. */
  DP_CHECK (fabs (dp_async_dsss_receiver_get_doppler_hz (rx) - 0.0) < 1e-6);
  /* Still tracking/decoding despite the unrefined seed -- the give-up
   * path must not otherwise break the object. */
  DP_CHECK (n_syms > 20);

  free (syms);
  free (x);
  free (data);
  dp_async_dsss_receiver_destroy (rx);
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

  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      code, sf, chip_rate, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0,
      0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false);
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

  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (n_syms > (n_sym / 2));
  DP_CHECK (ber < 0.05);
  /* Truth-free corroboration under the ramp: a real lock, not a lucky lag. */
  DP_CHECK (dp_test_evm_db_hard (syms, n_syms) < -8.0);
  DP_CHECK (dp_test_m2m4_snr_db (syms, n_syms) > 8.0);

  free (syms);
  free (x);
  free (data);
  free (code);
  dp_async_dsss_receiver_destroy (rx);
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
 * acquire. dp_acq_build_handoff() now advances the phase by the drift over
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
      dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
          code, sf, chip_rate, sym_rate, spc, 2, cn0, 1e-2, 0.9,
          1.2 * ppm * 1e-6 * carrier, 4, 8, 0, 0.5, 4, margin_db, 64, 8, false,
          100000, carrier, 0.0, false);
      DP_REQUIRE (rx != NULL);
      float _Complex *syms;
      size_t          n_syms = _stream (rx, x, n, te, &syms);
      double          ber    = _best_ber (syms, n_syms, data, n_sym + 4);
      printf ("  hand-over at %+.0f ppm, %.0f dB-Hz, seed %u: tracking %d, "
              "code %d, symbol %d, %zu symbols, BER %.3f, Doppler est "
              "%.0f Hz (truth %.0f), chip %.2f\n",
              sign * ppm, cn0, seed, dp_async_dsss_receiver_get_tracking (rx),
              dp_async_dsss_receiver_get_code_locked (rx),
              dp_async_dsss_receiver_get_locked (rx), n_syms, ber,
              dp_async_dsss_receiver_get_doppler_hz (rx),
              sign * ppm * 1e-6 * carrier,
              dp_async_dsss_receiver_get_chip_phase (rx));
      /* The hand-over's own claim, per seed: the live chain locks the
         dilated code. The carrier and the decode ride on the refine's
         Doppler; on the shipped look-back (0.5 dB, eleven dumps per
         epoch) it lands within tens of Hz and every seed decodes. On
         the retired 100 dB look-back this test used to pass, the
         estimate keeps a third of the seed's error (#1252, measured by
         validate_refine_bias) and only a majority decoded. */
      DP_CHECK_MSG (dp_async_dsss_receiver_get_tracking (rx) == 1
                        && dp_async_dsss_receiver_get_code_locked (rx) == 1,
                    "the live chain locks the dilated code from the "
                    "hand-over");
      decoded += dp_async_dsss_receiver_get_locked (rx) == 1
                 && n_syms > n_sym / 2 && ber < 0.05;
      free (syms);
      free (x);
      free (data);
      dp_async_dsss_receiver_destroy (rx);
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

  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      code, sf, chip_rate, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0,
      0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false);
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

  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  double dh = dp_async_dsss_receiver_get_doppler_hz (rx);
  DP_CHECK (isfinite (dh));

  free (syms);
  free (x);
  free (data);
  free (code);
  dp_async_dsss_receiver_destroy (rx);
  return 0;
}

/* AWGN-only Es/N0 decode floor at SPEC's own geometry (Gold-1023, 3.069
 * Mcps, 2700 bps asynchronous BPSK, spc=2), ZERO Doppler -- characterizes
 * the decode floor, independent of Doppler and of the aiding path.
 * Generated with the wfmgen C API (dp_wfm_cont_dsss_chips) so this test
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

  uint8_t *code = malloc (sf);
  dp_dsss_gold_1023 (code);
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
      dp_wfm_cont_dsss_chips (code, sf, dbits, n_data, cps, n_chips, chips);

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

      dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
          code, sf, chip_rate, sym_rate, spc, 2, cn0, 1e-3, 0.9, 100.0, 4, 8,
          0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false);
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
              esn0_pts[p], cn0,
              rx ? dp_async_dsss_receiver_get_tracking (rx) : 0, ber, evm,
              snr);

      free (syms);
      dp_async_dsss_receiver_destroy (rx);
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

/* The CODE7 fixture's seeded receiver: the cell mode with every number the
 * searching-flavor tests use, minus the search half-range, plus the release
 * interval -- the lifecycle tests below ran on the hand-off flavor until it
 * was retired (docs/design/async-dsss-receiver-measurements.md §12.28), and
 * the lifecycle past the seed is the same object's. */
static dp_async_dsss_receiver_state_t *
_seeded_rx (double cn0, double lost_confirm_s)
{
  return dp_async_dsss_receiver_create_cell (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, cn0, 1e-2, 0.9, 4, 8, 0, 0.0,
      lost_confirm_s, 100, ASYNC_DSSS_RX_CELL_GAIN, ASYNC_DSSS_RX_CELL_PULLIN);
}

/* Cell mode, section 11.1: no search of its own. Idle consumes and
 * discards; a seed -- here the truth, since the capture puts chip 0 on its
 * first signal sample with no Doppler -- starts the refine -> track chain
 * the searching flavor runs; a second seed is refused until reset(), which
 * returns to idle, and the object decodes again from the next seed. */
static int
_test_cell_seed_and_decode (void)
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

  dp_async_dsss_receiver_state_t *rx = _seeded_rx (cn0, 0.0);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }
  DP_CHECK (dp_async_dsss_receiver_get_idle (rx) == 1);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 0);
  DP_CHECK (dp_async_dsss_receiver_get_refining (rx) == 0);
  DP_CHECK (dp_async_dsss_receiver_get_lost (rx) == 0);
  DP_CHECK (rx->acq == NULL); /* no engine was built */
  DP_CHECK (dp_async_dsss_receiver_configure_search_raw (rx, 1, 1) == -1);

  /* Idle: the whole capture -- the emitter included, at chip 0 -- fed in
   * odd-sized blocks, changes nothing and emits nothing; a receiver that
   * were quietly running its chain would emit symbols from the signal. */
  float _Complex tmp[1021];
  size_t n_idle = 0;
  for (size_t pos = 0; pos < n; pos += 1021)
    {
      size_t take = (pos + 1021 <= n) ? 1021 : n - pos;
      n_idle += dp_async_dsss_receiver_steps (rx, x + pos, take, tmp, 1021);
    }
  DP_CHECK (n_idle == 0);
  DP_CHECK (dp_async_dsss_receiver_get_idle (rx) == 1);
  DP_CHECK (dp_async_dsss_receiver_get_chip_phase (rx) == 0.0);
  DP_CHECK (rx->state_samples == (uint64_t)n);

  /* A Doppler at or past half the sample rate (1 Mchip/s at 4 samples a
     chip: 2 MHz) is no reportable Doppler, and near 1e308 it NaN'd the
     hand-off's phase (doppler#2103): refused, and the receiver stays idle. */
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 2.0e6, cn0)
            == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, -1e308, cn0)
            == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_get_idle (rx) == 1);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  DP_CHECK (dp_async_dsss_receiver_get_idle (rx) == 0);
  DP_CHECK (dp_async_dsss_receiver_get_refining (rx) == 1);
  DP_CHECK (dp_async_dsss_receiver_get_doppler_hz (rx) == 0.0);
  DP_CHECK (dp_async_dsss_receiver_get_cn0_dbhz_est (rx) == cn0);
  /* Assigned once: refused while refining ... */
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 1.0, 0.0, cn0) == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_get_refining (rx) == 1);
  DP_CHECK (dp_async_dsss_receiver_get_doppler_hz (rx) == 0.0);

  float _Complex *syms;
  size_t n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (n_syms > 20);
  DP_CHECK (_best_ber (syms, n_syms, data, n_sym + 4) < 0.05);
  DP_CHECK (dp_test_evm_db_hard (syms, n_syms) < -8.0);
  DP_CHECK (dp_test_m2m4_snr_db (syms, n_syms) > 8.0);
  /* ... and while tracking. */
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  free (syms);

  /* reset() is the release: back to idle, not to a search it has not got,
   * and the same object takes its next seed. */
  dp_async_dsss_receiver_reset (rx);
  DP_CHECK (dp_async_dsss_receiver_get_idle (rx) == 1);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 0);
  DP_CHECK (dp_async_dsss_receiver_get_chip_phase (rx) == 0.0);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (_best_ber (syms, n_syms, data, n_sym + 4) < 0.05);
  free (syms);

  free (x);
  free (data);
  dp_async_dsss_receiver_destroy (rx);
  return 0;
}

/* Records a probe has written, from the context's own table (the
 * post-decimation, post-drop count), or UINT64_MAX when the name is not
 * registered -- so a missing probe and a silent one read differently. */
static uint64_t
_tlm_emitted (const dp_tlm_t *t, const char *name)
{
  for (uint32_t i = 0; i < t->n_probes; i++)
    if (strcmp (t->probes[i].name, name) == 0)
      return t->probes[i].emitted;
  return UINT64_MAX;
}

/* set_telemetry is a forwarder, and its contract is what the forwarding
 * survives. A seed destroys the Dll, the resampler and the demodulator and
 * builds new ones, so an attach made once must be re-applied to the new
 * loops; the hold copies car_held over car, so car_held must carry it too;
 * and costas_update() emits nothing, so the receiver must flush the
 * carrier's probes itself. Each of those, if missed, leaves a probe silent
 * that this test requires to keep counting. */
static int
_test_telemetry_follows_the_loops (void)
{
  const size_t sf = 7, spc = 4;
  const double fs          = 1.0e6 * (double)spc;
  const double tsym        = fs / 35714.29;
  const size_t te          = sf * spc;
  const size_t n_sym       = 400;
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 70.0;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 7,
                   &x, &n, &data);
  dp_async_dsss_receiver_state_t *rx = _seeded_rx (cn0, 0.0);
  dp_tlm_t                       *t  = dp_tlm_create (1 << 18);
  DP_CHECK (rx != NULL && t != NULL);
  if (!rx || !t)
    return 1;

  /* Refusals register nothing: decim 0, and a prefix that leaves even the
     receiver's first-level names ("<p>.car") no room. */
  DP_CHECK (dp_async_dsss_receiver_set_telemetry (rx, t, "rx", 0)
            == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_set_telemetry (
                rx, t, "abcdefghijklmnopqrstuvwxyz0123", 1)
            == DP_ERR_INVALID);
  DP_CHECK (dp_tlm_probe_count (t) == 0);
  /* A prefix the carrier's names fit ("<p>.car.locked") and the
     demodulator's deepest does not ("<p>.demod.agc.level_db"): the loops
     are attached to a scratch context first, which finds the refusal, so
     nothing registers here either -- no dead slots (#1897). */
  DP_CHECK (dp_async_dsss_receiver_set_telemetry (rx, t, "abcdefghijklmnop", 1)
            == DP_ERR_INVALID);
  DP_CHECK (dp_tlm_probe_count (t) == 0);
  DP_CHECK (rx->tlm == NULL);

  DP_CHECK (dp_async_dsss_receiver_set_telemetry (rx, t, "rx", 1) == DP_OK);
  const size_t n_probes = dp_tlm_probe_count (t);
  /* The hold restores car from car_held on entering a coast, and refreshes
     car_held only while both flags are up: both must carry the attachment,
     or an attach made with one flag down is undone by the next coast. */
  DP_CHECK (rx->car.tlm.ctx == t && rx->car_held.tlm.ctx == t);
  /* A cell receiver: the carrier, the code loop, the demodulator -- and no
     search, so no "rx.acq". The loop-filter outputs are among them, both
     views of each (the integrator, ".freq"/".rate", and the whole command,
     ".nco"), and so is the receiver's own symbol-lock decision. */
  const char *outputs[]
      = { "rx.car.freq", "rx.car.nco",        "rx.code.rate",
          "rx.code.nco", "rx.demod.car.freq", "rx.demod.sync.rate",
          "rx.sym.lock", "rx.sym.locked" };
  enum
  {
    N_OUT = sizeof outputs / sizeof outputs[0]
  };
  for (size_t i = 0; i < N_OUT; i++)
    DP_CHECK_MSG (_tlm_emitted (t, outputs[i]) == 0, outputs[i]);
  DP_CHECK (_tlm_emitted (t, "rx.acq.stat") == UINT64_MAX);

  /* Tracking: every loop's output is reporting. */
  float _Complex *syms;
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  size_t n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  free (syms);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1 && n_syms > 20);
  /* The symbol-lock pair: one record each per emitted symbol, and the
     last of each is the receiver's own decision as it stands. */
  DP_CHECK (_tlm_emitted (t, "rx.sym.lock") == n_syms);
  DP_CHECK (_tlm_emitted (t, "rx.sym.locked") == n_syms);
  {
    const int     id_lock   = dp_tlm_probe_id (t, "rx.sym.lock");
    const int     id_locked = dp_tlm_probe_id (t, "rx.sym.locked");
    dp_tlm_rec_t *recs      = malloc ((1u << 18) * sizeof *recs);
    DP_REQUIRE (recs != NULL);
    const size_t n_rec     = dp_tlm_read (t, 1u << 18, recs, 1u << 18);
    float        last_lock = NAN, last_locked = NAN;
    for (size_t j = 0; j < n_rec; j++)
      {
        if (recs[j].probe == id_lock)
          last_lock = recs[j].value;
        else if (recs[j].probe == id_locked)
          last_locked = recs[j].value;
      }
    DP_CHECK (last_lock == (float)dp_async_dsss_receiver_get_lock_metric (rx));
    DP_CHECK (last_locked == (float)dp_async_dsss_receiver_get_locked (rx));
    DP_CHECK (dp_async_dsss_receiver_get_locked (rx) == 1);
    free (recs);
  }
  uint64_t before[N_OUT];
  for (size_t i = 0; i < N_OUT; i++)
    {
      before[i] = _tlm_emitted (t, outputs[i]);
      DP_CHECK_MSG (before[i] > 0 && before[i] != UINT64_MAX, outputs[i]);
      if (before[i] == 0 || before[i] == UINT64_MAX)
        DP_TEST_ERR ("  %s wrote %llu records while tracking\n", outputs[i],
                     (unsigned long long)before[i]);
    }

  /* reset() and a new seed rebuild the chain twice: the new loops report
     under the same names, so the same ids and nothing new registers. */
  dp_async_dsss_receiver_reset (rx);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  free (syms);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (dp_tlm_probe_count (t) == n_probes);
  for (size_t i = 0; i < N_OUT; i++)
    {
      const uint64_t now = _tlm_emitted (t, outputs[i]);
      DP_CHECK_MSG (now > before[i] && now != UINT64_MAX, outputs[i]);
      if (now <= before[i] || now == UINT64_MAX)
        DP_TEST_ERR ("  %s: %llu records after the re-seed, %llu before\n",
                     outputs[i], (unsigned long long)now,
                     (unsigned long long)before[i]);
      before[i] = now;
    }

  /* A restore keeps the attachment: the blob carries none, and the
     children keep theirs live. */
  void *blob = malloc (dp_async_dsss_receiver_state_bytes (rx));
  dp_async_dsss_receiver_get_state (rx, blob);
  DP_CHECK (dp_async_dsss_receiver_set_state (rx, blob) == DP_OK);
  free (blob);
  n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  free (syms);
  for (size_t i = 0; i < N_OUT; i++)
    {
      const uint64_t now = _tlm_emitted (t, outputs[i]);
      DP_CHECK_MSG (now > before[i] && now != UINT64_MAX, outputs[i]);
      if (now <= before[i] || now == UINT64_MAX)
        DP_TEST_ERR ("  %s: %llu records after the restore, %llu before\n",
                     outputs[i], (unsigned long long)now,
                     (unsigned long long)before[i]);
      before[i] = now;
    }

  /* A refused re-attach leaves the receiver as it was, as a leaf's does:
     the dry run moves every loop off `t`, so the refusal must put them
     back. A second context and a prefix only the demodulator's deepest
     name refuses: nothing registers there, and `t` keeps counting. */
  dp_tlm_t *t2 = dp_tlm_create (1 << 10);
  DP_CHECK (t2 != NULL);
  DP_CHECK (
      dp_async_dsss_receiver_set_telemetry (rx, t2, "abcdefghijklmnop", 1)
      == DP_ERR_INVALID);
  DP_CHECK (dp_tlm_probe_count (t2) == 0);
  DP_CHECK (dp_tlm_probe_count (t) == n_probes);
  DP_CHECK (rx->tlm == t && strcmp (rx->tlm_prefix, "rx") == 0);
  DP_CHECK (rx->car.tlm.ctx == t && rx->car_held.tlm.ctx == t);
  n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  free (syms);
  for (size_t i = 0; i < N_OUT; i++)
    {
      const uint64_t now = _tlm_emitted (t, outputs[i]);
      DP_CHECK_MSG (now > before[i] && now != UINT64_MAX, outputs[i]);
      if (now <= before[i] || now == UINT64_MAX)
        DP_TEST_ERR ("  %s: %llu records after a refused re-attach, %llu "
                     "before\n",
                     outputs[i], (unsigned long long)now,
                     (unsigned long long)before[i]);
      before[i] = now;
    }
  dp_tlm_destroy (t2);

  /* A table one slot short of the attach is refused with nothing
     registered (the scratch knows every name, so it knows the count), and
     one with exactly enough room takes it. Filler names never collide with
     the receiver's. */
  for (size_t room = n_probes - 1; room <= n_probes; room++)
    {
      dp_tlm_t *full = dp_tlm_create (1 << 10);
      DP_CHECK (full != NULL);
      if (!full)
        break;
      for (size_t i = 0; i < DP_TLM_MAX_PROBES - room; i++)
        {
          char nm[DP_TLM_NAME_MAX];
          (void)snprintf (nm, sizeof nm, "fill.%zu", i);
          DP_CHECK (dp_tlm_probe (full, nm, 1) >= 0);
        }
      const size_t before_n = dp_tlm_probe_count (full);
      const int r = dp_async_dsss_receiver_set_telemetry (rx, full, "rx", 1);
      if (room < n_probes)
        {
          DP_CHECK_MSG (r == DP_ERR_INVALID, "one slot short attached");
          DP_CHECK_MSG (dp_tlm_probe_count (full) == before_n,
                        "a refused attach left names behind");
          DP_CHECK (rx->tlm == t);
        }
      else
        {
          DP_CHECK_MSG (r == DP_OK, "exactly enough room was refused");
          DP_CHECK (dp_tlm_probe_count (full) == DP_TLM_MAX_PROBES);
          DP_CHECK (rx->tlm == full);
          DP_CHECK (dp_async_dsss_receiver_set_telemetry (rx, t, "rx", 1)
                    == DP_OK);
        }
      dp_tlm_destroy (full);
    }
  DP_CHECK (rx->tlm == t && dp_tlm_probe_count (t) == n_probes);

  /* NULL detaches every loop: the live chain first -- a rebuild would hand
     back fresh, unattached loops and hide a detach that missed one -- and
     car_held with it, or a later coast would hand the carrier a context
     the caller may have destroyed. Then across a rebuild too. */
  DP_CHECK (dp_async_dsss_receiver_set_telemetry (rx, NULL, NULL, 1) == DP_OK);
  DP_CHECK (rx->tlm == NULL);
  DP_CHECK (rx->car.tlm.ctx == NULL && rx->car_held.tlm.ctx == NULL);
  n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  free (syms);
  for (size_t i = 0; i < N_OUT; i++)
    DP_CHECK_MSG (_tlm_emitted (t, outputs[i]) == before[i], outputs[i]);
  dp_async_dsss_receiver_reset (rx);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  free (syms);
  for (size_t i = 0; i < N_OUT; i++)
    DP_CHECK_MSG (_tlm_emitted (t, outputs[i]) == before[i], outputs[i]);

  dp_async_dsss_receiver_destroy (rx);
  dp_tlm_destroy (t);
  free (x);
  free (data);
  return 0;
}

/* The searching flavor carries a fourth instrumented child, its search
 * engine, which no rebuild touches: attached with the loops, kept by a
 * refused re-attach, and detached with them -- or a searching receiver
 * would keep a context the caller may destroy. */
static int
_test_telemetry_reaches_the_search (void)
{
  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5, 4,
      14.0, 32, 8, false, 100000, 0.0, 0.0, false);
  dp_tlm_t *t  = dp_tlm_create (1 << 12);
  dp_tlm_t *t2 = dp_tlm_create (1 << 12);
  DP_CHECK (rx != NULL && rx->acq != NULL && t != NULL && t2 != NULL);
  if (!rx || !rx->acq || !t || !t2)
    return 1;
  DP_CHECK (dp_async_dsss_receiver_set_telemetry (rx, t, "rx", 1) == DP_OK);
  DP_CHECK (rx->acq->tlm.ctx == t);
  DP_CHECK (dp_tlm_probe_id (t, "rx.acq.stat") >= 0);
  DP_CHECK (
      dp_async_dsss_receiver_set_telemetry (rx, t2, "abcdefghijklmnop", 1)
      == DP_ERR_INVALID);
  DP_CHECK (rx->acq->tlm.ctx == t && dp_tlm_probe_count (t2) == 0);
  DP_CHECK (dp_async_dsss_receiver_set_telemetry (rx, NULL, NULL, 1) == DP_OK);
  DP_CHECK (rx->acq->tlm.ctx == NULL);
  dp_async_dsss_receiver_destroy (rx);
  dp_tlm_destroy (t2);
  dp_tlm_destroy (t);
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

  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }
  DP_CHECK (dp_async_dsss_receiver_get_idle (rx)
            == 0); /* searching, not idle */

  /* Range: the code has 7 chips, so 7.0 is one past the end. */
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 7.0, 0.0, cn0) == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, -0.5, 0.0, cn0)
            == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, NAN, 0.0, cn0) == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, INFINITY, cn0)
            == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_get_refining (rx) == 0); /* untouched */

  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  DP_CHECK (dp_async_dsss_receiver_get_refining (rx) == 1);

  float _Complex *syms;
  size_t n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (n_syms > 20);
  DP_CHECK (_best_ber (syms, n_syms, data, n_sym + 4) < 0.05);
  DP_CHECK (dp_test_evm_db_hard (syms, n_syms) < -8.0);

  /* reset() on this flavor is still the search. */
  dp_async_dsss_receiver_reset (rx);
  DP_CHECK (dp_async_dsss_receiver_get_idle (rx) == 0);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 0);

  free (syms);
  free (x);
  free (data);
  dp_async_dsss_receiver_destroy (rx);
  return 0;
}

/* Feed `x` in `chunk`-sample blocks to a tracking receiver, keeping the
 * release clock's own book: the longest run of both flags down, in samples,
 * at the moment lost first reads 1 (or at the end). Returns 1 if lost
 * fired. */
static int
_feed_until_lost (dp_async_dsss_receiver_state_t *rx, const float _Complex *x,
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
      (void)dp_async_dsss_receiver_steps (rx, x + pos, take, tmp, chunk);
      fed += take;
      lost = dp_async_dsss_receiver_get_lost (rx);
      /* Read AFTER the call, as the object does; a lost receiver's flags
       * are frozen where they were. */
      if (dp_async_dsss_receiver_get_code_locked (rx)
          || dp_async_dsss_receiver_get_locked (rx))
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
      dp_async_dsss_receiver_state_t *rx = _seeded_rx (cn0, intervals[k]);
      DP_CHECK (rx != NULL);
      if (!rx)
        continue;
      DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
      float _Complex *syms;
      size_t          n_syms
          = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
      free (syms);
      DP_CHECK (n_syms > 20);
      DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
      DP_CHECK (dp_async_dsss_receiver_get_code_locked (rx) == 1);
      DP_CHECK (dp_async_dsss_receiver_get_locked (rx) == 1);
      DP_CHECK (dp_async_dsss_receiver_get_lost (rx) == 0);

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
          /* Nor late (#2461 #15): the clock is read after every call, so
           * the release lands within one call past the interval. */
          DP_CHECK (run <= rx->lost_confirm_samples + (uint64_t)chunk);
          DP_CHECK (fed < (uint64_t)tail_n); /* well inside the tail */
          DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 0);

          /* Lost is inert: samples are discarded, a seed is refused, the
           * blob carries the state, and reset() gives the object back. */
          float _Complex tmp[64];
          double chip_before = dp_async_dsss_receiver_get_chip_phase (rx);
          DP_CHECK (
              dp_async_dsss_receiver_steps (rx, x + pre_silence, 64, tmp, 64)
              == 0);
          DP_CHECK (dp_async_dsss_receiver_get_lost (rx) == 1);
          DP_CHECK (dp_async_dsss_receiver_get_chip_phase (rx) == chip_before);
          DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0)
                    == DP_ERR_INVALID);

          size_t cb   = dp_async_dsss_receiver_state_bytes (rx);
          void  *blob = malloc (cb);
          dp_async_dsss_receiver_get_state (rx, blob);
          dp_async_dsss_receiver_state_t *rx2 = _seeded_rx (cn0, intervals[k]);
          DP_CHECK (rx2 != NULL);
          if (rx2)
            {
              DP_CHECK (dp_async_dsss_receiver_set_state (rx2, blob) == DP_OK);
              DP_CHECK (dp_async_dsss_receiver_get_lost (rx2) == 1);
              dp_async_dsss_receiver_destroy (rx2);
            }
          free (blob);

          dp_async_dsss_receiver_reset (rx);
          DP_CHECK (dp_async_dsss_receiver_get_lost (rx) == 0);
          DP_CHECK (dp_async_dsss_receiver_get_idle (rx) == 1);
          DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
        }
      else
        {
          /* Still tracking, on noise: the rule did not fire. */
          DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
          DP_CHECK (dp_async_dsss_receiver_get_lost (rx) == 0);
        }
      dp_async_dsss_receiver_destroy (rx);
    }

  free (off);
  free (x);
  free (data);
  return 0;
}

/* reacquire, the parallel design's release (async-dsss-parallel.md section
 * 3): a searching receiver created with it releases on symbol lock alone
 * and goes back to SEARCHING by itself -- never lost, never waiting for
 * reset() -- and locks the next signal that arrives. Three phases on one
 * object: the signal (it tracks and decodes), the emitter switched off
 * past the confirm interval (it returns to searching, and keeps searching:
 * samples are consumed, nothing is emitted), and the signal back (it
 * acquires and decodes again). The same object without reacquire goes
 * lost on the same tail, which is the flag's whole difference. And
 * reacquire with no confirm interval is refused at create: its release is
 * the only way back to searching. Sabotage: enter lost instead of
 * resetting -> phase 2 red; never release -> phase 2 red. */
static dp_async_dsss_receiver_state_t *
_searching_rx (double cn0, double sym_rate, double lost_confirm_s,
               bool reacquire)
{
  return dp_async_dsss_receiver_create (
      CODE7, 7, 1.0e6, sym_rate, 4, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5, 4,
      14.0, 32, 8, false, 100000, 0.0, lost_confirm_s, reacquire);
}

static int
_test_reacquire_after_switch_off (void)
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

  /* Refused: a release that never fires would never search again -- at
     zero, and at a confirm time under one input sample (1e-9 s), which is
     the disabled clock. One sample is the smallest accepted. */
  DP_CHECK (_searching_rx (cn0, sym_rate, 0.0, true) == NULL);
  DP_CHECK (_searching_rx (cn0, sym_rate, 1e-9, true) == NULL);
  {
    dp_async_dsss_receiver_state_t *one_sample
        = _searching_rx (cn0, sym_rate, 1.0 / fs, true);
    DP_CHECK (one_sample != NULL);
    if (one_sample)
      dp_async_dsss_receiver_destroy (one_sample);
  }

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 7,
                   &x, &n, &data);
  float _Complex *off = _noise_tail (tail_n, fs, cn0, 99);

  for (int reacquire = 1; reacquire >= 0; reacquire--)
    {
      dp_async_dsss_receiver_state_t *rx
          = _searching_rx (cn0, sym_rate, confirm_s, reacquire != 0);
      DP_CHECK (rx != NULL);
      if (!rx)
        continue;

      /* Phase 1: the signal. */
      float _Complex *syms;
      size_t          n_syms = _stream (rx, x, n, te, &syms);
      free (syms);
      DP_CHECK (n_syms > 20);
      DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
      DP_CHECK (dp_async_dsss_receiver_get_locked (rx) == 1);

      /* Phase 2: the emitter off. Watch every chunk: with reacquire the
       * state must reach searching and never read lost; without it, lost. */
      int             searched = 0, lost = 0, dropped = 0;
      size_t          fed_at_release = 0, first_drop = 0;
      float _Complex *junk = malloc (chunk * 8 * sizeof *junk);
      for (size_t pos = 0; pos < tail_n; pos += chunk)
        {
          size_t k = tail_n - pos < chunk ? tail_n - pos : chunk;
          (void)dp_async_dsss_receiver_steps (rx, off + pos, k, junk,
                                              chunk * 8);
          int st = dp_async_dsss_receiver_status (rx).state;
          lost |= st == ASYNC_DSSS_RX_LOST;
          /* The chunk symbol lock first reads down after: the release
             clock counts whole calls, from that one on. */
          if (!dropped && st == ASYNC_DSSS_RX_TRACKING
              && !dp_async_dsss_receiver_get_locked (rx))
            {
              dropped    = 1;
              first_drop = pos;
            }
          if (!searched && st == ASYNC_DSSS_RX_SEARCHING)
            fed_at_release = pos + k;
          searched |= st == ASYNC_DSSS_RX_SEARCHING;
        }
      free (junk);
      if (reacquire)
        {
          /* It reached searching and never read lost. Where it is at the
           * tail's end is not the claim: at this fixture's pfa the search
           * may take a noise hit, refine, track and release again. */
          DP_CHECK (lost == 0);
          DP_CHECK (searched == 1);
          /* On time, to the call: from the start of the chunk symbol
           * lock first read down after, the release lands past
           * lost_confirm_s and within one more call of it -- the clock
           * counts whole calls. Neither early nor late. */
          DP_CHECK (rx->lost_confirm_samples
                    == (uint64_t)llround (confirm_s * fs));
          DP_CHECK (dropped == 1);
          DP_CHECK (fed_at_release - first_drop > rx->lost_confirm_samples);
          DP_CHECK (fed_at_release - first_drop
                    <= rx->lost_confirm_samples + chunk);

          /* Phase 3: the signal back -- found again, with no reset(). */
          n_syms = _stream (rx, x, n, te, &syms);
          DP_CHECK (n_syms > 20);
          DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
          DP_CHECK (dp_async_dsss_receiver_get_locked (rx) == 1);
          double ber = _best_ber (syms, n_syms, data, n_sym + 4);
          DP_CHECK (ber < 0.05);
          free (syms);
        }
      else
        {
          DP_CHECK (lost == 1); /* the release rule as it always was */

          /* Its lost blob: a reacquire receiver cannot be lost, so it
           * refuses the blob; a plain one, the control, takes it. */
          size_t cb   = dp_async_dsss_receiver_state_bytes (rx);
          void  *blob = malloc (cb);
          dp_async_dsss_receiver_get_state (rx, blob);
          dp_async_dsss_receiver_state_t *re
              = _searching_rx (cn0, sym_rate, confirm_s, true);
          dp_async_dsss_receiver_state_t *plain
              = _searching_rx (cn0, sym_rate, confirm_s, false);
          DP_CHECK (re != NULL && plain != NULL);
          if (re && plain)
            {
              DP_CHECK (dp_async_dsss_receiver_set_state (re, blob)
                        == DP_ERR_INVALID);
              DP_CHECK (dp_async_dsss_receiver_status (re).state
                        == ASYNC_DSSS_RX_SEARCHING);
              DP_CHECK (dp_async_dsss_receiver_set_state (plain, blob)
                        == DP_OK);
              DP_CHECK (dp_async_dsss_receiver_get_lost (plain) == 1);
            }
          if (re)
            dp_async_dsss_receiver_destroy (re);
          if (plain)
            dp_async_dsss_receiver_destroy (plain);
          free (blob);
        }
      dp_async_dsss_receiver_destroy (rx);
    }
  free (off);
  free (x);
  free (data);
  return 0;
}

/* reacquire's trigger is symbol lock ALONE: a 20 kHz carrier step breaks
 * the carrier loop -- symbol lock drops and stays down -- while the
 * carrier-blind Dll keeps code lock up the whole time. That is the case
 * the two rules decide differently. With reacquire the receiver releases
 * and returns to searching though code lock is still up; without it the
 * both-flags rule never fires, and the receiver goes on tracking. Measured
 * at this fixture: code held throughout, symbol lock down for ~0.138 s
 * unbroken, past the 0.02 s interval (16 kHz re-locks the loop and 28 kHz
 * also loses code lock, so 20 kHz sits mid-band). Sabotage: give reacquire
 * the both-flags test -> its row red. */
static int
_test_reacquire_is_symbol_lock_alone (void)
{
  const size_t sf = 7, spc = 4;
  const double fs        = 1.0e6 * (double)spc;
  const double sym_rate  = 35714.29;
  const double tsym      = fs / sym_rate;
  const size_t te        = sf * spc;
  const size_t n_sym     = 6000;
  const size_t pre       = te * 5 + 3;
  const double cn0       = 70.0;
  const double confirm_s = 0.02;
  const double step_hz   = 20.0e3;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre, 7, &x, &n,
                   &data);
  const size_t    head = n / 6; /* locks in the first sixth */
  const size_t    tail = n - head;
  float _Complex *y    = malloc (tail * sizeof *y);
  for (size_t i = 0; i < tail; i++)
    y[i] = x[head + i]
           * (float _Complex)cexp (I * 2.0 * M_PI * step_hz * (double)i / fs);

  for (int reacquire = 1; reacquire >= 0; reacquire--)
    {
      dp_async_dsss_receiver_state_t *rx
          = _searching_rx (cn0, sym_rate, confirm_s, reacquire != 0);
      DP_CHECK (rx != NULL);
      if (!rx)
        continue;
      float _Complex *syms;
      (void)_stream (rx, x, head, te, &syms);
      free (syms);
      DP_CHECK (dp_async_dsss_receiver_get_code_locked (rx) == 1);
      DP_CHECK (dp_async_dsss_receiver_get_locked (rx) == 1);

      int             searched = 0, lost = 0, code_held = 1;
      size_t          run = 0, longest = 0; /* symbol lock down, samples */
      float _Complex *junk = malloc (te * 8 * sizeof *junk);
      for (size_t pos = 0; pos < tail; pos += te)
        {
          size_t k            = tail - pos < te ? tail - pos : te;
          int    was_tracking = dp_async_dsss_receiver_get_tracking (rx);
          if (was_tracking && !dp_async_dsss_receiver_get_code_locked (rx))
            code_held = 0;
          (void)dp_async_dsss_receiver_steps (rx, y + pos, k, junk, te * 8);
          if (dp_async_dsss_receiver_get_tracking (rx)
              && !dp_async_dsss_receiver_get_locked (rx))
            {
              run += k;
              longest = run > longest ? run : longest;
            }
          else
            run = 0;
          int st = dp_async_dsss_receiver_status (rx).state;
          lost |= st == ASYNC_DSSS_RX_LOST;
          searched |= st == ASYNC_DSSS_RX_SEARCHING;
          if (searched)
            break; /* released: the claim is made */
        }
      free (junk);
      /* Code lock stayed up for as long as the receiver tracked: the
       * release, when it came, was on symbol lock alone. */
      DP_CHECK (code_held == 1);
      DP_CHECK (lost == 0);
      if (reacquire)
        DP_CHECK (searched == 1);
      else
        {
          DP_CHECK (searched == 0);
          DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
          /* The case itself, measured on the run that never released:
           * symbol lock down unbroken for longer than the interval (about
           * 0.138 s against 0.02 s), with code lock up throughout. */
          DP_CHECK (longest > (size_t)llround (confirm_s * fs));
          printf ("reacquire symbol-lock-alone: symbol lock down %.4f s "
                  "unbroken, interval %.3f s\n",
                  (double)longest / fs, confirm_s);
        }
      dp_async_dsss_receiver_destroy (rx);
    }
  free (y);
  free (x);
  free (data);
  return 0;
}

/* reacquire's clock restarts on symbol lock: the emitter cut four times
 * for 0.02 s each, against a 0.03 s interval, with 0.04 s of signal between
 * cuts for the loops to re-lock. The cuts add to 0.08 s, so a clock that
 * accumulated instead of restarting would release; a clock that restarts
 * on every re-lock never reaches the interval and never does. The test
 * also requires that symbol lock really dropped in at least two of the
 * cuts, or the clock would never have counted and the claim would be
 * vacuous. Sabotage: drop the restart -> red. */
static int
_test_reacquire_clock_restarts_on_lock (void)
{
  const size_t sf = 7, spc = 4;
  const double fs        = 1.0e6 * (double)spc;
  const double sym_rate  = 35714.29;
  const double tsym      = fs / sym_rate;
  const size_t te        = sf * spc;
  const size_t n_sym     = 11000;
  const size_t pre       = te * 5 + 3;
  const double cn0       = 70.0;
  const double confirm_s = 0.03;
  const size_t head      = (size_t)(0.05 * fs); /* lock first */
  const size_t off_n     = (size_t)(0.02 * fs);
  const size_t on_n      = (size_t)(0.04 * fs);
  const int    cuts      = 4;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre, 7, &x, &n,
                   &data);
  const size_t span = head + (size_t)cuts * (off_n + on_n);
  DP_CHECK (n >= span);
  float _Complex *noise = _noise_tail (off_n, fs, cn0, 31);
  /* The emitter off in each cut: noise alone over the capture's time, so
     the signal resumes where it would have been. */
  for (int c = 0; c < cuts; c++)
    {
      size_t at = head + (size_t)c * (off_n + on_n);
      memcpy (x + at, noise, off_n * sizeof *x);
    }

  dp_async_dsss_receiver_state_t *rx
      = _searching_rx (cn0, sym_rate, confirm_s, true);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (noise);
      free (x);
      free (data);
      return 1;
    }
  float _Complex *junk       = malloc (te * 8 * sizeof *junk);
  int             dropped[4] = { 0, 0, 0, 0 }, searched = 0, locked_head = 0;
  for (size_t pos = 0; pos < span; pos += te)
    {
      size_t k = span - pos < te ? span - pos : te;
      (void)dp_async_dsss_receiver_steps (rx, x + pos, k, junk, te * 8);
      if (pos + k <= head)
        locked_head = dp_async_dsss_receiver_get_locked (rx);
      else if (dp_async_dsss_receiver_status (rx).state
               == ASYNC_DSSS_RX_SEARCHING)
        searched = 1;
      else if (!dp_async_dsss_receiver_get_locked (rx))
        {
          size_t c = (pos - head) / (off_n + on_n);
          if (c < 4)
            dropped[c] = 1;
        }
    }
  free (junk);
  DP_CHECK (locked_head == 1);
  DP_CHECK (dropped[0] + dropped[1] + dropped[2] + dropped[3] >= 2);
  DP_CHECK (searched == 0); /* never released: the clock restarted */
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  dp_async_dsss_receiver_destroy (rx);
  free (noise);
  free (x);
  free (data);
  return 0;
}

/* doppler#2414: configure_chain_raw() on a tracking receiver re-seeds the
 * rebuilt chain from the WHOLE carrier -- loop 1 plus loop 2's integrator,
 * the sum status() reports -- not loop 2's residual alone. Loop 1 holds
 * nearly all of it (measured: 16002 Hz on loop 1, -0.3 Hz on loop 2), so
 * the residual-only re-seed rebuilt the chain near 0 Hz. This fixture's
 * fast carrier loop pulls a chain rebuilt at 0 back in up to 12 kHz
 * (measured at 3, 8 and 12 kHz), so the carrier is 16 kHz, where the
 * residual-only re-seed lost lock and read -7.8 kHz. Locked, retuned onto
 * the same grid, streamed on: still symbol locked, still decoding, still
 * reading ~16 kHz. Sabotage: re-seed from the residual alone -> red. */
static int
_test_configure_chain_raw_keeps_the_carrier (void)
{
  const size_t sf = 7, spc = 4;
  const double fs         = 1.0e6 * (double)spc;
  const double sym_rate   = 35714.29;
  const double tsym       = fs / sym_rate;
  const size_t te         = sf * spc;
  const size_t n_sym      = 2000;
  const size_t pre        = te * 5 + 3;
  const double cn0        = 70.0;
  const double carrier_hz = 16000.0;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, carrier_hz, cn0, n_sym, pre, 7,
                   &x, &n, &data);
  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 20000.0, 4, 8, 0,
      0.5, 4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }
  const size_t    half = n / 2;
  float _Complex *syms;
  (void)_stream (rx, x, half, te, &syms);
  free (syms);
  DP_CHECK (dp_async_dsss_receiver_get_locked (rx) == 1);
  DP_CHECK (fabs (dp_async_dsss_receiver_status (rx).doppler_hz - carrier_hz)
            < 100.0);

  const double before = dp_async_dsss_receiver_status (rx).doppler_hz;
  DP_CHECK (dp_async_dsss_receiver_configure_chain_raw (
                rx, 4, 8, (int)dp_async_dsss_receiver_get_n (rx))
            == 0);
  /* The rebuilt chain starts where the old one was: the whole carrier
     carried across, not just loop 2's residual. */
  DP_CHECK (fabs (dp_async_dsss_receiver_status (rx).doppler_hz - before)
            < 1.0);
  size_t n_syms = _stream (rx, x + half, n - half, te, &syms);
  DP_CHECK (n_syms > 100);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (dp_async_dsss_receiver_get_locked (rx) == 1);
  DP_CHECK (fabs (dp_async_dsss_receiver_status (rx).doppler_hz - carrier_hz)
            < 100.0);
  /* The rebuilt chain decodes, not just reads locked: EVM over the last
     half, measured -8.0 dB re-seeded from the sum, -1.3 dB from the
     residual (a BER needs the data's alignment, which a mid-capture
     stream does not start at). */
  DP_CHECK (dp_test_evm_db_hard (syms + n_syms / 2, n_syms - n_syms / 2)
            < -5.0);
  free (syms);
  dp_async_dsss_receiver_destroy (rx);
  free (x);
  free (data);
  return 0;
}

/* doppler#2395: the call in which the refine decides stops folding code
 * periods into the refine at the decision. The rest of that call is the
 * tracking chain's (steps() hands it the tail), so folding it into the
 * refine as well paid for those samples twice. One capture, two receivers:
 * one fed a code period a call, one fed the whole capture in a single
 * call. Each refine processes the same number of periods (within one, the
 * carry's alignment); before the fix the single call folded every period
 * it held. Both decode. Sabotage: drop the loop's decided test -> red. */
static int
_test_refine_stops_at_its_decision (void)
{
  const size_t sf = 7, spc = 4;
  const double fs       = 1.0e6 * (double)spc;
  const double sym_rate = 35714.29;
  const double tsym     = fs / sym_rate;
  const size_t te       = sf * spc;
  const size_t n_sym    = 400;
  const size_t pre      = te * 5 + 3;
  const double cn0      = 70.0;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre, 7, &x, &n,
                   &data);
  dp_async_dsss_receiver_state_t *chunked
      = _searching_rx (cn0, sym_rate, 0.0, false);
  dp_async_dsss_receiver_state_t *whole
      = _searching_rx (cn0, sym_rate, 0.0, false);
  DP_CHECK (chunked != NULL && whole != NULL);
  if (chunked && whole)
    {
      float _Complex *s1, *s2;
      size_t          n1 = _stream (chunked, x, n, te, &s1);
      size_t          n2 = _stream (whole, x, n, n, &s2);
      DP_CHECK (dp_async_dsss_receiver_get_tracking (chunked) == 1);
      DP_CHECK (dp_async_dsss_receiver_get_tracking (whole) == 1);
      uint64_t pc = chunked->refine_periods, pw = whole->refine_periods;
      DP_CHECK (pc > 0);
      DP_CHECK ((pc > pw ? pc - pw : pw - pc) <= 1);
      /* Before the fix the single call folded the whole capture's worth. */
      DP_CHECK (pw < (uint64_t)(n / te) / 2);
      /* Same symbols, bit for bit: the fix moves cost, not output. */
      DP_CHECK (n1 == n2 && n1 > 20);
      DP_CHECK (n1 == n2 && memcmp (s1, s2, n1 * sizeof *s1) == 0);
      DP_CHECK (_best_ber (s1, n1, data, n_sym + 4) < 0.05);
      free (s1);
      free (s2);
    }
  if (chunked)
    dp_async_dsss_receiver_destroy (chunked);
  if (whole)
    dp_async_dsss_receiver_destroy (whole);
  free (x);
  free (data);
  return 0;
}

/* set_refine_min_blocks() is a mutator, so its value travels in the blob
 * (#2022, v9): a searching receiver given a non-default refine floor, then
 * serialized, restores into a default-built one with that floor. A cell
 * receiver refuses the setter, so a cell blob carrying any other floor
 * than its create()'s is refused. Sabotage: leave it out of the blob -> red.
 */
static int
_test_refine_min_blocks_travels (void)
{
  const double                    sym_rate = 35714.29, cn0 = 70.0;
  dp_async_dsss_receiver_state_t *a
      = _searching_rx (cn0, sym_rate, 0.0, false);
  dp_async_dsss_receiver_state_t *b
      = _searching_rx (cn0, sym_rate, 0.0, false);
  DP_CHECK (a != NULL && b != NULL);
  if (a && b)
    {
      const size_t floor0 = a->refine_min_blocks;
      DP_CHECK (dp_async_dsss_receiver_set_refine_min_blocks (a, floor0 + 5)
                == DP_OK);
      size_t cb   = dp_async_dsss_receiver_state_bytes (a);
      void  *blob = malloc (cb);
      dp_async_dsss_receiver_get_state (a, blob);
      DP_CHECK (b->refine_min_blocks == floor0);
      DP_CHECK (dp_async_dsss_receiver_set_state (b, blob) == DP_OK);
      DP_CHECK (b->refine_min_blocks == floor0 + 5);
      free (blob);
    }
  if (a)
    dp_async_dsss_receiver_destroy (a);
  if (b)
    dp_async_dsss_receiver_destroy (b);

  /* A cell blob forged with another floor: refused. */
  dp_async_dsss_receiver_state_t *c = _seeded_rx (cn0, 0.0);
  dp_async_dsss_receiver_state_t *d = _seeded_rx (cn0, 0.0);
  DP_CHECK (c != NULL && d != NULL);
  if (c && d)
    {
      size_t cb   = dp_async_dsss_receiver_state_bytes (c);
      void  *blob = malloc (cb);
      DP_CHECK (dp_async_dsss_receiver_set_refine_min_blocks (c, 99)
                == DP_ERR_INVALID);
      c->refine_min_blocks += 3; /* what the refused setter never allows */
      dp_async_dsss_receiver_get_state (c, blob);
      DP_CHECK (dp_async_dsss_receiver_set_state (d, blob) == DP_ERR_INVALID);
      free (blob);
    }
  if (c)
    dp_async_dsss_receiver_destroy (c);
  if (d)
    dp_async_dsss_receiver_destroy (d);
  return 0;
}

/* A hand-off that never locks is released (#2461 #76, #78): the release
 * clock runs from the first tracking sample, before either flag has come
 * up, so a track on noise -- the false alarm the receiver cannot tell from
 * a signal until a lock flag fails to come -- is let go. Each flavor is
 * seeded onto noise alone (seed() takes exactly what a hit would hand
 * over). Without reacquire it reaches lost; with reacquire it goes back to
 * searching and never reads lost. The precondition is asserted too: each
 * reached tracking, on noise, with no flag up, before it let go. And it is
 * let go on time, as the header says ("released the same way as an emitter
 * that leaves"): the samples fed since tracking began, counted here from
 * the status record's state_samples at the first tracking read, land in
 * (L, L + chunk] at the release, L = lost_confirm_samples -- #15's bound.
 * Sabotages: start the clock only once a flag has been up -> red; release
 * a never-locked track 8x late -> red. */
static int
_test_a_handoff_that_never_locks_is_released (void)
{
  const size_t sf = 7, spc = 4;
  const double fs        = 1.0e6 * (double)spc;
  const double sym_rate  = 35714.29;
  const double cn0       = 70.0;
  const double confirm_s = 0.02;
  const size_t n         = (size_t)(0.5 * fs);
  const size_t chunk     = 1024;
  (void)sf;
  float _Complex *noise = _noise_tail (n, fs, cn0, 7);
  float _Complex *junk  = malloc (chunk * 8 * sizeof *junk);

  /* rows: cell plain, searching plain, searching reacquire */
  for (int row = 0; row < 3; row++)
    {
      dp_async_dsss_receiver_state_t *rx
          = row == 0 ? _seeded_rx (cn0, confirm_s)
                     : _searching_rx (cn0, sym_rate, confirm_s, row == 2);
      DP_CHECK (rx != NULL);
      if (!rx)
        continue;
      DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
      int      tracked = 0, any_flag = 0, lost = 0, searched_after = 0;
      uint64_t since_track = 0; /* samples fed since tracking began */
      for (size_t pos = 0; pos < n; pos += chunk)
        {
          size_t k = n - pos < chunk ? n - pos : chunk;
          (void)dp_async_dsss_receiver_steps (rx, noise + pos, k, junk,
                                              chunk * 8);
          int st = dp_async_dsss_receiver_status (rx).state;
          if (tracked)
            since_track += k;
          if (st == ASYNC_DSSS_RX_TRACKING)
            {
              if (!tracked)
                since_track = dp_async_dsss_receiver_status (rx).state_samples;
              tracked = 1;
              any_flag |= dp_async_dsss_receiver_get_code_locked (rx)
                          | dp_async_dsss_receiver_get_locked (rx);
            }
          lost |= st == ASYNC_DSSS_RX_LOST;
          if (tracked && st == ASYNC_DSSS_RX_SEARCHING)
            searched_after = 1;
          if (lost || searched_after)
            break;
        }
      DP_CHECK (tracked == 1);  /* the precondition: it held a track */
      DP_CHECK (any_flag == 0); /* on noise, with no flag ever up */
      if (row == 2)
        {
          DP_CHECK (searched_after == 1);
          DP_CHECK (lost == 0);
        }
      else
        DP_CHECK (lost == 1);
      DP_CHECK (since_track > rx->lost_confirm_samples);
      DP_CHECK (since_track <= rx->lost_confirm_samples + (uint64_t)chunk);
      dp_async_dsss_receiver_destroy (rx);
    }
  free (junk);
  free (noise);
  return 0;
}

/* The plain rule's clock restarts when a flag comes back (#2461 #16): its
 * release needs BOTH flags down without a break. The emitter cut four
 * times for 0.02 s against a 0.03 s interval, 0.04 s of signal between for
 * the loops to re-lock; the cuts add to 0.08 s, so a clock that
 * accumulated would release, and one that restarts on a returning flag
 * never does. Requires that both flags really dropped together in at least
 * two cuts, or the clock never counted. Sabotage: drop the restart -> red. */
static int
_test_plain_clock_restarts_on_a_flag (void)
{
  const size_t sf = 7, spc = 4;
  const double fs        = 1.0e6 * (double)spc;
  const double sym_rate  = 35714.29;
  const double tsym      = fs / sym_rate;
  const size_t te        = sf * spc;
  const size_t n_sym     = 11000;
  const size_t pre       = te * 5 + 3;
  const double cn0       = 70.0;
  const double confirm_s = 0.03;
  const size_t head      = (size_t)(0.05 * fs);
  const size_t off_n     = (size_t)(0.02 * fs);
  const size_t on_n      = (size_t)(0.04 * fs);
  const int    cuts      = 4;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre, 7, &x, &n,
                   &data);
  const size_t span = head + (size_t)cuts * (off_n + on_n);
  DP_CHECK (n >= span);
  float _Complex *noise = _noise_tail (off_n, fs, cn0, 31);
  for (int c = 0; c < cuts; c++)
    memcpy (x + head + (size_t)c * (off_n + on_n), noise, off_n * sizeof *x);

  dp_async_dsss_receiver_state_t *rx
      = _searching_rx (cn0, sym_rate, confirm_s, false);
  DP_CHECK (rx != NULL);
  if (rx)
    {
      float _Complex *junk    = malloc (te * 8 * sizeof *junk);
      int             both[4] = { 0, 0, 0, 0 }, lost = 0;
      int             locked_head = 0;
      for (size_t pos = 0; pos < span; pos += te)
        {
          size_t k = span - pos < te ? span - pos : te;
          (void)dp_async_dsss_receiver_steps (rx, x + pos, k, junk, te * 8);
          if (pos + k <= head)
            locked_head = dp_async_dsss_receiver_get_locked (rx);
          else
            {
              lost |= dp_async_dsss_receiver_get_lost (rx);
              if (dp_async_dsss_receiver_get_tracking (rx)
                  && !dp_async_dsss_receiver_get_code_locked (rx)
                  && !dp_async_dsss_receiver_get_locked (rx))
                {
                  size_t c = (pos - head) / (off_n + on_n);
                  if (c < 4)
                    both[c] = 1;
                }
            }
        }
      free (junk);
      DP_CHECK (locked_head == 1);
      DP_CHECK (both[0] + both[1] + both[2] + both[3] >= 2);
      DP_CHECK (lost == 0);
      DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
      dp_async_dsss_receiver_destroy (rx);
    }
  free (noise);
  free (x);
  free (data);
  return 0;
}

/* The symbol-lock detector, symbol by symbol (#2461 #36, #129, #130): the
 * metric is EMA(I^2 - Q^2) / EMA(I^2 + Q^2) over the emitted symbols, with
 * alpha = 1/LOCK_DWELL from a fresh chain; `locked` declares after
 * LOCK_N_UP consecutive looks strictly above LOCK_UP and drops after
 * LOCK_N_DOWN strictly below LOCK_DOWN. The test recomputes the metric
 * from the symbols steps() returned with its own arithmetic and replays
 * the hysteresis with its own counters -- not the library's EMA or lockdet
 * -- and holds the receiver's per-symbol telemetry (rx.sym.lock,
 * rx.sym.locked) to both, over a signal that locks and a noise tail that
 * drops it (lost_confirm_s 0, so the track rides on). It requires the
 * lock to rise AND fall. A locked metric reads near +1 there, and the
 * threshold the status reports is LOCK_UP. Sabotage: declare at
 * LOCK_N_UP - 1 -> red. */
static int
_test_symbol_lock_detector_symbol_by_symbol (void)
{
  const size_t sf = 7, spc = 4;
  const double fs       = 1.0e6 * (double)spc;
  const double sym_rate = 35714.29;
  const double tsym     = fs / sym_rate;
  const size_t te       = sf * spc;
  const size_t n_sym    = 800;
  const size_t pre      = te * 5 + 3;
  const double cn0      = 70.0;
  const size_t tail_n   = (size_t)(0.02 * fs);

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre, 7, &x, &n,
                   &data);
  float _Complex                 *off = _noise_tail (tail_n, fs, cn0, 5);
  dp_async_dsss_receiver_state_t *rx
      = _searching_rx (cn0, sym_rate, 0.0, false);
  dp_tlm_t *t = dp_tlm_create (1 << 18);
  DP_REQUIRE (rx != NULL && t != NULL);
  DP_REQUIRE (dp_async_dsss_receiver_set_telemetry (rx, t, NULL, 1) == 0);
  float _Complex *s1, *s2;
  size_t          n1 = _stream (rx, x, n, te, &s1);
  size_t          n2 = _stream (rx, off, tail_n, te, &s2);
  const size_t    ns = n1 + n2;
  float _Complex *sy = malloc (ns * sizeof *sy);
  memcpy (sy, s1, n1 * sizeof *sy);
  memcpy (sy + n1, s2, n2 * sizeof *sy);

  const int     id_lock   = dp_tlm_probe_id (t, "rx.sym.lock");
  const int     id_locked = dp_tlm_probe_id (t, "rx.sym.locked");
  dp_tlm_rec_t *recs      = malloc ((1u << 18) * sizeof *recs);
  const size_t  n_rec     = dp_tlm_read (t, 1u << 18, recs, 1u << 18);
  float        *metric    = malloc (ns * sizeof *metric);
  float        *flag      = malloc (ns * sizeof *flag);
  size_t        nm = 0, nf = 0;
  for (size_t j = 0; j < n_rec; j++)
    {
      if (recs[j].probe == id_lock && nm < ns)
        metric[nm++] = recs[j].value;
      else if (recs[j].probe == id_locked && nf < ns)
        flag[nf++] = recs[j].value;
    }
  DP_CHECK (nm == ns && nf == ns);

  /* The rule, independently: EMAs from zero, the ratio, the hysteresis. */
  const double a   = 1.0 / (double)ASYNC_DSSS_RX_LOCK_DWELL;
  double       num = 0.0, den = 0.0;
  int          locked = 0, rose = 0, fell = 0, mismatch = 0;
  unsigned     cnt  = 0;
  double       peak = -1.0;
  for (size_t i = 0; i < ns && i < nm && i < nf; i++)
    {
      double re = crealf (sy[i]), im = cimagf (sy[i]);
      num += a * ((re * re - im * im) - num);
      den += a * ((re * re + im * im) - den);
      double m = den > 0.0 ? num / den : 0.0;
      if (fabs (m - (double)metric[i]) > 1e-5)
        mismatch++;
      if (!locked)
        {
          cnt = m > ASYNC_DSSS_RX_LOCK_UP ? cnt + 1 : 0;
          if (cnt >= ASYNC_DSSS_RX_LOCK_N_UP)
            locked = 1, cnt = 0, rose = 1;
        }
      else
        {
          cnt = m < ASYNC_DSSS_RX_LOCK_DOWN ? cnt + 1 : 0;
          if (cnt >= ASYNC_DSSS_RX_LOCK_N_DOWN)
            locked = 0, cnt = 0, fell = 1;
        }
      if ((float)locked != flag[i])
        mismatch++;
      if (locked && m > peak)
        peak = m;
    }
  DP_CHECK (mismatch == 0);
  DP_CHECK (rose == 1 && fell == 1); /* the replay was exercised both ways */
  DP_CHECK (peak > 0.9);             /* a locked metric reads near +1 */
  DP_CHECK (dp_async_dsss_receiver_status (rx).lock_threshold
            == ASYNC_DSSS_RX_LOCK_UP);
  free (recs);
  free (metric);
  free (flag);
  free (sy);
  free (s1);
  free (s2);
  dp_async_dsss_receiver_destroy (rx);
  dp_tlm_destroy (t);
  free (off);
  free (x);
  free (data);
  return 0;
}

/* What each state emits, and destroy(NULL) (#2461 #9, #110, #89, #96).
 * The searching flavor emits nothing in a call it spends refining; the
 * cell flavor's refining is the pull-in on the live chain, so it DOES
 * emit; a cell receiver reaches tracking only once its pull-in has folded
 * the carrier estimate in; and destroy(NULL) is a no-op. Each asserts its
 * precondition (calls observed in the state). Sabotage: the searching
 * flavor emitting from the refine chain, or the cell's pull-in emitting
 * nothing -> red. */
static int
_test_emission_by_state (void)
{
  const size_t sf = 7, spc = 4;
  const double fs       = 1.0e6 * (double)spc;
  const double sym_rate = 35714.29;
  const double tsym     = fs / sym_rate;
  const size_t te       = sf * spc;
  const size_t n_sym    = 400;
  const size_t pre      = te * 5 + 3;
  const double cn0      = 70.0;

  dp_async_dsss_receiver_destroy (NULL); /* #96: safe, does nothing */

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre, 7, &x, &n,
                   &data);
  float _Complex *out = malloc (te * 8 * sizeof *out);

  /* Searching flavor: every call that ends refining emitted 0 (#110). */
  dp_async_dsss_receiver_state_t *rx
      = _searching_rx (cn0, sym_rate, 0.0, false);
  DP_REQUIRE (rx != NULL);
  int refining_calls = 0, refining_emitted = 0, tracked = 0;
  for (size_t pos = 0; pos + te <= n; pos += te)
    {
      size_t k = dp_async_dsss_receiver_steps (rx, x + pos, te, out, te * 8);
      if (dp_async_dsss_receiver_status (rx).state == ASYNC_DSSS_RX_REFINING)
        {
          refining_calls++;
          refining_emitted += (int)k;
        }
      tracked |= dp_async_dsss_receiver_get_tracking (rx);
    }
  DP_CHECK (refining_calls > 0); /* precondition: it was seen refining */
  DP_CHECK (refining_emitted == 0);
  DP_CHECK (tracked == 1);
  dp_async_dsss_receiver_destroy (rx);

  /* Cell flavor, seeded on the signal: refining (the pull-in) emits (#9),
     and tracking comes only after the fold (#89). */
  dp_async_dsss_receiver_state_t *cell = _seeded_rx (cn0, 0.0);
  DP_REQUIRE (cell != NULL);
  DP_CHECK (dp_async_dsss_receiver_seed (cell, 0.0, 0.0, cn0) == DP_OK);
  int cell_refining_calls = 0, cell_refining_emitted = 0, early = 0,
      cell_tracked = 0;
  for (size_t pos = pre; pos + te <= n; pos += te)
    {
      size_t k = dp_async_dsss_receiver_steps (cell, x + pos, te, out, te * 8);
      int    st = dp_async_dsss_receiver_status (cell).state;
      if (st == ASYNC_DSSS_RX_REFINING)
        {
          cell_refining_calls++;
          cell_refining_emitted += (int)k;
        }
      if (st == ASYNC_DSSS_RX_TRACKING)
        {
          cell_tracked = 1;
          early |= !cell->cell_refined; /* tracking before the fold */
        }
    }
  DP_CHECK (cell_refining_calls > 0);
  DP_CHECK (cell_refining_emitted > 0);
  DP_CHECK (cell_tracked == 1);
  DP_CHECK (early == 0);
  dp_async_dsss_receiver_destroy (cell);
  free (out);
  free (x);
  free (data);
  return 0;
}

/* Output capacity (#2461 #108, #115): steps() never writes past max_out,
 * and the symbols past it are DROPPED, not held for the next call. Two
 * receivers run in lockstep on one capture: B always has room for the
 * call's x_len (enough on this fixture, at about 0.25 symbols a code
 * period -- not in general, #2469); A has the same, except on the first
 * call where B emits several symbols, where A gets capacity 1. A must keep
 * exactly B's first symbol there, leave a guard region past max_out
 * untouched, and then match B bit for bit on every later call -- a
 * deferral would show up as extra symbols in A's next call. Sabotages:
 * write one past max_out -> red; hold the overflow for the next call ->
 * red. */
static int
_test_output_capacity (void)
{
  const size_t    sf = 7, spc = 4;
  const double    fs       = 1.0e6 * (double)spc;
  const double    sym_rate = 35714.29;
  const double    tsym     = fs / sym_rate;
  const size_t    te       = sf * spc;
  const size_t    n_sym    = 400;
  const size_t    pre      = te * 5 + 3;
  const size_t    chunk    = 512;
  const size_t    guard    = 16;
  float _Complex *x;
  size_t          n;
  double         *data;
  int             fails = 0;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, 70.0, n_sym, pre, 7, &x, &n,
                   &data);
  dp_async_dsss_receiver_state_t *a
      = _searching_rx (70.0, sym_rate, 0.0, false);
  dp_async_dsss_receiver_state_t *b
      = _searching_rx (70.0, sym_rate, 0.0, false);
  DP_REQUIRE (a != NULL && b != NULL);
  DP_CHECK (dp_async_dsss_receiver_steps_max_out (a) == 0);
  float _Complex *oa      = malloc ((chunk + guard) * sizeof *oa);
  float _Complex *ob      = malloc (chunk * sizeof *ob);
  int             starved = 0, after = 0, wrote_past = 0;
  size_t          total_b = 0;
  for (size_t pos = 0; pos < n; pos += chunk)
    {
      size_t kk = n - pos < chunk ? n - pos : chunk;
      size_t kb = dp_async_dsss_receiver_steps (b, x + pos, kk, ob, chunk);
      total_b += kb;
      size_t cap = (!starved && kb >= 2) ? 1 : chunk;
      for (size_t g = 0; g < guard; g++)
        oa[cap + g] = 12345.0f;
      size_t ka = dp_async_dsss_receiver_steps (a, x + pos, kk, oa, cap);
      for (size_t g = 0; g < guard; g++)
        wrote_past |= oa[cap + g] != 12345.0f;
      if (cap == 1)
        {
          starved = 1;
          DP_CHECK (ka == 1);
          DP_CHECK (memcmp (oa, ob, sizeof *oa) == 0);
          continue;
        }
      if (ka != kb || memcmp (oa, ob, ka * sizeof *oa) != 0)
        {
          DP_TEST_ERR ("  call at %zu: A gave %zu, B %zu%s\n", pos, ka, kb,
                       starved ? " (after the starved call)" : "");
          fails++;
        }
      after += starved;
    }
  DP_CHECK (wrote_past == 0);
  DP_CHECK (starved == 1);
  DP_CHECK (after > 0);
  DP_CHECK (total_b > n_sym * 3 / 4);
  dp_async_dsss_receiver_destroy (a);
  dp_async_dsss_receiver_destroy (b);
  free (oa);
  free (ob);
  free (x);
  free (data);
  return fails;
}

/* `differential` does not change what steps() returns (#2461 #70). It is
 * handed to the internal MpskReceiver, whose differential demap acts in its
 * bits() -- a hard decision this receiver does not offer: steps() returns
 * the soft symbols, before any demap. Pinned so the header can say so and
 * a caller knows the flag is inert here (#2471): two cell receivers that
 * differ only in `differential` give bit-identical symbols. Sabotage:
 * negate the output when differential is set -> red. */
static int
_test_differential_does_not_change_the_symbols (void)
{
  const size_t    sf = 7, spc = 4;
  const double    fs          = 1.0e6 * (double)spc;
  const double    sym_rate    = 35714.29;
  const double    tsym        = fs / sym_rate;
  const size_t    te          = sf * spc;
  const size_t    n_sym       = 300;
  const size_t    pre_silence = te * 5 + 3;
  const double    cn0         = 60.0;
  float _Complex *x, *syms[2];
  size_t          n, n_syms[2];
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 11,
                   &x, &n, &data);
  for (int d = 0; d < 2; d++)
    {
      dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create_cell (
          CODE7, 7, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 4, 8, d, 0.0, 0.0,
          100, ASYNC_DSSS_RX_CELL_GAIN, ASYNC_DSSS_RX_CELL_PULLIN);
      DP_REQUIRE (rx != NULL);
      DP_REQUIRE (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
      n_syms[d] = _stream (rx, x + pre_silence, n - pre_silence, te, &syms[d]);
      DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
      dp_async_dsss_receiver_destroy (rx);
    }
  DP_CHECK (n_syms[0] > 100);
  DP_CHECK (n_syms[0] == n_syms[1]);
  DP_CHECK (memcmp (syms[0], syms[1], n_syms[0] * sizeof *syms[0]) == 0);
  free (syms[0]);
  free (syms[1]);
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

  dp_async_dsss_receiver_state_t *rx = _seeded_rx (cn0, confirm_s);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  const size_t    split = pre_silence + (size_t)(400.0 * tsym);
  float _Complex *syms;
  size_t          n_syms
      = _stream (rx, x + pre_silence, split - pre_silence, te, &syms);
  free (syms);
  DP_CHECK (n_syms > 20);
  DP_CHECK (dp_async_dsss_receiver_get_code_locked (rx) == 1);
  DP_CHECK (dp_async_dsss_receiver_get_locked (rx) == 1);

  /* Pin the code detector out of reach: it drops within its own down-count
   * and never comes back, while the symbol detector is untouched. */
  dp_async_dsss_receiver_configure_lock_raw (rx, 1e30, 1e30, 8, 0.1, 1, 1);

  size_t code_down_blocks = 0, sym_up_blocks = 0, blocks = 0;
  float _Complex tmp[1024];
  for (size_t pos = split; pos < n; pos += 1024)
    {
      size_t take = (pos + 1024 <= n) ? 1024 : n - pos;
      (void)dp_async_dsss_receiver_steps (rx, x + pos, take, tmp, 1024);
      blocks++;
      code_down_blocks += !dp_async_dsss_receiver_get_code_locked (rx);
      sym_up_blocks += dp_async_dsss_receiver_get_locked (rx);
      DP_CHECK (dp_async_dsss_receiver_get_lost (rx) == 0);
      if (dp_async_dsss_receiver_get_lost (rx))
        break;
    }
  /* The premise held: code lock was down for nearly all of it and symbol
   * lock up for all of it, over many confirm intervals. */
  DP_CHECK ((double)(n - split) / fs > 5.0 * confirm_s);
  DP_CHECK (code_down_blocks > blocks * 9 / 10);
  DP_CHECK (sym_up_blocks == blocks);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);

  free (x);
  free (data);
  dp_async_dsss_receiver_destroy (rx);
  return 0;
}

/* The blob is keyed by flavor: a cell receiver's state resumes
 * bit-for-bit into another cell receiver, and neither flavor accepts
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
  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      code, 1023, 5.0e6, 2700.0, 2, 2, 45.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5, 4,
      14.0, 64, 8, false, 100000, 2.5e9, 2.0, false);
  DP_REQUIRE (rx != NULL);
  DP_CHECK (rx->refine_min_blocks == ASYNC_DSSS_RX_REFINE_MIN_BLOCKS);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, 45.0) == DP_OK);
  printf ("  refine dwell at 45 dB-Hz, margin 14: %zu blocks (floor %zu)\n",
          rx->ca->dwell_target, rx->refine_min_blocks);
  DP_CHECK_MSG (rx->ca->dwell_target >= ASYNC_DSSS_RX_REFINE_MIN_BLOCKS,
                "the dwell is floored at refine_min_blocks");
  /* Without the floor the detection sizing alone: two blocks here. */
  dp_async_dsss_receiver_reset (rx);
  DP_CHECK (dp_async_dsss_receiver_set_refine_min_blocks (rx, 0) == DP_OK);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, 45.0) == DP_OK);
  DP_CHECK_MSG (rx->ca->dwell_target < ASYNC_DSSS_RX_REFINE_MIN_BLOCKS,
                "with the floor removed the detection sizing is shorter -- "
                "the floor was binding");
  /* A floor above the give-up cap is clamped to it. */
  dp_async_dsss_receiver_reset (rx);
  DP_CHECK (dp_async_dsss_receiver_set_refine_min_blocks (rx, 1000000)
            == DP_OK);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, 45.0) == DP_OK);
  DP_CHECK (rx->ca->dwell_target == rx->ca->max_n_blocks);
  dp_async_dsss_receiver_destroy (rx);
  return 0;
}

/* The blob's carry region is the live carry, then zeros (#2076's sibling).
 * A restore fills the whole buffer with real samples at car_carry_len 0,
 * the state a completed period leaves behind; the next get_state, into a
 * poisoned blob, must ship none of them. Writing the whole buffer again
 * turns this red. */
static int
_test_carry_tail_is_zeros (void)
{
  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5, 4,
      14.0, 64, 8, false, 100000, 0.0, 0.0, false);
  DP_CHECK (rx != NULL);
  if (!rx)
    return 1;
  const size_t   cb    = dp_async_dsss_receiver_state_bytes (rx);
  const size_t   carry = rx->tsamps * sizeof (float _Complex);
  unsigned char *blob  = malloc (cb);
  unsigned char *back  = malloc (cb);
  dp_async_dsss_receiver_get_state (rx, blob);
  DP_CHECK (rx->car_carry_len == 0);
  for (size_t i = 0; i < rx->tsamps; i++)
    {
      const float re_im[2] = { (float)(i + 1), 0.5f };
      memcpy (blob + cb - carry + i * sizeof re_im, re_im, sizeof re_im);
    }
  DP_CHECK (dp_async_dsss_receiver_set_state (rx, blob) == DP_OK);
  /* The restore really put real samples in the buffer: a set_state that
     read only car_carry_len samples would leave this test nothing to see. */
  size_t dirty = 0;
  for (size_t i = 0; i < carry; i++)
    dirty += ((const unsigned char *)rx->car_carry_buf)[i] != 0;
  DP_CHECK (dirty > 0);
  memset (back, 0xA5, cb);
  dp_async_dsss_receiver_get_state (rx, back);
  size_t nonzero = 0;
  for (size_t i = cb - carry; i < cb; i++)
    nonzero += back[i] != 0;
  DP_CHECK (nonzero == 0);

  /* A live carry is 0..tsamps-1 samples, so a blob claiming tsamps is
     refused (offsetof on the public extra, checked against the live value
     before it is patched). */
  const size_t at = sizeof (dp_state_hdr_t)
                    + offsetof (async_dsss_receiver_extra_t, car_carry_len);
  uint64_t     v;
  memcpy (&v, blob + at, sizeof v);
  DP_CHECK (v == 0);
  v = rx->tsamps - 1;
  memcpy (blob + at, &v, sizeof v);
  DP_CHECK (dp_async_dsss_receiver_set_state (rx, blob) == DP_OK);
  v = rx->tsamps;
  memcpy (blob + at, &v, sizeof v);
  DP_CHECK (dp_async_dsss_receiver_set_state (rx, blob) == DP_ERR_INVALID);
  free (blob);
  free (back);
  dp_async_dsss_receiver_destroy (rx);
  return 0;
}

static int
_test_seeded_blob_keyed_by_flavor (void)
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
  dp_async_dsss_receiver_state_t *ra = _seeded_rx (cn0, 2.0);
  dp_async_dsss_receiver_state_t *rb = _seeded_rx (cn0, 2.0);
  DP_CHECK (ra != NULL && rb != NULL);
  if (!ra || !rb)
    {
      free (x);
      free (data);
      return 1;
    }
  {
    size_t cb   = dp_async_dsss_receiver_state_bytes (ra);
    void  *blob = malloc (cb);
    dp_async_dsss_receiver_get_state (ra, blob);
    DP_CHECK (((async_dsss_receiver_extra_t *)((char *)blob
                                               + sizeof (dp_state_hdr_t)))
                  ->cell
              == 1);
    DP_CHECK (dp_async_dsss_receiver_set_state (rb, blob) == DP_OK);
    DP_CHECK (dp_async_dsss_receiver_get_idle (rb) == 1);
    free (blob);
  }

  /* Tracking: split the stream, resume the second receiver from the blob,
   * and require the two to emit identical symbols from there on. */
  DP_CHECK (dp_async_dsss_receiver_seed (ra, 0.0, 0.0, cn0) == DP_OK);
  /* 900 epochs before the split: the refine's dwell is floored at seven
     blocks (#1265), 3 ms at this rate, and the split must find the
     receiver tracking with symbols already out. */
  const size_t    split = pre_silence + te * 900;
  float _Complex *syms;
  size_t n_a = _stream (ra, x + pre_silence, split - pre_silence, te, &syms);
  free (syms);
  DP_CHECK (n_a > 20);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (ra) == 1);

  size_t cb   = dp_async_dsss_receiver_state_bytes (ra);
  void  *blob = malloc (cb);
  dp_async_dsss_receiver_get_state (ra, blob);
  DP_CHECK (dp_async_dsss_receiver_set_state (rb, blob) == DP_OK);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rb) == 1);
  DP_CHECK (dp_async_dsss_receiver_get_idle (rb) == 0);

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
  dp_async_dsss_receiver_state_t *rs = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 2.0, false);
  DP_CHECK (rs != NULL);
  if (rs)
    {
      DP_CHECK (dp_async_dsss_receiver_set_state (rs, blob) == DP_ERR_INVALID);
      DP_CHECK (dp_async_dsss_receiver_get_tracking (rs) == 0);
      size_t cbs   = dp_async_dsss_receiver_state_bytes (rs);
      void  *blobs = malloc (cbs);
      dp_async_dsss_receiver_get_state (rs, blobs);
      DP_CHECK (cbs != cb); /* the search engine is in one, not the other */
      DP_CHECK (dp_async_dsss_receiver_set_state (rb, blobs)
                == DP_ERR_INVALID);
      DP_CHECK (dp_async_dsss_receiver_get_tracking (rb) == 1); /* untouched */
      free (blobs);
      dp_async_dsss_receiver_destroy (rs);
    }
  ((char *)blob)[0] ^= (char)0xFF;
  DP_CHECK (dp_async_dsss_receiver_set_state (rb, blob) == DP_ERR_INVALID);
  free (blob);

  free (x);
  free (data);
  dp_async_dsss_receiver_destroy (ra);
  dp_async_dsss_receiver_destroy (rb);
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

  dp_async_dsss_receiver_state_t *rx = _seeded_rx (cn0, 0.02);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }

  /* Idle: nothing to report but the state and the clock. */
  async_dsss_receiver_status_t st = dp_async_dsss_receiver_status (rx);
  DP_CHECK (st.state == ASYNC_DSSS_RX_IDLE);
  DP_CHECK (st.doppler_hz == 0.0);
  DP_CHECK (st.code_locked == 0 && st.locked == 0);
  DP_CHECK (st.state_samples == 0 && st.both_down_samples == 0);
  float _Complex tmp[64];
  (void)dp_async_dsss_receiver_steps (rx, x, 37, tmp, 64);
  DP_CHECK (dp_async_dsss_receiver_status (rx).state_samples == 37);

  /* Refining: the seed IS the estimate, and the clock restarted. */
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 2.5, -1234.0, 51.0) == DP_OK);
  st = dp_async_dsss_receiver_status (rx);
  DP_CHECK (st.state == ASYNC_DSSS_RX_REFINING);
  DP_CHECK (fabs (st.doppler_hz + 1234.0) < 1e-6);
  DP_CHECK (st.cn0_dbhz_est == 51.0);
  DP_CHECK (st.state_samples == 0);
  dp_async_dsss_receiver_reset (rx);

  /* Tracking on the real capture: every field equals its getter, and the
   * live Doppler is the carrier loop's, in Hz at the front-end rate. */
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  float _Complex *syms;
  size_t n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  free (syms);
  DP_CHECK (n_syms > 20);
  st = dp_async_dsss_receiver_status (rx);
  DP_CHECK (st.state == ASYNC_DSSS_RX_TRACKING);
  DP_CHECK (st.chip_phase == dp_async_dsss_receiver_get_chip_phase (rx));
  DP_CHECK (st.code_rate == dp_async_dsss_receiver_get_code_rate (rx));
  DP_CHECK (st.cn0_dbhz_est == dp_async_dsss_receiver_get_cn0_dbhz_est (rx));
  DP_CHECK (st.code_locked == dp_async_dsss_receiver_get_code_locked (rx));
  DP_CHECK (st.locked == dp_async_dsss_receiver_get_locked (rx));
  DP_CHECK (st.code_locked == 1 && st.locked == 1);
  DP_CHECK (st.lock_metric == dp_async_dsss_receiver_get_lock_metric (rx));
  DP_CHECK (st.lock_threshold
            == dp_async_dsss_receiver_get_lock_threshold (rx));
  DP_CHECK (st.car_last_error
            == dp_async_dsss_receiver_get_car_last_error (rx));
  DP_CHECK (st.mpsk_last_error
            == dp_async_dsss_receiver_get_mpsk_last_error (rx));
  /* doppler#1261: the whole carrier estimate -- loop 1 plus what loop 2
     took up beyond it, the sum configure_chain_raw() re-seeds from. */
  DP_CHECK (fabs (st.doppler_hz
                  - (dp_costas_get_norm_freq (&rx->car) * fs
                     + dp_async_dsss_receiver_get_norm_freq (rx)
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
    dp_async_dsss_receiver_state_t *rd = _seeded_rx (cn0, 0.0);
    DP_CHECK (rd != NULL);
    if (rd)
      {
        DP_CHECK (dp_async_dsss_receiver_seed (rd, 0.0, 0.0, cn0) == DP_OK);
        float _Complex *sd;
        size_t n_sd = _stream (rd, x + pre_silence, n - pre_silence, te, &sd);
        free (sd);
        DP_CHECK (n_sd > 20);
        dp_async_dsss_receiver_configure_lock_raw (rd, 1e30, 1e30, 8, 0.1, 1,
                                                   1);
        n_sd = _stream (rd, x2, n2, 1024, &sd);
        free (sd);
        async_dsss_receiver_status_t sd_st
            = dp_async_dsss_receiver_status (rd);
        DP_CHECK (sd_st.state == ASYNC_DSSS_RX_TRACKING);
        DP_CHECK (sd_st.code_locked == 0 && sd_st.locked == 1);
        DP_CHECK (sd_st.code_locked
                  == dp_async_dsss_receiver_get_code_locked (rd));
        DP_CHECK (sd_st.locked == dp_async_dsss_receiver_get_locked (rd));
        dp_async_dsss_receiver_destroy (rd);
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
  st = dp_async_dsss_receiver_status (rx);
  DP_CHECK (st.state == ASYNC_DSSS_RX_LOST);
  DP_CHECK (st.both_down_samples > rx->lost_confirm_samples);
  DP_CHECK (st.both_down_samples == rx->both_down_samples);
  DP_CHECK (st.code_locked == 0 && st.locked == 0);
  /* Frozen: more input changes nothing the record reports. */
  (void)dp_async_dsss_receiver_steps (rx, off, 1024, tmp, 64);
  async_dsss_receiver_status_t st2 = dp_async_dsss_receiver_status (rx);
  DP_CHECK (st2.doppler_hz == st.doppler_hz);
  DP_CHECK (st2.both_down_samples == st.both_down_samples + 1024);
  DP_CHECK (st2.state_samples == st.state_samples + 1024);
  (void)doppler_before;

  free (off);
  free (x);
  free (data);
  dp_async_dsss_receiver_destroy (rx);
  return 0;
}

static int
_test_accessor_coverage (void)
{
  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5, 4,
      14.0, 64, 8, false, 100000, 0.0, 0.0, false);
  DP_CHECK (rx != NULL);
  if (!rx)
    return 1;

  /* Read-only accessors: each executes its one-line body on live state. */
  (void)dp_async_dsss_receiver_get_lock (rx);
  (void)dp_async_dsss_receiver_get_idle (rx);
  (void)dp_async_dsss_receiver_get_lost (rx);
  (void)dp_async_dsss_receiver_get_locked (rx);
  (void)dp_async_dsss_receiver_get_code_locked (rx);
  (void)dp_async_dsss_receiver_get_lock_metric (rx);
  DP_CHECK (dp_async_dsss_receiver_get_lock_threshold (rx) > 0.0);
  (void)dp_async_dsss_receiver_get_norm_freq (rx);
  (void)dp_async_dsss_receiver_get_nco_freq (rx);
  (void)dp_async_dsss_receiver_get_car_nco_freq (rx);
  (void)dp_async_dsss_receiver_get_car_last_error (rx);
  (void)dp_async_dsss_receiver_get_mpsk_last_error (rx);
  (void)dp_async_dsss_receiver_steps_max_out (rx); /* 0 until first stream */

  /* Raw sub-loop reconfiguration: valid grids/detectors, then the chain's
   * accept + both reject branches (segments < 1, and sps not a multiple of n).
   */
  DP_CHECK (dp_async_dsss_receiver_configure_search_raw (rx, 1, 1) == 0);
  DP_CHECK (dp_async_dsss_receiver_configure_search_raw (rx, 100000, 1) == -1);
  dp_async_dsss_receiver_configure_lock_raw (rx, 12.0, 6.0, 8, 0.1, 3, 3);
  DP_CHECK (dp_async_dsss_receiver_configure_chain_raw (rx, 4, 8, 4) == 0);
  DP_CHECK (dp_async_dsss_receiver_configure_chain_raw (rx, 0, 8, 4) == -1);
  DP_CHECK (dp_async_dsss_receiver_configure_chain_raw (rx, 4, 8, 3) == -1);
  /* The new grid meets what create() checked, before anything is rebuilt
     (doppler#2103). This code's symbol is 4 code periods, so 262145
     segments is 4 partials past the 2^20 the Dll's aid takes, where the
     rebuild aborted; and sps = 1 carries no m_out, where MpskReceiver's
     NULL aborted. Each refusal leaves the receiver on its grid. */
  DP_CHECK (dp_async_dsss_receiver_configure_chain_raw (rx, 262145, 8, 4)
            == -1);
  DP_CHECK (dp_async_dsss_receiver_configure_chain_raw (rx, 4, 1, 1) == -1);
  /* An n outside dp_ratesync_m_ok() that still divides sps: odd, under 2,
     over RATESYNC_MAX_M. Each passed the old n >= 1 && sps % n == 0 check
     and aborted in the rebuild's MpskReceiver create (doppler#2421). */
  DP_CHECK (dp_async_dsss_receiver_configure_chain_raw (rx, 4, 6, 3) == -1);
  DP_CHECK (dp_async_dsss_receiver_configure_chain_raw (rx, 4, 8, 1) == -1);
  DP_CHECK (dp_async_dsss_receiver_configure_chain_raw (rx, 4, 10, 10) == -1);
  DP_CHECK (dp_async_dsss_receiver_configure_chain_raw (rx, 4, 8, -2) == -1);
  DP_CHECK (rx->segments == 4 && rx->sps == 8 && rx->n == 4);

  dp_async_dsss_receiver_destroy (rx);
  return 0;
}

/* ------------------------------------------------------------------ *
 * The cell mode (docs/design/async-dsss-receiver-measurements.md       *
 * §12.22-12.24                                                         *
 * as a mode of this receiver, #1283): the receiver a searcher's cell     *
 * drives -- no refine, the carrier frozen, the Dll held from the first   *
 * sample and put back once an interval at a held phase corrected by a  *
 * gain times its interval-mean discriminator.                           *
 * ------------------------------------------------------------------ */
static dp_async_dsss_receiver_state_t *
_cell_rx (double cn0, double lost_confirm_s, double carrier_hz,
          size_t correct_periods, double gain, size_t pullin)
{
  return dp_async_dsss_receiver_create_cell (
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
  dp_async_dsss_receiver_state_t *rx = _cell_rx (cn0, 0.0, 0.0, 100, 0.125, 4);
  DP_CHECK (rx != NULL);
  if (!rx)
    return 1;
  DP_CHECK (dp_async_dsss_receiver_get_idle (rx) == 1);
  /* No search and no refine chain; the pull-in's estimator is its own. */
  DP_CHECK (rx->acq == NULL && rx->ca != NULL && rx->refine_dll == NULL);
  DP_CHECK (rx->cell == 1);
  DP_CHECK (dp_async_dsss_receiver_set_refine_min_blocks (rx, 3)
            == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_configure_search_raw (rx, 1, 1) == -1);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 1.0, 0.0, cn0) == DP_OK);
  DP_CHECK (dp_async_dsss_receiver_get_refining (rx) == 1); /* the pull-in */
  DP_CHECK (rx->held_phase == 1.0);
  DP_CHECK (rx->dll->coast == 1); /* held from the first sample */
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 2.0, 0.0, cn0) == DP_ERR_INVALID);
  /* Idle consumes and discards; refining decodes: fed a little noise,
     the pull-in receiver emits (garbage) symbols where idle emits none. */
  dp_async_dsss_receiver_reset (rx);
  DP_CHECK (dp_async_dsss_receiver_get_idle (rx) == 1);
  DP_CHECK (rx->held_phase == 0.0 && rx->intervals == 0);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 6.5, -100.0, cn0) == DP_OK);
  DP_CHECK (dp_async_dsss_receiver_get_doppler_hz (rx) == -100.0);
  dp_async_dsss_receiver_destroy (rx);
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
/* A cell seed past loop 1's pull-in -- four times its bound, where the
   loop alone is dead (costas_pullin.c): a searcher's data-block copy,
   section 12.14 -- is pulled in by the estimate on the live chain and
   tracks with both flags at the emitter's Doppler; the same stimulus
   seeded inside the bound tracks alike. Sabotage: mark the estimate
   folded at the seed (skip adr_cell_refine) -> the far seed holds code
   lock and never symbol lock. */
static int
_test_cell_seed_past_pullin_is_estimated (void)
{
  const size_t sf = 7, spc = 4;
  const double fs         = 1.0e6 * (double)spc;
  const double sym_rate   = 35714.29;
  const double tsym       = fs / sym_rate;
  const size_t te         = sf * spc;
  const double carrier_hz = 2.5e8, ppm = 20.0;
  const double doppler_hz = carrier_hz * ppm * 1e-6; /* 5 kHz */
  const double cn0        = 58.0;
  const size_t periods    = 100;
  const size_t interval   = periods * te;
  /* bn * chip_rate / (2 * code_len): 2857 Hz at this toy geometry. */
  const double bound = ASYNC_DSSS_RX_CARRIER_PULLIN_HZ (1.0e6, sf);
  DP_CHECK (fabs (bound - 0.04 * 1.0e6 / 14.0) < 1e-9);

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_dilated_capture (CODE7, sf, spc, fs, tsym, carrier_hz, ppm, 0.0, cn0,
                           6000, 0, 11, &x, &n, &data);
  dp_doppler_channel_state_t *ch
      = dp_doppler_channel_create (fs, carrier_hz, ppm, 0.0);
  const double delay = dp_doppler_channel_get_delay_samples (ch);
  dp_doppler_channel_destroy (ch);
  /* The offsets, in bounds: zero, and then past where the loops alone
     acquire -- measured by sabotage (the fold skipped): at this toy
     geometry the loops pull in 4 bounds by themselves, and fail at 16. */
  const double offs[3] = { 0.0, 4.0 * bound, 16.0 * bound };
  for (int k = 0; k < 3; k++)
    {
      dp_async_dsss_receiver_state_t *rx
          = _cell_rx (cn0, 0.0, carrier_hz, periods, 0.125, 4);
      DP_REQUIRE (rx != NULL);
      double seed = fmod (
          _dilated_truth (0.0, ppm, delay, spc, sf) + 7.0 * 4.0, (double)sf);
      DP_CHECK (
          dp_async_dsss_receiver_seed (rx, seed, doppler_hz + offs[k], cn0)
          == DP_OK);
      float _Complex *syms       = malloc (n * sizeof *syms);
      size_t          refined_at = 0;
      for (size_t pos = 0; pos + interval <= n; pos += interval)
        {
          (void)dp_async_dsss_receiver_steps (rx, x + pos, interval, syms, n);
          if (rx->cell_refined && !refined_at)
            refined_at = pos / interval + 1;
        }
      free (syms);
      async_dsss_receiver_status_t st = dp_async_dsss_receiver_status (rx);
      printf ("  cell: seed %+.0f Hz off (%.1f bounds): estimate folded by "
              "interval %zu; tracking %d code %d sym %d, Doppler %+.0f Hz "
              "off the truth\n",
              offs[k], offs[k] / bound, refined_at,
              st.state == ASYNC_DSSS_RX_TRACKING, st.code_locked, st.locked,
              st.doppler_hz - doppler_hz);
      DP_CHECK_MSG (refined_at >= 1, "the estimate is folded");
      DP_CHECK_MSG (st.state == ASYNC_DSSS_RX_TRACKING && st.code_locked
                        && st.locked,
                    k ? "a seed past the bound is pulled in and tracks "
                        "with both flags"
                      : "a seed inside the bound tracks with both flags");
      DP_CHECK_MSG (fabs (st.doppler_hz - doppler_hz) < 0.1 * bound,
                    "the Doppler reported is the emitter's");
      dp_async_dsss_receiver_destroy (rx);
    }
  free (x);
  free (data);
  return 0;
}

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
  dp_doppler_channel_state_t *ch
      = dp_doppler_channel_create (fs, carrier_hz, ppm, 0.0);
  const double delay = dp_doppler_channel_get_delay_samples (ch);
  dp_doppler_channel_destroy (ch);
  /* The truth is the channel's own mapping; the Dll's convention is the
     code phase at the next sample, so no constant is calibrated -- the
     coasting loop reads the mapping to a hundredth of a chip (the running
     searching flavor's loop, for the record, converges 0.24 chip off it here).
   */
  const double c = 0.0;

  const double gains[2] = { 0.125, 1.0 };
  double       sig[2]   = { 0.0, 0.0 };
  for (int g = 0; g < 2; g++)
    {
      dp_async_dsss_receiver_state_t *rx
          = _cell_rx (cn0, 0.0, carrier_hz, periods, gains[g], 4);
      DP_CHECK (rx != NULL);
      double seed = _dilated_truth (0.0, ppm, delay, spc, sf) + c + 0.4;
      seed        = fmod (seed + 7.0 * 4.0, (double)sf);
      DP_CHECK (dp_async_dsss_receiver_seed (rx, seed, doppler_hz + 5.0, cn0)
                == DP_OK);
      float _Complex *syms = malloc (n * sizeof *syms);
      size_t          ns = 0, k = 0, worst_i = 0;
      double          se = 0.0, se2 = 0.0, worst = 0.0;
      for (size_t pos = 0; pos + interval <= n; pos += interval)
        {
          ns += dp_async_dsss_receiver_steps (rx, x + pos, interval, syms + ns,
                                              n - ns);
          /* Settled: past the pull-in and a few intervals of the design
             gain; the phase the receiver holds against the truth here. */
          size_t i = pos / interval;
          if (i < 30)
            continue;
          double e
              = _wrap_chips (dp_async_dsss_receiver_get_chip_phase (rx)
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
              dp_async_dsss_receiver_get_tracking (rx),
              dp_async_dsss_receiver_get_code_locked (rx),
              dp_async_dsss_receiver_get_locked (rx), ns);
      DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
      DP_CHECK (dp_async_dsss_receiver_get_code_locked (rx) == 1);
      DP_CHECK (dp_async_dsss_receiver_get_locked (rx) == 1);
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
      DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0)
                == DP_ERR_INVALID);
      free (syms);
      dp_async_dsss_receiver_destroy (rx);
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
  dp_doppler_channel_state_t *ch
      = dp_doppler_channel_create (fs, carrier_hz, ppm, 0.0);
  const double delay = dp_doppler_channel_get_delay_samples (ch);
  dp_doppler_channel_destroy (ch);
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
  dp_async_dsss_receiver_state_t *rx
      = _cell_rx (cn0, lost_s, carrier_hz, periods, 1.0, 4);
  DP_CHECK (rx != NULL);
  double seed = fmod (_dilated_truth (0.0, ppm, delay, spc, sf) + c + 28.0,
                      (double)sf);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, seed, doppler_hz, cn0) == DP_OK);
  float _Complex *syms = malloc ((n + n_tail) * sizeof *syms);
  size_t          ns = 0, pos = 0;
  for (; pos + interval <= n; pos += interval)
    ns += dp_async_dsss_receiver_steps (rx, x + pos, interval, syms + ns,
                                        n + n_tail - ns);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (dp_async_dsss_receiver_get_code_locked (rx) == 1
            && dp_async_dsss_receiver_get_locked (rx) == 1);
  const double e_on = _wrap_chips (
      dp_async_dsss_receiver_get_chip_phase (rx)
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
      ns += dp_async_dsss_receiver_steps (rx, tail + tp, interval, syms + ns,
                                          n + n_tail - ns);
      if (lost_at < 0 && dp_async_dsss_receiver_get_lost (rx))
        {
          /* Lost: the receiver stops updating and its phase is where the
             emitter left it (section 10) -- read it at the interval the
             rule fired on, dead-reckoned to there. */
          lost_at = (int)(tp / interval);
          e_off   = _wrap_chips (
              dp_async_dsss_receiver_get_chip_phase (rx)
                  - _dilated_truth ((double)(pos + tp + interval), ppm, delay,
                                    spc, sf)
                  - c,
              sf);
        }
    }
  DP_CHECK (dp_async_dsss_receiver_get_lost (rx) == 1);
  DP_CHECK (lost_at > 0);
  printf ("  cell: switched off -- lost after %d intervals of %zu; held "
          "phase %+.3f chips from the hold point (%+.3f at switch-off), "
          "%llu intervals on\n",
          lost_at, tp / interval, e_off, e_on,
          (unsigned long long)intervals_on);
  DP_CHECK_MSG (fabs (e_off - e_on) < 0.03,
                "cell mode: switched off, the held phase dead-reckons and "
                "does not walk");
  dp_async_dsss_receiver_reset (rx);
  DP_CHECK (dp_async_dsss_receiver_get_idle (rx) == 1);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, seed, doppler_hz, cn0) == DP_OK);
  free (syms);
  free (tail);
  free (x);
  free (data);
  dp_async_dsss_receiver_destroy (rx);
  return 0;
}

/* The ramp: the searching flavor's carrier loop runs in the cell mode
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
  dp_async_dsss_receiver_state_t *rx
      = _cell_rx (cn0, 0.0, carrier_hz, periods, 0.125, 4);
  DP_CHECK (rx != NULL);
  /* The truth's phase at sample 0 is the channel's delay; no ramp yet. */
  dp_doppler_channel_state_t *ch
      = dp_doppler_channel_create (fs, carrier_hz, 0.0, ppm_s);
  const double delay = dp_doppler_channel_get_delay_samples (ch);
  dp_doppler_channel_destroy (ch);
  const double seed
      = fmod (_dilated_truth (0.0, 0.0, delay, spc, sf) + 28.0, (double)sf);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, seed, 0.0, cn0) == DP_OK);
  float _Complex *syms = malloc (n * sizeof *syms);
  size_t          ns = 0, unlocked_after = 0, pos = 0;
  for (; pos + interval <= n; pos += interval)
    {
      ns += dp_async_dsss_receiver_steps (rx, x + pos, interval, syms + ns,
                                          n - ns);
      if (pos > n / 4 && !dp_async_dsss_receiver_get_locked (rx))
        unlocked_after++;
    }
  const double                 t_end  = (double)pos / fs;
  const double                 f_true = carrier_hz * ppm_s * 1e-6 * t_end;
  async_dsss_receiver_status_t st     = dp_async_dsss_receiver_status (rx);
  printf ("  cell: ramp -- truth %.0f Hz at the end, status %.0f; intervals "
          "with the symbol flag down after the first quarter: %zu of %zu\n",
          f_true, st.doppler_hz, unlocked_after, pos / interval);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK_MSG (fabs (st.doppler_hz - f_true) < 60.0,
                "cell mode: the carrier loop follows the ramp");
  DP_CHECK_MSG (unlocked_after == 0,
                "cell mode: the symbol lock holds through the ramp");
  DP_CHECK (_best_ber (syms, ns, data, n_sym + 4) < 0.05);
  free (syms);
  free (x);
  free (data);
  dp_async_dsss_receiver_destroy (rx);
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
  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create_cell (
      code, sf, chip_rate, sym_rate, spc, 2, cn0, 1e-2, 0.9, 4, 8, 0, 0.0, 0.0,
      periods, ASYNC_DSSS_RX_CELL_GAIN, ASYNC_DSSS_RX_CELL_PULLIN);
  DP_CHECK (rx != NULL);
  /* Chip 0 on the first signal sample, the ramp from 0 Hz: the seed is the
     truth, as the cell-mode tests seed. */
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 0.0, cn0) == DP_OK);
  float _Complex *syms = malloc (n * sizeof *syms);
  size_t          ns = 0, down = 0, pos = pre, k = 0;
  for (; pos + interval <= n; pos += interval, k++)
    {
      ns += dp_async_dsss_receiver_steps (rx, x + pos, interval, syms + ns,
                                          n - ns);
      if (k >= 8 && !dp_async_dsss_receiver_get_locked (rx))
        down++;
    }
  const double                 t_end  = (double)(pos - pre) / fs;
  const double                 f_true = rate_hz_s * t_end;
  async_dsss_receiver_status_t st     = dp_async_dsss_receiver_status (rx);
  printf ("  cell: ramp at SPEC -- truth %.0f Hz at the end, status %.0f; "
          "intervals with the symbol flag down after the pull-in: %zu of "
          "%zu; %zu symbols\n",
          f_true, st.doppler_hz, down, k, ns);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
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
  dp_async_dsss_receiver_destroy (rx);
  return 0;
}

/* The blob: keyed on the mode (a searching-flavour blob is refused), no refine
 * children, the held phase and Doppler and the interval clocks in it -- a
 * receiver resumed mid-stream emits the same symbols and holds its
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
  dp_doppler_channel_state_t *ch
      = dp_doppler_channel_create (fs, carrier_hz, ppm, 0.0);
  const double delay = dp_doppler_channel_get_delay_samples (ch);
  dp_doppler_channel_destroy (ch);

  dp_async_dsss_receiver_state_t *ra
      = _cell_rx (cn0, 2.0, carrier_hz, periods, 0.125, 4);
  dp_async_dsss_receiver_state_t *rb
      = _cell_rx (cn0, 2.0, carrier_hz, periods, 0.125, 4);
  DP_CHECK (ra && rb);
  /* Idle: the mode in the blob; a searching receiver's blob is refused. */
  {
    size_t cb   = dp_async_dsss_receiver_state_bytes (ra);
    void  *blob = malloc (cb);
    dp_async_dsss_receiver_get_state (ra, blob);
    const async_dsss_receiver_extra_t *ex
        = (const async_dsss_receiver_extra_t *)((const char *)blob
                                                + sizeof (dp_state_hdr_t));
    DP_CHECK (ex->cell == 1);
    DP_CHECK (dp_async_dsss_receiver_set_state (rb, blob) == DP_OK);
    free (blob);
    dp_async_dsss_receiver_state_t *rh = dp_async_dsss_receiver_create (
        CODE7, 7, 1.0e6, 35714.29, 4, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
        4, 14.0, 64, 8, false, 100000, carrier_hz, 2.0, false);
    size_t ch_b = dp_async_dsss_receiver_state_bytes (rh);
    void  *hb   = malloc (ch_b);
    dp_async_dsss_receiver_get_state (rh, hb);
    DP_CHECK (dp_async_dsss_receiver_set_state (rb, hb) == DP_ERR_INVALID);
    free (hb);
    dp_async_dsss_receiver_destroy (rh);
  }
  const double seed
      = fmod (_dilated_truth (0.0, ppm, delay, spc, sf) + 28.0, (double)sf);
  DP_CHECK (dp_async_dsss_receiver_seed (ra, seed, doppler_hz, cn0) == DP_OK);
  float _Complex *sa = malloc (n * sizeof *sa), *sb = malloc (n * sizeof *sb);
  size_t          na = 0, nb = 0, pos = 0;
  /* Forty intervals in, tracking, mid-interval: the split. */
  const size_t split = 40 * interval + interval / 3;
  for (; pos + interval <= split; pos += interval)
    na += dp_async_dsss_receiver_steps (ra, x + pos, interval, sa + na,
                                        n - na);
  na += dp_async_dsss_receiver_steps (ra, x + pos, split - pos, sa + na,
                                      n - na);
  pos = split;
  DP_CHECK (dp_async_dsss_receiver_get_tracking (ra) == 1);
  DP_STATE_ROUNDTRIP_TEST (dp_async_dsss_receiver, ra, rb);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rb) == 1);
  DP_CHECK (rb->held_phase == ra->held_phase
            && rb->period_count == ra->period_count
            && rb->intervals == ra->intervals);
  size_t na0 = na;
  nb         = 0;
  for (; pos + interval <= n; pos += interval)
    {
      na += dp_async_dsss_receiver_steps (ra, x + pos, interval, sa + na,
                                          n - na);
      nb += dp_async_dsss_receiver_steps (rb, x + pos, interval, sb + nb,
                                          n - nb);
    }
  DP_CHECK (na - na0 == nb && nb > 100);
  DP_CHECK (memcmp (sa + na0, sb, nb * sizeof *sb) == 0);
  async_dsss_receiver_status_t sta = dp_async_dsss_receiver_status (ra),
                               stb = dp_async_dsss_receiver_status (rb);
  DP_CHECK (sta.chip_phase == stb.chip_phase
            && sta.doppler_hz == stb.doppler_hz
            && sta.code_locked == stb.code_locked && sta.locked == stb.locked);
  free (sa);
  free (sb);
  free (x);
  free (data);
  dp_async_dsss_receiver_destroy (ra);
  dp_async_dsss_receiver_destroy (rb);
  return 0;
}

/* The hand-off resumes where the HIT's frame ended (#2042). At a design
 * C/N0 of 58 dB-Hz the auto-sizer picks several non-coherent looks, so the
 * frames after a hit decide nothing; before #2042's round 1 the embedded
 * acquisition kept framing them, and the receiver took its tail from the
 * engine's live counter, so a call that carried input past the hit began
 * tracking whole frames late and lost those symbols without a word. One
 * call over the whole capture must therefore equal the same capture in
 * one-epoch chunks -- where no call carries a frame past the hit -- bit
 * for bit. */
static int
_test_handoff_resumes_at_the_hit (void)
{
  const size_t sf = 7, spc = 4, te = sf * spc, n_sym = 400;
  const double fs = 1.0e6 * (double)spc, tsym = fs / 35714.29;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, 90.0, n_sym, te * 5 + 3, 7,
                   &x, &n, &data);

  dp_async_dsss_receiver_state_t *one = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, 35714.29, spc, 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false);
  dp_async_dsss_receiver_state_t *chunked = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, 35714.29, spc, 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false);
  DP_CHECK (one != NULL && chunked != NULL);
  if (!one || !chunked)
    {
      dp_async_dsss_receiver_destroy (one);
      dp_async_dsss_receiver_destroy (chunked);
      free (x);
      free (data);
      return 1;
    }
  /* The premise: frames after a hit that decide nothing. */
  DP_CHECK (one->acq->n_noncoh > 1);

  float _Complex *a, *b;
  size_t          n_a = _stream (one, x, n, n, &a);
  size_t          n_b = _stream (chunked, x, n, te, &b);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (one) == 1);
  DP_CHECK (n_a > 20);
  DP_CHECK (n_a == n_b && memcmp (a, b, n_a * sizeof *a) == 0);

  free (a);
  free (b);
  dp_async_dsss_receiver_destroy (one);
  dp_async_dsss_receiver_destroy (chunked);
  free (x);
  free (data);
  return 0;
}

/* The hand-off's two counts must be one timebase (#2042's final round).
 * The tail is where the hit's frame ended in the ENGINE's position; a count
 * of what this receiver fed was a second timebase that configure_search_raw
 * (an engine reset) and set_state (an engine restore) left behind, and the
 * tail then started late or read outside x. Cut in the leading silence --
 * two frames and a carry, no hit yet -- then (a) reconfigure and (b) carry
 * the state into a fresh receiver: each must equal the run it stands for,
 * bit for bit. */
static int
_test_handoff_survives_reconfigure_and_restore (void)
{
  const size_t    sf = 7, spc = 4, te = sf * spc, n_sym = 400;
  const double    fs = 1.0e6 * (double)spc, tsym = fs / 35714.29;
  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, 90.0, n_sym, te * 5 + 3, 7,
                   &x, &n, &data);
  const size_t cut = 2 * te + 5; /* inside the silence: frames and a carry */

  dp_async_dsss_receiver_state_t *a = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, 35714.29, spc, 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false);
  dp_async_dsss_receiver_state_t *ref = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, 35714.29, spc, 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false);
  dp_async_dsss_receiver_state_t *b = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, 35714.29, spc, 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false);
  dp_async_dsss_receiver_state_t *whole = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, 35714.29, spc, 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false);
  DP_CHECK (a && ref && b && whole);
  if (!a || !ref || !b || !whole)
    {
      dp_async_dsss_receiver_destroy (a);
      dp_async_dsss_receiver_destroy (ref);
      dp_async_dsss_receiver_destroy (b);
      dp_async_dsss_receiver_destroy (whole);
      free (x);
      free (data);
      return 1;
    }
  float _Complex *o1 = malloc (n * sizeof *o1), *o2 = malloc (n * sizeof *o2);
  DP_CHECK (o1 && o2);

  /* (a) Reconfigured mid-search: the engine restarts at 0; a fresh
     receiver configured the same way and fed the same rest is the truth. */
  DP_CHECK (dp_async_dsss_receiver_steps (a, x, cut, o1, n) == 0);
  DP_CHECK (dp_async_dsss_receiver_configure_search_raw (a, 1, 4) == 0);
  DP_CHECK (dp_async_dsss_receiver_configure_search_raw (ref, 1, 4) == 0);
  size_t na = dp_async_dsss_receiver_steps (a, x + cut, n - cut, o1, n);
  size_t nr = dp_async_dsss_receiver_steps (ref, x + cut, n - cut, o2, n);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (a) == 1);
  DP_CHECK (na > 20 && na == nr && memcmp (o1, o2, na * sizeof *o1) == 0);

  /* (b) Restored mid-search into a fresh receiver: the uninterrupted run
     over the whole capture is the truth. */
  dp_async_dsss_receiver_state_t *src = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, 35714.29, spc, 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false);
  DP_CHECK (src != NULL);
  if (src)
    {
      DP_CHECK (dp_async_dsss_receiver_steps (src, x, cut, o1, n) == 0);
      void *blob = malloc (dp_async_dsss_receiver_state_bytes (src));
      DP_CHECK (blob != NULL);
      if (blob)
        {
          dp_async_dsss_receiver_get_state (src, blob);
          DP_CHECK (dp_async_dsss_receiver_set_state (b, blob) == DP_OK);
          size_t nb
              = dp_async_dsss_receiver_steps (b, x + cut, n - cut, o1, n);
          size_t nw = dp_async_dsss_receiver_steps (whole, x, n, o2, n);
          DP_CHECK (dp_async_dsss_receiver_get_tracking (b) == 1);
          DP_CHECK (nb > 20 && nb == nw
                    && memcmp (o1, o2, nb * sizeof *o1) == 0);
          free (blob);
        }
      dp_async_dsss_receiver_destroy (src);
    }

  free (o1);
  free (o2);
  dp_async_dsss_receiver_destroy (a);
  dp_async_dsss_receiver_destroy (ref);
  dp_async_dsss_receiver_destroy (b);
  dp_async_dsss_receiver_destroy (whole);
  free (x);
  free (data);
  return 0;
}

/* A forged seed in a live blob is refused, and the target is left alone
 * (doppler#2103). set_state() restored seed_chip_phase and seed_doppler_hz_est
 * unchecked: a NaN phase went into the refine chain's Dll create, which
 * aborts, and a Doppler of 1e308 sent the hand-off's phase to NaN. The
 * forged value is located by its bytes: a seed writes its phase and Doppler
 * as doubles, so a distinctive value's first occurrence in the blob is the
 * extra record's field, which precedes the children. Asserted: each forged
 * blob refused with the target's state byte-identical. Returns the number of
 * failed checks. */
static int
_test_forged_seed_is_refused (void)
{
  uint8_t  code[1023];
  uint32_t cst = 13;
  for (size_t i = 0; i < 1023; i++)
    code[i] = (uint8_t)(dp_bit (&cst) > 0 ? 0u : 1u);
  int fails = 0;

  struct
  {
    double      seed_phase, seed_doppler, forge_at_phase, forge_at_doppler;
    const char *what;
  } forge[] = {
    { 512.25, 1000.5, NAN, 1000.5, "a NaN seed phase" },
    { 512.25, 1000.5, 512.25, 1.0e308, "a seed Doppler of 1e308" },
  };
  for (size_t k = 0; k < sizeof forge / sizeof *forge; k++)
    {
      dp_async_dsss_receiver_state_t *src = dp_async_dsss_receiver_create (
          code, 1023, 5.0e6, 2700.0, 2, 2, 45.0, 1e-3, 0.9, 100.0, 4, 8, 0,
          0.5, 4, 14.0, 64, 8, false, 100000, 2.5e9, 2.0, false);
      dp_async_dsss_receiver_state_t *dst = dp_async_dsss_receiver_create (
          code, 1023, 5.0e6, 2700.0, 2, 2, 45.0, 1e-3, 0.9, 100.0, 4, 8, 0,
          0.5, 4, 14.0, 64, 8, false, 100000, 2.5e9, 2.0, false);
      DP_REQUIRE (src != NULL && dst != NULL);
      DP_REQUIRE (dp_async_dsss_receiver_seed (src, forge[k].seed_phase,
                                               forge[k].seed_doppler, 45.0)
                  == DP_OK);
      const size_t   cb   = dp_async_dsss_receiver_state_bytes (src);
      unsigned char *blob = malloc (cb);
      dp_async_dsss_receiver_get_state (src, blob);

      /* The seed field, by offset: the extra record follows the blob's header
         (async_dsss_receiver_extra_t, in the public header), and the seed's
         two doubles are named fields of it. */
      const size_t at
          = sizeof (dp_state_hdr_t)
            + (k == 0 ? offsetof (async_dsss_receiver_extra_t, seed_chip_phase)
                      : offsetof (async_dsss_receiver_extra_t,
                                  seed_doppler_hz_est));
      DP_CHECK (at + sizeof (double) <= cb);
      if (at + sizeof (double) <= cb)
        {
          const double forged
              = k == 0 ? forge[k].forge_at_phase : forge[k].forge_at_doppler;
          memcpy (blob + at, &forged, sizeof forged);
          const size_t   cbd    = dp_async_dsss_receiver_state_bytes (dst);
          unsigned char *before = malloc (cbd), *after = malloc (cbd);
          dp_async_dsss_receiver_get_state (dst, before);
          if (dp_async_dsss_receiver_set_state (dst, blob) != DP_ERR_INVALID)
            {
              DP_TEST_ERR ("  forged blob accepted: %s\n", forge[k].what);
              fails++;
            }
          dp_async_dsss_receiver_get_state (dst, after);
          if (memcmp (after, before, cbd) != 0)
            {
              DP_TEST_ERR ("  refused blob changed target: %s\n",
                           forge[k].what);
              fails++;
            }
          free (before);
          free (after);
        }
      free (blob);
      dp_async_dsss_receiver_destroy (src);
      dp_async_dsss_receiver_destroy (dst);
    }
  return fails;
}

/* The hand-over holds the refined Doppler to the seed domain (#2461 #122).
 * The estimator is forced ready by the blob, with a +-5 kHz residual, so the
 * next steps() is the hand-over. Every seed is in range at fs = 10 MHz; the
 * refined Doppler (seed plus residual) is not always:
 *   4.990 MHz + 5 kHz =  4.995 MHz  inside: tracks at exactly that;
 *  -4.990 MHz - 5 kHz = -4.995 MHz  inside, the negative side;
 *   4.995 MHz + 5 kHz =  5.000 MHz  AT fs/2: refused ("at or past");
 *  -4.998 MHz - 5 kHz = -5.003 MHz  past -fs/2: refused.
 * A refused hand-over is refused like a hit: not tracking, searching again,
 * one refused hit counted -- and the Doppler it refused is not left
 * readable: both estimates read 0, as after reset(). Sabotages: drop the
 * hand-over's domain test -> the refused rows track, red; keep the refused
 * seed's Doppler -> get_doppler_hz() reads it, red. Returns the number of
 * failed checks. */
static int
_test_handover_holds_the_refined_doppler_to_the_domain (void)
{
  uint8_t  code[1023];
  uint32_t cst = 13;
  for (size_t i = 0; i < 1023; i++)
    code[i] = (uint8_t)(dp_bit (&cst) > 0 ? 0u : 1u);
  const double seeds[4]    = { 4.990e6, -4.990e6, 4.995e6, -4.998e6 };
  const double residual[4] = { 5.0e3, -5.0e3, 5.0e3, -5.0e3 };
  const int    inside[4]   = { 1, 1, 0, 0 };
  int          fails       = 0;
  for (int k = 0; k < 4; k++)
    {
      const double                    residual_hz = residual[k];
      dp_async_dsss_receiver_state_t *src = dp_async_dsss_receiver_create (
          code, 1023, 5.0e6, 2700.0, 2, 2, 45.0, 1e-3, 0.9, 100.0, 4, 8, 0,
          0.5, 4, 14.0, 64, 8, false, 100000, 2.5e9, 2.0, false);
      dp_async_dsss_receiver_state_t *dst = dp_async_dsss_receiver_create (
          code, 1023, 5.0e6, 2700.0, 2, 2, 45.0, 1e-3, 0.9, 100.0, 4, 8, 0,
          0.5, 4, 14.0, 64, 8, false, 100000, 2.5e9, 2.0, false);
      DP_REQUIRE (src != NULL && dst != NULL);
      DP_REQUIRE (dp_async_dsss_receiver_seed (src, 0.0, seeds[k], 45.0)
                  == DP_OK);
      const size_t   cb   = dp_async_dsss_receiver_state_bytes (src);
      unsigned char *blob = malloc (cb);
      dp_async_dsss_receiver_get_state (src, blob);
      uint32_t magic = CARRIER_ACQ_STATE_MAGIC;
      size_t   at    = cb;
      for (size_t i = 0; i + sizeof magic <= cb && at == cb; i++)
        if (memcmp (blob + i, &magic, sizeof magic) == 0)
          at = i;
      DP_CHECK (at < cb);
      if (at < cb)
        {
          const size_t rec = at + sizeof (dp_state_hdr_t);
          blob[rec + 32]   = 1; /* ready: the carrier_acq extra's ready byte */
          memcpy (blob + rec + 40, &residual_hz, sizeof residual_hz);
          DP_CHECK (dp_async_dsss_receiver_set_state (dst, blob) == DP_OK);
          const uint64_t refused0 = dst->refused_hits;
          float _Complex x[1024]  = { 0 }, out[1024];
          (void)dp_async_dsss_receiver_steps (dst, x, 1024, out, 1024);
          if (inside[k])
            {
              /* Inside: tracks at seed plus residual, exactly. */
              DP_CHECK (dp_async_dsss_receiver_get_tracking (dst) == 1);
              if (dp_async_dsss_receiver_get_doppler_hz (dst)
                  != seeds[k] + residual_hz)
                {
                  DP_TEST_ERR ("  hand-off Doppler %g, not seed + residual "
                               "%g\n",
                               dp_async_dsss_receiver_get_doppler_hz (dst),
                               seeds[k] + residual_hz);
                  fails++;
                }
              DP_CHECK (dst->refused_hits == refused0);
            }
          else
            {
              /* Past fs/2: refused, searching again, counted. */
              DP_CHECK (dp_async_dsss_receiver_get_tracking (dst) == 0);
              DP_CHECK (dp_async_dsss_receiver_status (dst).state
                        == ASYNC_DSSS_RX_SEARCHING);
              DP_CHECK (dst->refused_hits == refused0 + 1);
              DP_CHECK (dp_async_dsss_receiver_get_doppler_hz (dst) == 0.0);
              DP_CHECK (dst->seed_doppler_hz_est == 0.0);
            }
        }
      free (blob);
      dp_async_dsss_receiver_destroy (src);
      dp_async_dsss_receiver_destroy (dst);
    }
  return fails;
}

/* The seed domain, pinned at its edges (doppler#2103): the predicate that
 * seed(), set_state(), the searching hit path and the pool all use. A phase
 * must be finite and inside [0, code_len); a Doppler strictly inside
 * (-fs/2, fs/2), fs = chip_rate * spc. Here fs = 10 MHz (5 Mchip/s at 2
 * samples a chip), so fs/2 = 5 MHz: that is refused, the next float below it
 * accepted, and NaN or infinity refused on either axis.
 * Returns the number of failed checks. */
static int
_test_seed_domain (void)
{
  uint8_t  code[1023];
  uint32_t cst = 13;
  for (size_t i = 0; i < 1023; i++)
    code[i] = (uint8_t)(dp_bit (&cst) > 0 ? 0u : 1u);
  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      code, 1023, 5.0e6, 2700.0, 2, 2, 45.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5, 4,
      14.0, 64, 8, false, 100000, 2.5e9, 2.0, false);
  DP_REQUIRE (rx != NULL);
  int          fails   = 0;
  const double fs_half = 0.5 * 5.0e6 * 2.0; /* chip_rate * spc / 2 */
  struct
  {
    double      phase, doppler;
    int         ok;
    const char *what;
  } cases[] = {
    { 0.0, 0.0, 1, "the origin" },
    { 1022.99, 0.0, 1, "the last chip" },
    { 1023.0, 0.0, 0, "a phase at code_len" },
    { -0.5, 0.0, 0, "a negative phase" },
    { NAN, 0.0, 0, "a NaN phase" },
    { 0.0, fs_half, 0, "a Doppler at fs/2" },
    { 0.0, nextafter (fs_half, 0.0), 1, "a Doppler just under fs/2" },
    { 0.0, -fs_half, 0, "a Doppler at -fs/2" },
    { 0.0, NAN, 0, "a NaN Doppler" },
    { 0.0, INFINITY, 0, "an infinite Doppler" },
  };
  for (size_t k = 0; k < sizeof cases / sizeof *cases; k++)
    {
      const int got = dp_async_dsss_receiver_seed_ok (rx, cases[k].phase,
                                                      cases[k].doppler);
      if ((got != 0) != (cases[k].ok != 0))
        {
          DP_TEST_ERR ("  seed domain: %s gave %d\n", cases[k].what, got);
          fails++;
        }
    }
  dp_async_dsss_receiver_destroy (rx);
  return fails;
}

/* A forged carrier residual in an async blob is refused (doppler#2103). The
 * carrier estimator's record sits inside the async blob under its own magic,
 * and its residual is at a fixed offset in its extra record: four uint64
 * fields, the ready byte and seven padding bytes, so byte 40 (the layout in
 * carrier_acq_core.c's carrier_acq_extra_t). A residual of NaN, or one past
 * half the sample rate, was restored unchecked and reached the hand-off's Dll
 * build under dp_xnn. Asserted: each forged blob is refused. Returns the
 * number of failed checks. */
static int
_test_forged_residual_refused (void)
{
  uint8_t  code[1023];
  uint32_t cst = 13;
  for (size_t i = 0; i < 1023; i++)
    code[i] = (uint8_t)(dp_bit (&cst) > 0 ? 0u : 1u);
  dp_async_dsss_receiver_state_t *src = dp_async_dsss_receiver_create (
      code, 1023, 5.0e6, 2700.0, 2, 2, 45.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5, 4,
      14.0, 64, 8, false, 100000, 2.5e9, 2.0, false);
  DP_REQUIRE (src != NULL);
  const size_t   cb   = dp_async_dsss_receiver_state_bytes (src);
  unsigned char *blob = malloc (cb);
  dp_async_dsss_receiver_get_state (src, blob);

  /* Find the carrier estimator's record by its magic. */
  uint32_t magic = CARRIER_ACQ_STATE_MAGIC;
  size_t   at    = cb;
  for (size_t i = 0; i + sizeof magic <= cb && at == cb; i++)
    if (memcmp (blob + i, &magic, sizeof magic) == 0)
      at = i;
  int fails = 0;
  DP_CHECK (at < cb);
  const size_t residual = at + sizeof (dp_state_hdr_t) + 40;
  const double nan = NAN, huge = 1.0e298;
  const double forged[] = { nan, huge };
  for (size_t k = 0; at < cb && k < 2; k++)
    {
      dp_async_dsss_receiver_state_t *dst = dp_async_dsss_receiver_create (
          code, 1023, 5.0e6, 2700.0, 2, 2, 45.0, 1e-3, 0.9, 100.0, 4, 8, 0,
          0.5, 4, 14.0, 64, 8, false, 100000, 2.5e9, 2.0, false);
      DP_REQUIRE (dst != NULL);
      unsigned char *bad = malloc (cb);
      memcpy (bad, blob, cb);
      memcpy (bad + residual, &forged[k], sizeof forged[k]);
      if (dp_async_dsss_receiver_set_state (dst, bad) != DP_ERR_INVALID)
        {
          DP_TEST_ERR ("  forged residual accepted: %s\n",
                       k == 0 ? "a NaN residual" : "a residual past fs/2");
          fails++;
        }
      free (bad);
      dp_async_dsss_receiver_destroy (dst);
    }
  free (blob);
  dp_async_dsss_receiver_destroy (src);
  return fails;
}

/* A cell receiver has no acquisition engine, so a blob that puts it in
 * SEARCHING is refused (doppler#2103). The state byte is the first byte of
 * the extra record (async_dsss_receiver_extra_t): forging it to SEARCHING
 * gave a cell receiver a steps() that dereferenced a NULL acq. Asserted: the
 * forged blob is refused, and the honest one still restores. Returns the
 * number of failed checks. */
static int
_test_forged_cell_searching_refused (void)
{
  dp_async_dsss_receiver_state_t *src = dp_async_dsss_receiver_create_cell (
      CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 4, 8, 0, 2.5e9, 1.0, 100,
      0.125, 4);
  dp_async_dsss_receiver_state_t *dst = dp_async_dsss_receiver_create_cell (
      CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 4, 8, 0, 2.5e9, 1.0, 100,
      0.125, 4);
  DP_REQUIRE (src != NULL && dst != NULL);
  const size_t   cb   = dp_async_dsss_receiver_state_bytes (src);
  unsigned char *blob = malloc (cb);
  dp_async_dsss_receiver_get_state (src, blob);
  int fails = 0;
  if (dp_async_dsss_receiver_set_state (dst, blob) != DP_OK)
    {
      DP_TEST_ERR ("  honest cell blob refused\n");
      fails++;
    }
  blob[sizeof (dp_state_hdr_t)] = (unsigned char)ASYNC_DSSS_RX_SEARCHING;
  if (dp_async_dsss_receiver_set_state (dst, blob) != DP_ERR_INVALID)
    {
      DP_TEST_ERR ("  forged cell SEARCHING blob accepted\n");
      fails++;
    }
  free (blob);
  dp_async_dsss_receiver_destroy (src);
  dp_async_dsss_receiver_destroy (dst);
  return fails;
}

/* refused_hits is part of the blob, so a resumed receiver's status reports the
 * count it had: set 7, round-trip, and the restored status reads 7
 * (doppler#2103). Returns the number of failed checks. */
static int
_test_refused_hits_round_trips (void)
{
  uint8_t  code[1023];
  uint32_t cst = 13;
  for (size_t i = 0; i < 1023; i++)
    code[i] = (uint8_t)(dp_bit (&cst) > 0 ? 0u : 1u);
  dp_async_dsss_receiver_state_t *src = dp_async_dsss_receiver_create (
      code, 1023, 5.0e6, 2700.0, 2, 2, 45.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5, 4,
      14.0, 64, 8, false, 100000, 2.5e9, 2.0, false);
  dp_async_dsss_receiver_state_t *dst = dp_async_dsss_receiver_create (
      code, 1023, 5.0e6, 2700.0, 2, 2, 45.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5, 4,
      14.0, 64, 8, false, 100000, 2.5e9, 2.0, false);
  DP_REQUIRE (src != NULL && dst != NULL);
  src->refused_hits   = 7;
  const size_t   cb   = dp_async_dsss_receiver_state_bytes (src);
  unsigned char *blob = malloc (cb);
  dp_async_dsss_receiver_get_state (src, blob);
  int fails = 0;
  if (dp_async_dsss_receiver_set_state (dst, blob) != DP_OK)
    {
      DP_TEST_ERR ("  honest blob refused\n");
      fails++;
    }
  if (dp_async_dsss_receiver_status (dst).refused_hits != 7)
    {
      DP_TEST_ERR ("  restored refused_hits %llu, not 7\n",
                   (unsigned long long)dp_async_dsss_receiver_status (dst)
                       .refused_hits);
      fails++;
    }
  free (blob);
  dp_async_dsss_receiver_destroy (src);
  dp_async_dsss_receiver_destroy (dst);
  return fails;
}

/* Drain every record a call wrote and pick out the receiver's lifecycle
 * trio: how many records each probe wrote, and the last value of each. */
typedef struct
{
  size_t n_state, n_handover, n_refused;
  float  state, handover, refused;
} _tlm_trio_t;

static _tlm_trio_t
_tlm_read_trio (dp_tlm_t *t, int id_state, int id_handover, int id_refused)
{
  _tlm_trio_t  r = { 0, 0, 0, NAN, NAN, NAN };
  dp_tlm_rec_t recs[1024];
  size_t       m;
  while ((m = dp_tlm_read (t, 1024, recs, 1024)) > 0)
    for (size_t j = 0; j < m; j++)
      {
        if (recs[j].probe == id_state)
          r.n_state++, r.state = recs[j].value;
        else if (recs[j].probe == id_handover)
          r.n_handover++, r.handover = recs[j].value;
        else if (recs[j].probe == id_refused)
          r.n_refused++, r.refused = recs[j].value;
      }
  return r;
}

/* The receiver's own lifecycle probes (set_telemetry(): ".state",
 * ".handover_hz", ".refused") through a whole life: searching, a hit, the
 * refine, the hand-over, tracking, and lost after the emitter leaves. Each
 * call that carries samples writes one record per probe, after the call,
 * so each record reads what the accessors read once the call returns, and
 * the states arrive in the life's order. The emitter sits 20 kHz off: the
 * coarse seed is 0 Hz and the refine hands over within one of its own
 * zero-padded FFT bins of the emitter. Sabotage: emit before
 * the call -> red (the hand-over call reads refining); emit the seed's
 * Doppler -> red at the hand-over; emit from inside the recursion -> red
 * (a hit's call writes twice). */
static int
_test_tlm_lifecycle_probes (void)
{
  const size_t sf = 7, spc = 4;
  const double fs          = 1.0e6 * (double)spc;
  const double sym_rate    = 35714.29;
  const double tsym        = fs / sym_rate;
  const size_t te          = sf * spc;
  const size_t n_sym       = 400;
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 70.0;
  const double doppler_hz  = 20.0e3;
  const double confirm_s   = 0.02;
  const size_t tail_n      = (size_t)(0.25 * fs);
  const size_t chunk       = 1024;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, doppler_hz, cn0, n_sym,
                   pre_silence, 7, &x, &n, &data);
  float _Complex                 *off = _noise_tail (tail_n, fs, cn0, 99);
  dp_async_dsss_receiver_state_t *rx
      = _searching_rx (cn0, sym_rate, confirm_s, false);
  dp_tlm_t *t = dp_tlm_create (1 << 16);
  DP_REQUIRE (rx != NULL && t != NULL);
  DP_REQUIRE (dp_async_dsss_receiver_set_telemetry (rx, t, "rx", 1) == DP_OK);
  const int id_state    = dp_tlm_probe_id (t, "rx.state");
  const int id_handover = dp_tlm_probe_id (t, "rx.handover_hz");
  const int id_refused  = dp_tlm_probe_id (t, "rx.refused");
  DP_REQUIRE (id_state >= 0 && id_handover >= 0 && id_refused >= 0);

  float _Complex *out = malloc (chunk * 8 * sizeof *out);
  DP_REQUIRE (out != NULL);
  /* An empty call changed nothing, so it reports nothing. */
  (void)dp_async_dsss_receiver_steps (rx, x, 0, out, chunk * 8);
  DP_CHECK (_tlm_emitted (t, "rx.state") == 0);

  size_t calls = 0, bad = 0;
  int    seen[5] = { 0 }, order[5], n_order = 0;
  double seed_hz = NAN, handed_hz = NAN;
  for (int part = 0; part < 2; part++)
    {
      const float _Complex *src  = part ? off : x;
      const size_t          len  = part ? tail_n : n;
      const size_t          step = part ? chunk : te;
      for (size_t pos = 0; pos < len; pos += step)
        {
          size_t k = len - pos < step ? len - pos : step;
          (void)dp_async_dsss_receiver_steps (rx, src + pos, k, out,
                                              chunk * 8);
          calls++;
          _tlm_trio_t r
              = _tlm_read_trio (t, id_state, id_handover, id_refused);
          const int    st = dp_async_dsss_receiver_status (rx).state;
          const double ho = dp_async_dsss_receiver_get_doppler_hz (rx);
          const float  rf = (float)rx->refused_hits;
          if (r.n_state != 1 || r.n_handover != 1 || r.n_refused != 1
              || r.state != (float)st || r.handover != (float)ho
              || r.refused != rf)
            {
              if (bad++ < 3)
                DP_TEST_ERR ("  call %zu: records %zu/%zu/%zu, state %g "
                             "(%d), handover %g (%g), refused %g (%g)\n",
                             calls, r.n_state, r.n_handover, r.n_refused,
                             (double)r.state, st, (double)r.handover, ho,
                             (double)r.refused, (double)rf);
            }
          if (st >= 0 && st < 5 && !seen[st])
            {
              seen[st]         = 1;
              order[n_order++] = st;
            }
          if (st == ASYNC_DSSS_RX_REFINING)
            seed_hz = ho;
          if (st == ASYNC_DSSS_RX_TRACKING && isnan (handed_hz))
            handed_hz = ho;
        }
    }
  DP_CHECK (bad == 0);
  /* The life, in order: searching, refining, tracking, lost. */
  DP_CHECK (n_order == 4);
  if (n_order == 4)
    DP_CHECK (order[0] == ASYNC_DSSS_RX_SEARCHING
              && order[1] == ASYNC_DSSS_RX_REFINING
              && order[2] == ASYNC_DSSS_RX_TRACKING
              && order[3] == ASYNC_DSSS_RX_LOST);
  /* The hand-over reported the refined Doppler, not the seed's: within one
     bin of the refine that produced it -- its own rate over its zero-padded
     FFT -- of the emitter's. */
  const double refine_bin_hz
      = (double)rx->refine_samples_per_symbol * rx->symbol_rate
        / (double)(rx->refine_n_fft * rx->refine_zero_pad);
  DP_CHECK (isfinite (seed_hz) && isfinite (handed_hz));
  printf ("  lifecycle probes: seed %g Hz, handed over %g Hz, emitter %g Hz, "
          "%zu calls\n",
          seed_hz, handed_hz, doppler_hz, calls);
  DP_CHECK (handed_hz != seed_hz);
  DP_CHECK (fabs (handed_hz - doppler_hz) < refine_bin_hz);

  free (out);
  free (off);
  free (data);
  free (x);
  dp_async_dsss_receiver_destroy (rx);
  dp_tlm_destroy (t);
  return 0;
}

/* The cell flavor's two states the life above never visits: idle before a
 * seed, and its refused count, which no stimulus here can raise -- so it is
 * set in place, as _test_refused_hits_round_trips does, and the probe must
 * read it. A seed reports at once, and reset() returns the probes to idle
 * on the same ids. Sabotage: emit 0 for ".refused" -> red; report nothing
 * while idle or lost (emit only from the tracking path) -> red. */
static int
_test_tlm_idle_and_refused (void)
{
  const double                    cn0 = 70.0;
  dp_async_dsss_receiver_state_t *rx  = _seeded_rx (cn0, 0.0);
  dp_tlm_t                       *t   = dp_tlm_create (1 << 14);
  DP_REQUIRE (rx != NULL && t != NULL);
  DP_REQUIRE (dp_async_dsss_receiver_set_telemetry (rx, t, "rx", 1) == DP_OK);
  const int id_state    = dp_tlm_probe_id (t, "rx.state");
  const int id_handover = dp_tlm_probe_id (t, "rx.handover_hz");
  const int id_refused  = dp_tlm_probe_id (t, "rx.refused");
  DP_REQUIRE (id_state >= 0 && id_handover >= 0 && id_refused >= 0);

  float _Complex *noise = _noise_tail (256, 4.0e6, cn0, 5);
  float _Complex out[2048];

  /* Idle: the call is consumed and discarded, and still reported. */
  (void)dp_async_dsss_receiver_steps (rx, noise, 256, out, 2048);
  _tlm_trio_t r = _tlm_read_trio (t, id_state, id_handover, id_refused);
  DP_CHECK (r.n_state == 1 && r.state == (float)ASYNC_DSSS_RX_IDLE);
  DP_CHECK (r.n_handover == 1 && r.handover == 0.0f);
  DP_CHECK (r.n_refused == 1 && r.refused == 0.0f);

  rx->refused_hits = 5;
  (void)dp_async_dsss_receiver_steps (rx, noise, 256, out, 2048);
  r = _tlm_read_trio (t, id_state, id_handover, id_refused);
  DP_CHECK (r.n_refused == 1 && r.refused == 5.0f);

  /* A seed: the pull-in reports refining, at the seed's Doppler. */
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 1234.5, cn0) == DP_OK);
  (void)dp_async_dsss_receiver_steps (rx, noise, 256, out, 2048);
  r = _tlm_read_trio (t, id_state, id_handover, id_refused);
  DP_CHECK (r.n_state == 1 && r.state == (float)ASYNC_DSSS_RX_REFINING);
  DP_CHECK (r.n_handover == 1 && r.handover == 1234.5f);

  /* reset(): idle again, on the same ids, and nothing new registers. */
  const size_t n_probes = dp_tlm_probe_count (t);
  dp_async_dsss_receiver_reset (rx);
  (void)dp_async_dsss_receiver_steps (rx, noise, 256, out, 2048);
  r = _tlm_read_trio (t, id_state, id_handover, id_refused);
  DP_CHECK (r.n_state == 1 && r.state == (float)ASYNC_DSSS_RX_IDLE);
  DP_CHECK (r.n_handover == 1 && r.handover == 0.0f);
  DP_CHECK (dp_tlm_probe_count (t) == n_probes);

  /* Detached: nothing more is written. */
  DP_CHECK (dp_async_dsss_receiver_set_telemetry (rx, NULL, NULL, 1) == DP_OK);
  (void)dp_async_dsss_receiver_steps (rx, noise, 256, out, 2048);
  r = _tlm_read_trio (t, id_state, id_handover, id_refused);
  DP_CHECK (r.n_state == 0 && r.n_handover == 0 && r.n_refused == 0);

  free (noise);
  dp_async_dsss_receiver_destroy (rx);
  dp_tlm_destroy (t);
  return 0;
}

/* set_telemetry()'s probe list is the header's claim about what a caller
 * will find, so every name either flavor registers sits under a prefix the
 * header documents -- ".car", ".code", ".demod", ".acq" (searching only)
 * -- or is one of the receiver's own five; and ".rc" registers nothing, as
 * the header says, because the rate converter is built without an AGC.
 * Sabotage: rename a probe -> red; register an undocumented one -> red;
 * register one under ".rc" -> red. */
static int
_test_tlm_names_documented (void)
{
  static const char *const children[]
      = { "rx.car.", "rx.code.", "rx.demod.", "rx.acq." };
  static const char *const own[]
      = { "rx.state", "rx.handover_hz", "rx.refused", "rx.sym.lock",
          "rx.sym.locked" };
  enum
  {
    N_CHILD = sizeof children / sizeof children[0],
    N_OWN   = sizeof own / sizeof own[0]
  };
  for (int cell = 0; cell < 2; cell++)
    {
      dp_async_dsss_receiver_state_t *rx
          = cell ? _seeded_rx (70.0, 0.0)
                 : _searching_rx (70.0, 35714.29, 0.0, false);
      dp_tlm_t *t = dp_tlm_create (1 << 12);
      DP_REQUIRE (rx != NULL && t != NULL);
      DP_REQUIRE (dp_async_dsss_receiver_set_telemetry (rx, t, "rx", 1)
                  == DP_OK);
      for (size_t i = 0; i < N_OWN; i++)
        DP_CHECK_MSG (dp_tlm_probe_id (t, own[i]) >= 0, own[i]);
      size_t acq = 0;
      for (uint32_t i = 0; i < t->n_probes; i++)
        {
          const char *name   = t->probes[i].name;
          int         listed = 0;
          for (size_t j = 0; j < N_OWN && !listed; j++)
            listed = strcmp (name, own[j]) == 0;
          for (size_t j = 0; j < N_CHILD && !listed; j++)
            listed = strncmp (name, children[j], strlen (children[j])) == 0;
          acq += strncmp (name, "rx.acq.", 7) == 0;
          DP_CHECK_MSG (listed, name);
          DP_CHECK_MSG (strncmp (name, "rx.rc.", 6) != 0, name);
        }
      /* The search is the searching flavor's alone. */
      DP_CHECK (cell ? acq == 0 : acq > 0);
      dp_async_dsss_receiver_destroy (rx);
      dp_tlm_destroy (t);
    }
  return 0;
}

int
main (void)
{
  (void)_test_arg_validation ();
  DP_CHECK (_test_forged_cell_searching_refused () == 0);
  DP_CHECK (_test_seed_domain () == 0);
  DP_CHECK (_test_forged_residual_refused () == 0);
  DP_CHECK (_test_handover_holds_the_refined_doppler_to_the_domain () == 0);
  DP_CHECK (_test_refused_hits_round_trips () == 0);
  DP_CHECK (_test_forged_seed_is_refused () == 0);
  (void)_test_acquire_and_decode ();
  (void)_test_handoff_resumes_at_the_hit ();
  (void)_test_handoff_survives_reconfigure_and_restore ();
  (void)_test_give_up_cap ();
  (void)_test_spec_ramp_decode ();
  (void)_test_handover_under_clock_offset ();
  (void)_test_spec_combined_scenario_at_spec_floor ();
  (void)_test_awgn_esn0_floor ();
  (void)_test_accessor_coverage ();
  (void)_test_cell_seed_and_decode ();
  (void)_test_telemetry_follows_the_loops ();
  (void)_test_telemetry_reaches_the_search ();
  (void)_test_seed_on_searching_flavor ();
  (void)_test_lost_after_switch_off ();
  (void)_test_reacquire_after_switch_off ();
  (void)_test_reacquire_is_symbol_lock_alone ();
  (void)_test_reacquire_clock_restarts_on_lock ();
  (void)_test_configure_chain_raw_keeps_the_carrier ();
  (void)_test_refine_stops_at_its_decision ();
  (void)_test_refine_min_blocks_travels ();
  (void)_test_a_handoff_that_never_locks_is_released ();
  (void)_test_plain_clock_restarts_on_a_flag ();
  (void)_test_symbol_lock_detector_symbol_by_symbol ();
  (void)_test_emission_by_state ();
  DP_CHECK (_test_output_capacity () == 0);
  DP_CHECK (_test_differential_does_not_change_the_symbols () == 0);
  (void)_test_one_flag_down_is_a_degrade ();
  (void)_test_refine_dwell_floor ();
  (void)_test_seeded_blob_keyed_by_flavor ();
  (void)_test_carry_tail_is_zeros ();
  (void)_test_status_record ();
  (void)_test_cell_lifecycle_and_args ();
  (void)_test_cell_seed_past_pullin_is_estimated ();
  (void)_test_cell_holds_and_decodes ();
  (void)_test_cell_holds_through_switch_off ();
  (void)_test_cell_ramp ();
  (void)_test_cell_ramp_at_spec ();
  (void)_test_cell_state_roundtrip ();
  DP_CHECK (_test_tlm_lifecycle_probes () == 0);
  DP_CHECK (_test_tlm_idle_and_refused () == 0);
  DP_CHECK (_test_tlm_names_documented () == 0);

  DP_TEST_END ("test_async_dsss_receiver_core");
}
