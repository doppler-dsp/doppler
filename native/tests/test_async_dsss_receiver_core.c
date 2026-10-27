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
 * check, and the specification's own combined-scenario geometry (500 Hz/s
 * Doppler (docs/design/async-dsss-receiver.md) ramp, Gold-1023-style code /
 * async BPSK data clock) at two operating points: a moderately-stressed Es/N0
 * where full decode is verified
 * (_test_spec_ramp_decode(), proving this object's own new machinery --
 * the refine stage and its per-code-period carrier cadence -- works
 * correctly under a real ramp), and the specification's own literal 5dB floor
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
 * 11.1-11.2): seed() and its range checks, assigned once, the release rule
 * (both flags down past lost_confirm_s -> lost, not before, never at 0)
 * and the state round trip.
 */
#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/carrier_acq/carrier_acq_core.h" /* its blob magic, for forging */
#include "doppler/doppler_channel/doppler_channel_core.h"
#include "doppler/gold/gold_core.h" /* the specification Gold-1023 for the Es/N0-floor sweep   */
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
                4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0)
            == NULL);
  /* A 1-chip code: refused, where the Dll built under dp_xnn used to abort
     the process once it refused the code (doppler#2103). */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 1, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0)
            == NULL);
  /* Refused by create itself, where each reached a dp_xnn build and
     aborted the process (doppler#2103): the searcher's own domain (a pfa of
     2), sps = 1, which carries no m_out, and a carrier so small that
     doppler / carrier overflows. The control is the same receiver at good
     values, so it is the one argument that is refused. */
  {
    dp_async_dsss_receiver_state_t *ok = dp_async_dsss_receiver_create (
        CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5, 4,
        14.0, 64, 8, false, 100000, 2.5e9, 0.0, false, 0.0, 0.0);
    DP_CHECK (ok != NULL);
    dp_async_dsss_receiver_destroy (ok);
  }
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 2.0, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 2.5e9, 0.0, false, 0.0, 0.0)
            == NULL); /* pfa */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 1, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 2.5e9, 0.0, false, 0.0, 0.0)
            == NULL); /* sps */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 1e-305, 0.0, false, 0.0, 0.0)
            == NULL); /* carrier */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 1.5, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 2.5e9, 0.0, false, 0.0, 0.0)
            == NULL); /* pd */
  /* A symbol rate at either extreme: 5e-324 underflowed the chain's ratio
     to 0, and one past the chip rate sized the scratch at ~4e12 samples.
     The 2^20-partial cap holds at one segment too (doppler#2103). */
  DP_CHECK (dp_async_dsss_receiver_create (CODE7, 7, 1e6, 5e-324, 2, 2, 55.0,
                                           1e-3, 0.9, 100.0, 4, 8, 0, 0.5, 4,
                                           14.0, 64, 8, false, 100000, 2.5e9,
                                           0.0, false, 0.0, 0.0)
            == NULL); /* symbol_rate underflows the ratio */
  DP_CHECK (dp_async_dsss_receiver_create (CODE7, 7, 1e6, 1e15, 2, 2, 55.0,
                                           1e-3, 0.9, 100.0, 4, 8, 0, 0.5, 4,
                                           14.0, 64, 8, false, 100000, 2.5e9,
                                           0.0, false, 0.0, 0.0)
            == NULL); /* symbol_rate past the chip rate */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 0.1, 2, 2, 55.0, 1e-3, 0.9, 100.0, 1, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 2.5e9, 0.0, false, 0.0, 0.0)
            == NULL); /* 1e6 / (7 * 0.1) partials at ONE segment */
  /* A symbol of more than 2^20 Dll partials -- here 4 * 1e6 / (7 * 0.1),
     about 5.7e6 -- is refused at create by the receiver's own chain-ratio
     clause, segments * chip_rate / (code_len * symbol_rate) <= 2^20. That
     is the same count the Dll's aid refuses past, so this case pins the
     receiver's clause, not the aid's: the tracker sets the period
     mid-stream, where a refusal used to drop the aid silently (#2103). */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 0.1, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0)
            == NULL);
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 0.0, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0)
            == NULL); /* chip_rate <= 0 */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 3, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0)
            == NULL); /* m not in {2,4,8} */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 0, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0)
            == NULL); /* segments < 1 */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, -1.0, 0.0, false, 0.0, 0.0)
            == NULL); /* carrier_freq_hz < 0 */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 0.0, -1.0, false, 0.0, 0.0)
            == NULL); /* lost_confirm_s < 0 */
  DP_CHECK (dp_async_dsss_receiver_create (
                CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5,
                4, 14.0, 64, 8, false, 100000, 0.0, NAN, false, 0.0, 0.0)
            == NULL); /* lost_confirm_s NaN */

  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5, 4,
      14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
  DP_CHECK (rx3 != NULL);
  if (rx3)
    {
      size_t cb3   = dp_async_dsss_receiver_state_bytes (rx3);
      void  *blob3 = malloc (cb3);
      dp_async_dsss_receiver_get_state (rx3, blob3);

      dp_async_dsss_receiver_state_t *rx4 = dp_async_dsss_receiver_create (
          CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0,
          0.5, 4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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
      0.0 /* carrier_freq_hz: aiding off */, 0.0, false, 0.0, 0.0);
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
 * RateConverter/MpskReceiver chain, at the specification's own real
 * operating-point RATIOS (chip_rate=3.069e6, code_len=1023, symbol_rate=2700
 * -- CCSDS Gold-1023 @ 3.069 Mcps / async BPSK @ 2700 bps) against a 500 Hz/s
 * Doppler ramp (the specification's corrected worst case), run once at a
 * moderately stressed Es/N0 where the shared MpskReceiver/Dll chain is known
 * able to lock at all (see _test_spec_combined_scenario_at_spec_floor()'s own
 * comment for why the specification's literal 5dB floor is NOT used here).
 * Proves THIS object's own new machinery -- the refine stage's frequency
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
                                       clock relationship the specification's
                                      own  waveform has. */
  const double tsym          = fs / sym_rate;
  const size_t te            = sf * spc;
  const double rate_hz_per_s = 500.0;
  const size_t n_sym         = 2430;
  const size_t pre_silence   = te * 5 + 3;
  const double esn0_db       = 30.0; /* moderately stressed -- see the
                                         comment above; not the specification's own
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
      0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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
 * The stimulus is the shipped channel at the specification's 20 ppm of a 2.5
 * GHz carrier with no rate (the specification's two worst cases do not
 * coincide); three seeds closing and one opening, every one must lock the code
 * and decode, at 45 dB-Hz with the margin the old test used and at the floor
 * with the shipped one. */
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
  /* the specification's 20 ppm: 50 kHz, 100 chips/s of dilation. Two operating
     points: 45 dB-Hz with a design margin of 19 dB, which sizes the same
     7-block, 42 ms refine dwell that the shipped 14 dB does at 40 dB-Hz
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
          100000, carrier, 0.0, false, 0.0, 0.0);
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

/* the specification's own literal Es/N0=5dB floor, same geometry/ramp as
 * _test_spec_ramp_decode() above. task #99's own cliff at this exact
 * operating point was ALREADY characterized as "pure-SNR, rate-
 * independent" with "Acquisition's own hit quality/reliability... the
 * leading remaining candidate, not yet directly inspected" (FINISHING_
 * PLAN.md). Direct measurement while building this object CONFIRMED that
 * diagnosis and ruled out this object's own refine stage as the fix:
 * the ALREADY-SHIPPED, already-validated `DsssReceiver` fails to decode
 * (BER~0.43, lock~0.55) at this exact Es/N0 even given a trivial STATIC
 * ZERO Doppler offset -- no frequency estimation error at all, coarse or
 * refined. So this object cannot be expected to decode at the specification's
 * literal floor either (task #99's real fix is elsewhere -- see the comment
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
  const double esn0_db       = 5.0; /* the specification's own floor */
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
      0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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

/* AWGN-only Es/N0 decode floor at the specification's own geometry
 * (Gold-1023, 3.069 Mcps, 2700 bps asynchronous BPSK, spc=2), ZERO Doppler --
 * characterizes the decode floor, independent of Doppler and of the aiding
 * path. Generated with the wfmgen C API (dp_wfm_cont_dsss_chips) so this test
 * exercises the same continuous-DSSS builder the wfmgen tool ships. Sweeps
 * Es/N0 and prints three metrics (visible under `ctest -V`) so the floor is
 * a tracked, inspectable quantity: the truth-referenced BER PLUS two
 * truth-free validators that cannot be fooled by a lucky BER lag/polarity --
 * the self-referenced EVM (each symbol vs its OWN hard decision, no lag) and
 * the blind M2M4 SNR (snr_m2m4_db, moment-based, no reference symbols).
 * AWGN-only, this receiver essentially MEETS the specification's 5 dB floor:
 * it decodes cleanly at 5 dB and only fails at 4 dB. (An earlier "~12 dB
 * floor" was a pure BER-lag artifact -- the settling delay grows toward the
 * floor, so a fixed short lag window reports chance while the receiver
 * actually decodes; the EVM/M2M4 corroboration is what caught and corrected
 * it.)
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
          0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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

  /* AWGN-only, this receiver essentially MEETS the specification's 5 dB floor:
     it decodes cleanly at 5 dB and only fails at 4 dB. (An earlier
     characterization put the floor ~12 dB -- that was purely a narrow BER
     lag-search artifact: the settling delay grows toward the floor, so a fixed
     short lag window misses the alignment and reports chance even while the
     receiver decodes perfectly. Widening the lag AND cross-checking with the
     truth-free EVM/M2M4 validators is what corrected it.) */
  DP_CHECK (ber_at[4] < 0.05); /* 10 dB: clean */
  DP_CHECK (ber_at[3] < 0.05); /* 8 dB: clean */
  DP_CHECK (
      ber_at[1]
      < 0.05); /* 5 dB: MEETS the specification's 5 dB floor (AWGN-only) */
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

/* The CODE7 fixture's receiver: every number the lifecycle tests use, the
 * release interval and reacquire given. Most of them seed it (seed())
 * rather than wait on its own search. */
static dp_async_dsss_receiver_state_t *
_searching_rx (double cn0, double sym_rate, double lost_confirm_s,
               bool reacquire)
{
  return dp_async_dsss_receiver_create (
      CODE7, 7, 1.0e6, sym_rate, 4, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5, 4,
      14.0, 32, 8, false, 100000, 0.0, lost_confirm_s, reacquire, 0.0, 0.0);
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
  dp_async_dsss_receiver_state_t *rx
      = _searching_rx (cn0, 35714.29, 0.0, false);
  dp_tlm_t *t = dp_tlm_create (1 << 18);
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
  /* The carrier, the code loop, the demodulator and the search, all
     registered and none run yet. The loop-filter outputs are among them, both
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
  DP_CHECK (_tlm_emitted (t, "rx.acq.stat") == 0);

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
      14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }
  DP_CHECK (dp_async_dsss_receiver_status (rx).state
            == ASYNC_DSSS_RX_SEARCHING);

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
  /* Assigned once: a receiver that holds a seed refuses another, refining
     or tracking, until reset() releases it. Sabotage: accept a seed in any
     state but lost -> red. */
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 3.0, 0.0, cn0) == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_get_refining (rx) == 1);

  float _Complex *syms;
  size_t n_syms = _stream (rx, x + pre_silence, n - pre_silence, te, &syms);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 3.0, 0.0, cn0) == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (n_syms > 20);
  DP_CHECK (_best_ber (syms, n_syms, data, n_sym + 4) < 0.05);
  DP_CHECK (dp_test_evm_db_hard (syms, n_syms) < -8.0);

  /* reset() on this flavor is still the search. */
  dp_async_dsss_receiver_reset (rx);
  DP_CHECK (dp_async_dsss_receiver_status (rx).state
            == ASYNC_DSSS_RX_SEARCHING);
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
      dp_async_dsss_receiver_state_t *rx
          = _searching_rx (cn0, 35714.29, intervals[k], false);
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
          dp_async_dsss_receiver_state_t *rx2
              = _searching_rx (cn0, 35714.29, intervals[k], false);
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
          DP_CHECK (dp_async_dsss_receiver_status (rx).state
                    == ASYNC_DSSS_RX_SEARCHING);
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
      0.5, 4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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
 * serialized, restores into a default-built one with that floor. Sabotage:
 * leave it out of the blob -> red.
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

  return 0;
}

/* A hand-off that never locks is released (#2461 #76, #78): the release
 * clock runs from the first tracking sample, before either flag has come
 * up, so a track on noise -- the false alarm the receiver cannot tell from
 * a signal until a lock flag fails to come -- is let go. The receiver is
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

  /* rows: plain, reacquire */
  for (int row = 0; row < 2; row++)
    {
      dp_async_dsss_receiver_state_t *rx
          = _searching_rx (cn0, sym_rate, confirm_s, row == 1);
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
      if (row == 1)
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

/* What each state emits, and destroy(NULL) (#2461 #110, #96). The
 * receiver emits nothing in a call it spends refining, and destroy(NULL) is
 * a no-op. It asserts its precondition (calls observed in the state).
 * Sabotage: emitting from the refine chain -> red. */
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

  /* Every call that ends refining emitted 0 (#110). */
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
 * a caller knows the flag is inert here (#2471): two receivers that
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
      dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
          CODE7, 7, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 0.0, 4, 8, d, 0.5,
          4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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

  dp_async_dsss_receiver_state_t *rx
      = _searching_rx (cn0, 35714.29, confirm_s, false);
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

/* The blob round-trips: a seeded receiver's state resumes bit-for-bit
 * into another, and a receiver of another configuration refuses it. */
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
      14.0, 64, 8, false, 100000, 2.5e9, 2.0, false, 0.0, 0.0);
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
      14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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
_test_seeded_blob_round_trip (void)
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

  /* A searching receiver's round trip first: before any seed. */
  dp_async_dsss_receiver_state_t *ra
      = _searching_rx (cn0, 35714.29, 2.0, false);
  dp_async_dsss_receiver_state_t *rb
      = _searching_rx (cn0, 35714.29, 2.0, false);
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
    DP_CHECK (dp_async_dsss_receiver_set_state (rb, blob) == DP_OK);
    DP_CHECK (dp_async_dsss_receiver_status (rb).state
              == ASYNC_DSSS_RX_SEARCHING);
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
  /* The shared harness: every byte written, b re-serializes as a, the
     envelope reject (dp_state_test.h). */
  DP_STATE_ROUNDTRIP_TEST (dp_async_dsss_receiver, ra, rb);

  size_t cb   = dp_async_dsss_receiver_state_bytes (ra);
  void  *blob = malloc (cb);
  dp_async_dsss_receiver_get_state (ra, blob);
  DP_CHECK (dp_async_dsss_receiver_set_state (rb, blob) == DP_OK);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (rb) == 1);

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

  /* Across configurations (another sps, a layout key): refused both ways,
     and the envelope reject still holds. */
  dp_async_dsss_receiver_state_t *rs = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 16, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 2.0, false, 0.0, 0.0);
  DP_CHECK (rs != NULL);
  if (rs)
    {
      DP_CHECK (dp_async_dsss_receiver_set_state (rs, blob) == DP_ERR_INVALID);
      DP_CHECK (dp_async_dsss_receiver_get_tracking (rs) == 0);
      size_t cbs   = dp_async_dsss_receiver_state_bytes (rs);
      void  *blobs = malloc (cbs);
      dp_async_dsss_receiver_get_state (rs, blobs);
      DP_CHECK (cbs != cb); /* sps sizes the blob: refused on size first */
      DP_CHECK (dp_async_dsss_receiver_set_state (rb, blobs)
                == DP_ERR_INVALID);
      DP_CHECK (dp_async_dsss_receiver_get_tracking (rb) == 1); /* untouched */
      free (blobs);
      dp_async_dsss_receiver_destroy (rs);
    }
  /* A state no receiver can be in: 3 is reserved (the deleted cell mode's
     idle, kept because the values are persisted) and 5 is past the last.
     Forged into a valid blob, each is refused and rb is left as it was.
     Sabotage: drop either refusal in set_state() -> red. */
  {
    async_dsss_receiver_extra_t *ex
        = (async_dsss_receiver_extra_t *)((char *)blob
                                          + sizeof (dp_state_hdr_t));
    const uint8_t was = ex->state;
    for (int k = 0; k < 2; k++)
      {
        ex->state = k == 0 ? (uint8_t)ASYNC_DSSS_RX_RESERVED : 5u;
        DP_CHECK (dp_async_dsss_receiver_set_state (rb, blob)
                  == DP_ERR_INVALID);
        DP_CHECK (dp_async_dsss_receiver_get_tracking (rb) == 1);
      }
    ex->state = was;
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
 * owns it (nothing while searching, the seed while refining, the live carrier
 * loop once tracking), and the two clocks are the ones the release rule runs.
 */
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

  dp_async_dsss_receiver_state_t *rx
      = _searching_rx (cn0, 35714.29, 0.02, false);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }

  /* Searching: nothing to report but the state and the clock. */
  async_dsss_receiver_status_t st = dp_async_dsss_receiver_status (rx);
  DP_CHECK (st.state == ASYNC_DSSS_RX_SEARCHING);
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
    dp_async_dsss_receiver_state_t *rd
        = _searching_rx (cn0, 35714.29, 0.0, false);
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
      14.0, 64, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
  DP_CHECK (rx != NULL);
  if (!rx)
    return 1;

  /* Read-only accessors: each executes its one-line body on live state. */
  (void)dp_async_dsss_receiver_get_lock (rx);
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

  /* create's derived n is one configure_chain_raw accepts (doppler#2650):
     2 at sps 3, 4 at 5, 8 at 10, none dividing sps. The same rule as
     DsssReceiver's, mpsk_rx_m_out_ok(). */
  static const size_t odd_sps[] = { 3, 5, 10 };
  for (size_t k = 0; k < sizeof odd_sps / sizeof *odd_sps; k++)
    {
      dp_async_dsss_receiver_state_t *r = dp_async_dsss_receiver_create (
          CODE7, 7, 1.0e6, 35714.29, 4, 2, 70.0, 1e-2, 0.9, 500.0, 4,
          odd_sps[k], 0, 0.5, 4, 14.0, 64, 8, false, 100000, 0.0, 0.0, false,
          0.0, 0.0);
      DP_REQUIRE (r != NULL);
      const int n = dp_async_dsss_receiver_get_n (r);
      DP_CHECK ((size_t)n == mpsk_rx_derive_m_out ((double)odd_sps[k], 0));
      DP_CHECK (
          dp_async_dsss_receiver_configure_chain_raw (r, 4, odd_sps[k], n)
          == 0);
      DP_CHECK (dp_async_dsss_receiver_get_n (r) == n);
      dp_async_dsss_receiver_destroy (r);
    }
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
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
  dp_async_dsss_receiver_state_t *chunked = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, 35714.29, spc, 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
  dp_async_dsss_receiver_state_t *ref = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, 35714.29, spc, 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
  dp_async_dsss_receiver_state_t *b = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, 35714.29, spc, 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
  dp_async_dsss_receiver_state_t *whole = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, 35714.29, spc, 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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

  /* (a) Reconfigured mid-search: the engine restarts at 0 on the next
     block boundary of the receiver's own count (one epoch here, D = 1;
     get_coherent_bins()); a fresh receiver configured the same way and fed
     the rest from that boundary is the truth. */
  const size_t on_grid = (cut + te - 1) / te * te;
  DP_CHECK (dp_async_dsss_receiver_steps (a, x, cut, o1, n) == 0);
  DP_CHECK (dp_async_dsss_receiver_configure_search_raw (a, 1, 4) == 0);
  DP_CHECK (dp_async_dsss_receiver_configure_search_raw (ref, 1, 4) == 0);
  size_t na = dp_async_dsss_receiver_steps (a, x + cut, n - cut, o1, n);
  size_t nr
      = dp_async_dsss_receiver_steps (ref, x + on_grid, n - on_grid, o2, n);
  DP_CHECK (dp_async_dsss_receiver_get_tracking (a) == 1);
  DP_CHECK (na > 20 && na == nr && memcmp (o1, o2, na * sizeof *o1) == 0);

  /* (b) Restored mid-search into a fresh receiver: the uninterrupted run
     over the whole capture is the truth. */
  dp_async_dsss_receiver_state_t *src = dp_async_dsss_receiver_create (
      CODE7, sf, 1.0e6, 35714.29, spc, 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, 0.0, 0.0);
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
          0.5, 4, 14.0, 64, 8, false, 100000, 2.5e9, 2.0, false, 0.0, 0.0);
      dp_async_dsss_receiver_state_t *dst = dp_async_dsss_receiver_create (
          code, 1023, 5.0e6, 2700.0, 2, 2, 45.0, 1e-3, 0.9, 100.0, 4, 8, 0,
          0.5, 4, 14.0, 64, 8, false, 100000, 2.5e9, 2.0, false, 0.0, 0.0);
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
          0.5, 4, 14.0, 64, 8, false, 100000, 2.5e9, 2.0, false, 0.0, 0.0);
      dp_async_dsss_receiver_state_t *dst = dp_async_dsss_receiver_create (
          code, 1023, 5.0e6, 2700.0, 2, 2, 45.0, 1e-3, 0.9, 100.0, 4, 8, 0,
          0.5, 4, 14.0, 64, 8, false, 100000, 2.5e9, 2.0, false, 0.0, 0.0);
      DP_REQUIRE (src != NULL && dst != NULL);
      DP_REQUIRE (dp_async_dsss_receiver_seed (src, 0.0, seeds[k], 45.0)
                  == DP_OK);
      /* Off the block grid: 100 samples in, so a restart owes 1946. */
      src->samples_in     = 100;
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
              /* The search restarts on the grid: 1124 in, owed to 2046. */
              DP_CHECK (dst->align_pending && dp_acq_position (dst->acq) == 0);
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
      14.0, 64, 8, false, 100000, 2.5e9, 2.0, false, 0.0, 0.0);
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
      14.0, 64, 8, false, 100000, 2.5e9, 2.0, false, 0.0, 0.0);
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
          0.5, 4, 14.0, 64, 8, false, 100000, 2.5e9, 2.0, false, 0.0, 0.0);
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
      14.0, 64, 8, false, 100000, 2.5e9, 2.0, false, 0.0, 0.0);
  dp_async_dsss_receiver_state_t *dst = dp_async_dsss_receiver_create (
      code, 1023, 5.0e6, 2700.0, 2, 2, 45.0, 1e-3, 0.9, 100.0, 4, 8, 0, 0.5, 4,
      14.0, 64, 8, false, 100000, 2.5e9, 2.0, false, 0.0, 0.0);
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

/* Two things the life above never shows: a call that only searches, and
 * the refused count, which no stimulus here can raise -- so it is set in
 * place, as _test_refused_hits_round_trips does, and the probe must read
 * it. A seed reports at once, and reset() returns the probes to searching
 * on the same ids. The searching calls are shorter than one code period, so
 * no dwell decides and noise cannot hand off. Sabotage: emit 0 for
 * ".refused" -> red; report nothing while searching or lost (emit only
 * from the tracking path) -> red. */
static int
_test_tlm_searching_and_refused (void)
{
  const double                    cn0 = 70.0;
  dp_async_dsss_receiver_state_t *rx
      = _searching_rx (cn0, 35714.29, 0.0, false);
  dp_tlm_t *t = dp_tlm_create (1 << 14);
  DP_REQUIRE (rx != NULL && t != NULL);
  DP_REQUIRE (dp_async_dsss_receiver_set_telemetry (rx, t, "rx", 1) == DP_OK);
  const int id_state    = dp_tlm_probe_id (t, "rx.state");
  const int id_handover = dp_tlm_probe_id (t, "rx.handover_hz");
  const int id_refused  = dp_tlm_probe_id (t, "rx.refused");
  DP_REQUIRE (id_state >= 0 && id_handover >= 0 && id_refused >= 0);

  float _Complex *noise = _noise_tail (256, 4.0e6, cn0, 5);
  float _Complex out[2048];

  /* Searching: the call emits nothing, and is still reported. */
  (void)dp_async_dsss_receiver_steps (rx, noise, 16, out, 2048);
  _tlm_trio_t r = _tlm_read_trio (t, id_state, id_handover, id_refused);
  DP_CHECK (r.n_state == 1 && r.state == (float)ASYNC_DSSS_RX_SEARCHING);
  DP_CHECK (r.n_handover == 1 && r.handover == 0.0f);
  DP_CHECK (r.n_refused == 1 && r.refused == 0.0f);

  rx->refused_hits = 5;
  (void)dp_async_dsss_receiver_steps (rx, noise, 16, out, 2048);
  r = _tlm_read_trio (t, id_state, id_handover, id_refused);
  DP_CHECK (r.n_refused == 1 && r.refused == 5.0f);

  /* A seed: the refine reports refining, at the seed's Doppler. */
  DP_CHECK (dp_async_dsss_receiver_seed (rx, 0.0, 1234.5, cn0) == DP_OK);
  (void)dp_async_dsss_receiver_steps (rx, noise, 256, out, 2048);
  r = _tlm_read_trio (t, id_state, id_handover, id_refused);
  DP_CHECK (r.n_state == 1 && r.state == (float)ASYNC_DSSS_RX_REFINING);
  DP_CHECK (r.n_handover == 1 && r.handover == 1234.5f);

  /* reset(): searching again, on the same ids, and nothing new registers. */
  const size_t n_probes = dp_tlm_probe_count (t);
  dp_async_dsss_receiver_reset (rx);
  (void)dp_async_dsss_receiver_steps (rx, noise, 16, out, 2048);
  r = _tlm_read_trio (t, id_state, id_handover, id_refused);
  DP_CHECK (r.n_state == 1 && r.state == (float)ASYNC_DSSS_RX_SEARCHING);
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
 * will find, so every name it registers sits under a prefix the header
 * documents -- ".car", ".code", ".demod", ".acq" -- or is one of the
 * receiver's own five; and ".rc" registers nothing, as the header says,
 * because the rate converter is built without an AGC. Sabotage: rename a probe
 * -> red; register an undocumented one -> red; register one under ".rc" ->
 * red. */
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
  {
    dp_async_dsss_receiver_state_t *rx
        = _searching_rx (70.0, 35714.29, 0.0, false);
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
    DP_CHECK (acq > 0); /* the search registers */
    dp_async_dsss_receiver_destroy (rx);
    dp_tlm_destroy (t);
  }
  return 0;
}

/* The design point of docs/design/async-dsss-parallel.md: the specification
 * Gold-1023 receiver at 3.069 Mchip/s and 2700 symbols/s, two samples a chip.
 */
#define SLOW_CR 3.069e6
#define SLOW_RS 2700.0
#define SLOW_SPC 2

#define DP_CHECK_EQ_SZ(a, b) DP_CHECK ((size_t)(a) == (size_t)(b))
#define DP_CHECK_EQ_INT(a, b) DP_CHECK ((int)(a) == (int)(b))
#define DP_CHECK_EQ_U64(a, b) DP_CHECK ((uint64_t)(a) == (uint64_t)(b))

static dp_async_dsss_receiver_state_t *
_slow_rx (const uint8_t *code, double cn0, double du, double code_only_s,
          double doppler_rate)
{
  return dp_async_dsss_receiver_create (code, 1023, SLOW_CR, SLOW_RS, SLOW_SPC,
                                        2, cn0, 1e-3, 0.9, du, 4, 8, 0, 0.5, 4,
                                        14.0, 64, 8, false, 100000, 0.0, 0.0,
                                        false, code_only_s, doppler_rate);
}

/* The slow-time depth comes from the physical inputs (create()'s
 * @param code_only_s / doppler_rate): code_only_epochs = floor(W *
 * chip_rate / sf) - 1, D at most (epochs + 1)/2 and f_epoch /
 * sqrt(2 * rate), run 7-smooth. Sabotage: pass the old `1, 0.0` -> red at
 * the design point; drop the `- 1` -> the 7.5-epoch row reads 4, not 3;
 * drop the rate -> the 2000 Hz/s row reads 90, not 45. */
static int
_test_slow_time_depth_from_physical_inputs (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const double f_epoch = SLOW_CR / 1023.0; /* 3000 Hz */
  static const struct
  {
    double code_only_s, doppler_rate;
    size_t d;
  } rows[] = {
    { 0.0, 0.0, 1 },              /* no window: the epoch-by-epoch search */
    { 170.0 / SLOW_RS, 0.0, 90 }, /* 187 epochs: 94, run 7-smooth          */
    { 0.0627, 0.0, 90 },          /* the header's worked example           */
    { 1.5 / 3000.0, 0.0, 1 },     /* under two epochs: no window           */
    { 7.5 / 3000.0, 0.0, 3 },     /* floor 7 - 1 = 6 epochs: (6 + 1)/2 = 3 */
    { 170.0 / SLOW_RS, 2000.0, 45 }, /* the rate bounds it: 47 -> 45      */
    { 450.0 / SLOW_RS, 500.0,
      90 }, /* the specification: 499 epochs, 500 Hz/s -> 94  */
  };
  for (size_t i = 0; i < sizeof rows / sizeof rows[0]; i++)
    {
      dp_async_dsss_receiver_state_t *rx = _slow_rx (
          code, 45.0, 500.0, rows[i].code_only_s, rows[i].doppler_rate);
      DP_REQUIRE (rx != NULL);
      DP_CHECK_EQ_SZ (dp_async_dsss_receiver_get_coherent_bins (rx),
                      rows[i].d);
      dp_async_dsss_receiver_destroy (rx);
    }
  (void)f_epoch;
  /* Refused at create: a window that is not a time, a rate that is not. */
  DP_CHECK (_slow_rx (code, 45.0, 500.0, -1e-3, 0.0) == NULL);
  DP_CHECK (_slow_rx (code, 45.0, 500.0, NAN, 0.0) == NULL);
  DP_CHECK (_slow_rx (code, 45.0, 500.0, INFINITY, 0.0) == NULL);
  DP_CHECK (_slow_rx (code, 45.0, 500.0, 1e300, 0.0) == NULL);
  DP_CHECK (_slow_rx (code, 45.0, 500.0, 0.0, -1.0) == NULL);
  DP_CHECK (_slow_rx (code, 45.0, 500.0, 0.0, NAN) == NULL);
  return 0;
}

/* set_threads() changes the cost and nothing else: five Doppler tiles
 * searched serially and on a pool give the same symbols and the same
 * status, sample for sample. Sabotage: let a tile's rows depend on its
 * worker -> red. */
static int
_test_threads_change_nothing_but_cost (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const double    fs = SLOW_CR * SLOW_SPC;
  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (code, 1023, SLOW_SPC, fs, fs / SLOW_RS, 2500.0, 45.0, 300,
                   5003, 7, &x, &n, &data);
  dp_async_dsss_receiver_state_t *a = _slow_rx (code, 45.0, 5000.0, 0.0, 0.0);
  dp_async_dsss_receiver_state_t *b = _slow_rx (code, 45.0, 5000.0, 0.0, 0.0);
  DP_REQUIRE (a != NULL && b != NULL);
  DP_REQUIRE (a->acq->window_bins == 5); /* something to fan out */
  DP_CHECK_EQ_INT (dp_async_dsss_receiver_set_threads (a, 1), DP_OK);
  DP_CHECK_EQ_INT (dp_async_dsss_receiver_set_threads (b, 4), DP_OK);
  DP_CHECK_EQ_INT (dp_async_dsss_receiver_get_threads (a), 1);
  DP_CHECK_EQ_INT (dp_async_dsss_receiver_get_threads (b), 4);
  float _Complex *sa, *sb;
  const size_t    na = _stream (a, x, n, 4093, &sa);
  const size_t    nb = _stream (b, x, n, 4093, &sb);
  DP_CHECK (a->state == ASYNC_DSSS_RX_TRACKING); /* the search ran and hit */
  DP_CHECK_EQ_SZ (na, nb);
  DP_CHECK (na > 0 && memcmp (sa, sb, na * sizeof *sa) == 0);
  /* Every running field, through the blob: equal but for the thread count
     each carries. */
  const size_t   nbytes = dp_async_dsss_receiver_state_bytes (a);
  unsigned char *ba = malloc (nbytes), *bb = malloc (nbytes);
  dp_async_dsss_receiver_get_state (a, ba);
  dp_async_dsss_receiver_get_state (b, bb);
  ((async_dsss_receiver_extra_t *)(bb + sizeof (dp_state_hdr_t)))->threads = 1;
  DP_CHECK (memcmp (ba, bb, nbytes) == 0);
  free (ba);
  free (bb);
  free (sa);
  free (sb);
  free (x);
  free (data);
  dp_async_dsss_receiver_destroy (a);
  dp_async_dsss_receiver_destroy (b);
  return 0;
}

/* A restarted search starts its next dwell on a multiple of one block,
 * D * sf * spc input samples, counted from create (the header's
 * get_coherent_bins()): the engine's own position after the restart is what
 * was fed less what was owed to the boundary. The debt survives a split
 * through the blob, and chunking does not move it. Sabotage: skip the
 * alignment -> the position reads every sample fed; drop samples_in from the
 * blob -> the resumed receiver pays a different debt. */
static int
_test_dwells_start_on_the_block_grid (void)
{
  /* CODE7 at 1 Mchip/s: a 7 us epoch. 13.5 epochs of window -> 12 whole
     -> D = 6, a block of 6 * 7 * 4 = 168 samples. */
  const double    code_only_s = 13.5 * 7.0e-6;
  const size_t    block       = 6 * 7 * 4;
  const size_t    fed_before  = 1000; /* 1000 % 168 = 160: 8 owed after */
  const size_t    owed        = block - fed_before % block;
  float _Complex *pre         = _noise_tail (fed_before, 4.0e6, 40.0, 3);
  float _Complex *post        = _noise_tail (500, 4.0e6, 40.0, 4);
  float _Complex out[600];

  for (size_t chunk = 1; chunk <= 500; chunk = chunk * 3 + 4)
    {
      dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
          CODE7, 7, 1.0e6, 35714.29, 4, 2, 40.0, 1e-9, 0.9, 500.0, 4, 8, 0,
          0.5, 4, 14.0, 32, 8, false, 100000, 0.0, 0.0, false, code_only_s,
          0.0);
      DP_REQUIRE (rx != NULL);
      DP_REQUIRE (dp_async_dsss_receiver_get_coherent_bins (rx) == 6);
      (void)dp_async_dsss_receiver_steps (rx, pre, fed_before, out, 600);
      /* From create the grid starts at sample 0: nothing was owed. */
      DP_CHECK_EQ_U64 (dp_acq_position (rx->acq), (uint64_t)fed_before);
      dp_async_dsss_receiver_reset (rx);
      for (size_t pos = 0; pos < 500; pos += chunk)
        {
          const size_t take = pos + chunk <= 500 ? chunk : 500 - pos;
          (void)dp_async_dsss_receiver_steps (rx, post + pos, take, out, 600);
          const size_t fed = pos + take;
          DP_CHECK_EQ_U64 (dp_acq_position (rx->acq),
                           (uint64_t)(fed > owed ? fed - owed : 0));
        }
      DP_CHECK (rx->state == ASYNC_DSSS_RX_SEARCHING);
      dp_async_dsss_receiver_destroy (rx);
    }

  /* Split mid-debt: 3 of the 8 paid, the blob taken, the rest fed to both. */
  dp_async_dsss_receiver_state_t *a = dp_async_dsss_receiver_create (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, 40.0, 1e-9, 0.9, 500.0, 4, 8, 0, 0.5, 4,
      14.0, 32, 8, false, 100000, 0.0, 0.0, false, code_only_s, 0.0);
  dp_async_dsss_receiver_state_t *b = dp_async_dsss_receiver_create (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, 40.0, 1e-9, 0.9, 500.0, 4, 8, 0, 0.5, 4,
      14.0, 32, 8, false, 100000, 0.0, 0.0, false, code_only_s, 0.0);
  DP_REQUIRE (a != NULL && b != NULL);
  (void)dp_async_dsss_receiver_steps (a, pre, fed_before, out, 600);
  dp_async_dsss_receiver_reset (a);
  (void)dp_async_dsss_receiver_steps (a, post, 3, out, 600);
  DP_CHECK (a->align_pending);
  const size_t   nbytes = dp_async_dsss_receiver_state_bytes (a);
  unsigned char *blob   = malloc (nbytes);
  dp_async_dsss_receiver_get_state (a, blob);
  DP_REQUIRE (dp_async_dsss_receiver_set_state (b, blob) == DP_OK);
  (void)dp_async_dsss_receiver_steps (a, post + 3, 497, out, 600);
  (void)dp_async_dsss_receiver_steps (b, post + 3, 497, out, 600);
  DP_CHECK_EQ_U64 (dp_acq_position (a->acq), (uint64_t)(500 - owed));
  DP_CHECK_EQ_U64 (dp_acq_position (b->acq), (uint64_t)(500 - owed));
  /* A flag that is not a flag is a forged blob. */
  async_dsss_receiver_extra_t *extra
      = (async_dsss_receiver_extra_t *)(blob + sizeof (dp_state_hdr_t));
  extra->align_pending = 2;
  DP_CHECK (dp_async_dsss_receiver_set_state (b, blob) == DP_ERR_INVALID);
  free (blob);
  free (pre);
  free (post);
  dp_async_dsss_receiver_destroy (a);
  dp_async_dsss_receiver_destroy (b);
  return 0;
}

/* What the depth is for: a signal too weak for the epoch-by-epoch search to
 * decide inside one code-only window is found inside it at D = 90. 32 dB-Hz
 * at the design point: D = 90 decides on one 30 ms block, D = 1 sized for
 * the same C/N0 needs 256 looks (85 ms), past the 63 ms window. The found
 * Doppler is the emitter's to within one D = 90 bin (33 Hz), so the hit is
 * the signal and not a false alarm. Sabotage: create the engine at D = 1
 * regardless -> red. */
static int
_test_depth_finds_what_one_epoch_misses (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const double fs      = SLOW_CR * SLOW_SPC;
  const double tsym    = fs / SLOW_RS;
  const size_t w_sym   = 170;
  const double doppler = 150.0;
  for (uint32_t seed = 1; seed <= 3; seed++)
    {
      float _Complex *x;
      size_t          n;
      double         *data;
      dp_dsss_windowed_capture (code, 1023, SLOW_SPC, fs, tsym, doppler, 32.0,
                                w_sym, 0, seed, w_sym, w_sym + 400, &x, &n,
                                &data);
      const size_t    win = (size_t)((double)w_sym * tsym);
      float _Complex *out = malloc (win * sizeof *out);
      for (int deep = 0; deep < 2; deep++)
        {
          dp_async_dsss_receiver_state_t *rx = _slow_rx (
              code, 32.0, 500.0, deep ? (double)w_sym / SLOW_RS : 0.0, 0.0);
          DP_REQUIRE (rx != NULL);
          DP_REQUIRE (dp_async_dsss_receiver_get_coherent_bins (rx)
                      == (deep ? 90u : 1u));
          (void)dp_async_dsss_receiver_steps (rx, x, win, out, win);
          if (deep)
            {
              DP_CHECK_MSG (rx->state != ASYNC_DSSS_RX_SEARCHING,
                            "D = 90 finds it inside the window");
              DP_CHECK (fabs (rx->seed_doppler_hz_est - doppler)
                        < SLOW_CR / 1023.0 / 90.0);
            }
          else
            DP_CHECK_MSG (rx->state == ASYNC_DSSS_RX_SEARCHING
                              && rx->refused_hits == 0,
                          "D = 1 has not decided inside the window");
          dp_async_dsss_receiver_destroy (rx);
        }
      free (out);
      free (x);
      free (data);
    }
  return 0;
}

/* Two emitters on one Gold code at the design point, their code phases
 * ~400 chips apart: A at 50 dB-Hz carries the noise, and B, rendered all
 * but clean and scaled by 1/sqrt(2), sits 3 dB below it (the capture's
 * cn0_dbhz sets the NOISE; the signal is unit amplitude). The stream every
 * rank test feeds. `which`
 * 1 = A alone, 2 = B alone, 3 = both. Caller frees. */
static float _Complex *
_two_emitters (const uint8_t *code, int which, size_t *n_out)
{
  const double    fs = SLOW_CR * SLOW_SPC, tsym = fs / SLOW_RS;
  float _Complex *a, *b;
  size_t          na, nb;
  double         *da, *db;
  dp_dsss_capture (code, 1023, SLOW_SPC, fs, tsym, 200.0, 50.0, 60, 1000, 11,
                   &a, &na, &da);
  dp_dsss_capture (code, 1023, SLOW_SPC, fs, tsym, -300.0, 120.0, 60, 200, 12,
                   &b, &nb, &db);
  const size_t n = na < nb ? na : nb;
  for (size_t i = 0; i < n; i++)
    a[i] = (which & 1 ? a[i] : 0) + (which & 2 ? (float)M_SQRT1_2 * b[i] : 0);
  free (b);
  free (da);
  free (db);
  *n_out = n;
  return a;
}

/* Feed `rx` until it leaves searching (or the stream ends); return the
 * samples fed. */
static size_t
_until_seeded (dp_async_dsss_receiver_state_t *rx, const float _Complex *x,
               size_t n)
{
  float _Complex out[4096];
  size_t pos = 0;
  while (pos < n && rx->state == ASYNC_DSSS_RX_SEARCHING)
    {
      const size_t take = n - pos < 3001 ? n - pos : 3001;
      (void)dp_async_dsss_receiver_steps (rx, x + pos, take, out, 4096);
      pos += take;
    }
  return pos;
}

static double
_chip_dist (double a, double b, double l)
{
  double d = fabs (a - b);
  return d > l / 2 ? l - d : d;
}

/* The controller's levers (design async-dsss-parallel.md §3): two
 * searchers on one stream at ranks 0 and 1 take the two signals, the
 * stronger at rank 0, in the SAME dwell; an excluded code phase is never
 * taken, so rank 0 with A excluded takes B; and a dwell with no peak at the
 * rank seeds nothing. Each signal's own phase comes from a rank-0
 * receiver fed it alone. Sabotage: ignore the rank -> both take A; ignore
 * the exclusions -> the excluded receiver takes A; drop the duplicate rule
 * -> rank 1 takes A's neighbouring cell. */
static int
_test_ranks_and_exclusions_split_two_signals (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  size_t          n, na, nb;
  float _Complex *both = _two_emitters (code, 3, &n);
  float _Complex *xa   = _two_emitters (code, 1, &na);
  float _Complex *xb   = _two_emitters (code, 2, &nb);

  dp_async_dsss_receiver_state_t *ra = _slow_rx (code, 47.0, 500.0, 0, 0);
  dp_async_dsss_receiver_state_t *rb = _slow_rx (code, 47.0, 500.0, 0, 0);
  dp_async_dsss_receiver_state_t *r0 = _slow_rx (code, 47.0, 500.0, 0, 0);
  dp_async_dsss_receiver_state_t *r1 = _slow_rx (code, 47.0, 500.0, 0, 0);
  dp_async_dsss_receiver_state_t *rx = _slow_rx (code, 47.0, 500.0, 0, 0);
  DP_REQUIRE (ra && rb && r0 && r1 && rx);
  /* From two epochs in, where both emitters are on: every dwell then sees
     both whole, so their strengths alone order them. */
  const size_t on = 2 * 2046;
  (void)_until_seeded (ra, xa + on, na - on);
  (void)_until_seeded (rb, xb + on, nb - on);
  DP_REQUIRE (ra->state != ASYNC_DSSS_RX_SEARCHING
              && rb->state != ASYNC_DSSS_RX_SEARCHING);
  const double pa = ra->seed_chip_phase, pb = rb->seed_chip_phase;
  DP_REQUIRE (_chip_dist (pa, pb, 1023.0) > 100.0);

  DP_CHECK (dp_async_dsss_receiver_set_rank (r1, 1) == DP_OK);
  const size_t at0 = _until_seeded (r0, both + on, n - on);
  const size_t at1 = _until_seeded (r1, both + on, n - on);
  DP_CHECK (r0->state != ASYNC_DSSS_RX_SEARCHING
            && r1->state != ASYNC_DSSS_RX_SEARCHING);
  DP_CHECK (at0 == at1); /* one dwell: the same call seeded both */
  DP_CHECK (_chip_dist (r0->seed_chip_phase, pa, 1023.0) <= 1.0);
  DP_CHECK (_chip_dist (r1->seed_chip_phase, pb, 1023.0) <= 1.0);

  /* A excluded, rank 0: takes B. The exclusion is given a chip off A's
     phase and across the wrap (+1023), so the zone, not equality, and the
     fold both carry it. */
  const double ex[1] = { pa + 0.9 + 1023.0 };
  DP_CHECK (dp_async_dsss_receiver_set_exclusions (rx, ex, 1) == DP_OK);
  (void)_until_seeded (rx, both + on, n - on);
  DP_CHECK (rx->state != ASYNC_DSSS_RX_SEARCHING);
  DP_CHECK (_chip_dist (rx->seed_chip_phase, pb, 1023.0) <= 1.0);

  dp_async_dsss_receiver_destroy (ra);
  dp_async_dsss_receiver_destroy (rb);
  dp_async_dsss_receiver_destroy (r0);
  dp_async_dsss_receiver_destroy (r1);
  dp_async_dsss_receiver_destroy (rx);

  /* A alone: excluded, it is never taken at any Doppler, dwell after
     dwell; and at rank 1 there is no second signal to take. Either way the
     search goes on, and the engine is fed every sample exactly once -- its
     position is what was fed (the rest resumes at dp_acq_consumed(), not
     at the hit: resuming at the hit re-fed the carry). */
  for (int k = 0; k < 2; k++)
    {
      dp_async_dsss_receiver_state_t *r = _slow_rx (code, 47.0, 500.0, 0, 0);
      DP_REQUIRE (r != NULL);
      if (k == 0)
        DP_CHECK (dp_async_dsss_receiver_set_exclusions (r, &pa, 1) == DP_OK);
      else
        DP_CHECK (dp_async_dsss_receiver_set_rank (r, 1) == DP_OK);
      const size_t fed = _until_seeded (r, xa, na);
      DP_CHECK_MSG (r->state == ASYNC_DSSS_RX_SEARCHING,
                    k ? "rank 1 of one signal" : "excluded signal taken");
      DP_CHECK_EQ_U64 (dp_acq_position (r->acq), (uint64_t)fed);
      DP_CHECK_EQ_U64 (dp_async_dsss_receiver_status (r).state_samples,
                       (uint64_t)fed);
      dp_async_dsss_receiver_destroy (r);
    }
  free (both);
  free (xa);
  free (xb);
  return 0;
}

/* The sidelobe rule's near-far range does not narrow with the number of
 * signals on the air (doppler#2554). Five strong emitters, each 10 dB
 * above a weak one W and all excluded, on one Gold code at distinct code
 * phases and carriers, each on a Doppler row of its own (modulo the epoch
 * rate): a rank-0 search must take W. 10 dB is well inside the -16.95 dB
 * sidelobe level, so W is a signal against any one of them; a bound summed
 * over all five read W as their sidelobes and never took it. Run on a
 * one-epoch search over data and on the specification frame's deep block (D =
 * 90, 450 code-only symbols in 500). Each emitter's phase comes from a rank-0
 * receiver fed it alone. Sabotage: sum every stronger emitter in
 * adr_sidelobe_bound() -> red at D = 1; sum them regardless of row at
 * D > 1 -> red at D = 90. */
static int
_near_far_many (int deep)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const double fs = SLOW_CR * SLOW_SPC, tsym = fs / SLOW_RS;
  enum
  {
    NS = 5
  };
  const double dop[NS + 1]
      = { 700.0, 200.0, -1200.0, 2300.0, -2900.0, 4100.0 };
  const size_t    pre[NS + 1] = { 900, 0, 300, 600, 1300, 1700 };
  const float     gain        = 3.16227766f; /* +10 dB */
  float _Complex *x[NS + 1];
  size_t          len[NS + 1], n = (size_t)-1;
  double         *data;
  /* W carries the noise at 50 dB-Hz; the strong ones are rendered clean. */
  for (size_t k = 0; k <= NS; k++)
    {
      dp_dsss_windowed_capture (code, 1023, SLOW_SPC, fs, tsym, dop[k],
                                k ? 120.0 : 50.0, deep ? 600 : 60, pre[k],
                                21 + (uint32_t)k, deep ? 450 : 0,
                                deep ? 500 : 0, &x[k], &len[k], &data);
      free (data);
      if (len[k] < n)
        n = len[k];
    }
  float _Complex *all   = malloc (n * sizeof *all);
  float _Complex *alone = malloc (n * sizeof *alone);
  DP_REQUIRE (all && alone);
  double phase[NS + 1];
  for (size_t i = 0; i < n; i++)
    {
      all[i] = x[0][i];
      for (size_t k = 1; k <= NS; k++)
        all[i] += gain * x[k][i];
    }
  /* From two epochs in, where every emitter is on. */
  const size_t on = 2 * 2046 + 1700;
  for (size_t k = 0; k <= NS; k++)
    {
      for (size_t i = 0; i < n; i++)
        alone[i] = (k ? gain : 1.0f) * x[k][i];
      dp_async_dsss_receiver_state_t *r = _slow_rx (
          code, 47.0, 5000.0, deep ? 450.0 / SLOW_RS : 0, deep ? 500.0 : 0);
      DP_REQUIRE (r != NULL);
      (void)_until_seeded (r, alone + on, n - on);
      DP_REQUIRE (r->state != ASYNC_DSSS_RX_SEARCHING);
      phase[k] = r->seed_chip_phase;
      dp_async_dsss_receiver_destroy (r);
    }
  for (size_t j = 0; j <= NS; j++)
    for (size_t k = j + 1; k <= NS; k++)
      DP_REQUIRE (_chip_dist (phase[j], phase[k], 1023.0) > 50.0);
  dp_async_dsss_receiver_state_t *rx = _slow_rx (
      code, 47.0, 5000.0, deep ? 450.0 / SLOW_RS : 0, deep ? 500.0 : 0);
  DP_REQUIRE (rx != NULL);
  DP_REQUIRE ((dp_async_dsss_receiver_get_coherent_bins (rx) > 1) == deep);
  DP_CHECK (dp_async_dsss_receiver_set_exclusions (rx, phase + 1, NS)
            == DP_OK);
  (void)_until_seeded (rx, all + on, n - on);
  DP_CHECK_MSG (rx->state != ASYNC_DSSS_RX_SEARCHING,
                deep ? "D=90: W was not taken beside five stronger signals"
                     : "D=1: W was not taken beside five stronger signals");
  DP_CHECK (_chip_dist (rx->seed_chip_phase, phase[0], 1023.0) <= 1.0);
  dp_async_dsss_receiver_destroy (rx);
  for (size_t k = 0; k <= NS; k++)
    free (x[k]);
  free (all);
  free (alone);
  return 0;
}

static int
_test_near_far_range_holds_with_many_signals (void)
{
  DP_CHECK (_near_far_many (0) == 0);
  DP_CHECK (_near_far_many (1) == 0);
  return 0;
}

/* Two equal emitters on ONE carrier stack their sidelobes on one cell
 * (doppler#2556, review 5630513120). On a deep block over a code-only
 * window an emitter's Doppler-offset sidelobes are fixed cells on its own
 * Doppler row, so two emitters on the same row can put theirs on one cell
 * and together pass a bound that only the stronger one sets. Hand-built
 * geometries from the reviews, D = 90 on the specification frame, B clean at
 * A's amplitude: A at 1500 Hz with B on its carrier 674 chips after it, or at
 * -4500 Hz (the same row modulo the epoch rate) 426 chips after it, two
 * draws each; and A at -994 Hz with B at -977 Hz, 367 chips after it,
 * draw 9 -- 17 Hz apart, the two quantize to ADJACENT rows (review
 * 5630995725). Neither a rank-2 search nor one with A and B excluded may
 * leave searching. Sabotage: bound by the strongest alone at D > 1 -> red;
 * reach half a row instead of a row and a half -> red on the third. */
static int
_test_two_emitters_sidelobes_stack (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const double fs = SLOW_CR * SLOW_SPC, tsym = fs / SLOW_RS;
  const struct
  {
    double   lag, fa, fb;
    uint32_t sd;
  } cases[] = { { 674.0, 1500.0, 1500.0, 1 },
                { 674.0, 1500.0, 1500.0, 2 },
                { 426.0, 1500.0, -4500.0, 1 },
                { 426.0, 1500.0, -4500.0, 2 },
                { 367.0, -994.0, -977.0, 9 } };
  int taken = 0;
  for (size_t g = 0; g < sizeof cases / sizeof cases[0]; g++)
    {
      const uint32_t sd = cases[g].sd;
      {
        const size_t dB = (size_t)llround ((1023.0 - cases[g].lag) * SLOW_SPC);
        const size_t p0 = 300 + 37 * sd;
        float _Complex *a, *b;
        size_t          na, nb;
        double         *da, *db;
        dp_dsss_windowed_capture (code, 1023, SLOW_SPC, fs, tsym, cases[g].fa,
                                  80.0, 600, p0, sd, 450, 500, &a, &na, &da);
        dp_dsss_windowed_capture (code, 1023, SLOW_SPC, fs, tsym, cases[g].fb,
                                  300.0, 600, p0 + dB, 100 + sd, 450, 500, &b,
                                  &nb, &db);
        const size_t    n   = na < nb ? na : nb;
        float _Complex *all = malloc (n * sizeof *all);
        DP_REQUIRE (all != NULL);
        for (size_t i = 0; i < n; i++)
          all[i] = a[i] + b[i];
        const size_t    on = p0 + dB + 2 * 2046;
        double          ph[2];
        float _Complex *one[2] = { a, b };
        for (size_t k = 0; k < 2; k++)
          {
            dp_async_dsss_receiver_state_t *r
                = _slow_rx (code, 47.0, 5000.0, 450.0 / SLOW_RS, 500.0);
            DP_REQUIRE (r != NULL);
            (void)_until_seeded (r, one[k] + on, n - on);
            DP_REQUIRE (r->state != ASYNC_DSSS_RX_SEARCHING);
            ph[k] = r->seed_chip_phase;
            dp_async_dsss_receiver_destroy (r);
          }
        dp_async_dsss_receiver_state_t *r2
            = _slow_rx (code, 47.0, 5000.0, 450.0 / SLOW_RS, 500.0);
        dp_async_dsss_receiver_state_t *rx
            = _slow_rx (code, 47.0, 5000.0, 450.0 / SLOW_RS, 500.0);
        DP_REQUIRE (r2 && rx);
        DP_REQUIRE (dp_async_dsss_receiver_get_coherent_bins (r2) == 90);
        DP_CHECK (dp_async_dsss_receiver_set_rank (r2, 2) == DP_OK);
        DP_CHECK (dp_async_dsss_receiver_set_exclusions (rx, ph, 2) == DP_OK);
        (void)_until_seeded (r2, all + on, n - on);
        (void)_until_seeded (rx, all + on, n - on);
        if (r2->state != ASYNC_DSSS_RX_SEARCHING
            || rx->state != ASYNC_DSSS_RX_SEARCHING)
          {
            DP_TEST_ERR ("  lag %.0f fa %+.0f fb %+.0f draw %u: rank 2 %d, "
                         "excluded %d\n",
                         cases[g].lag, cases[g].fa, cases[g].fb, sd, r2->state,
                         rx->state);
            taken++;
          }
        dp_async_dsss_receiver_destroy (r2);
        dp_async_dsss_receiver_destroy (rx);
        free (a);
        free (b);
        free (da);
        free (db);
        free (all);
      }
    }
  DP_CHECK_MSG (taken == 0, "two emitters' stacked sidelobes were taken");
  return 0;
}

/* One signal holds ONE place in the ranking. A strong emitter between two
 * Doppler tiles (1.5 kHz, tiles 3 kHz apart over +/-5 kHz) peaks in both
 * at its own code phase; the engine's own zone is per Doppler bin, so the
 * list carries it twice, and only the receiver's duplicate rule makes the
 * second copy no second signal. Rank 0 takes it; rank 1 finds nothing.
 * Sabotage: drop the duplicate rule -> rank 1 seeds on the copy. */
static int
_test_one_signal_holds_one_rank (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const double    fs = SLOW_CR * SLOW_SPC;
  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (code, 1023, SLOW_SPC, fs, fs / SLOW_RS, 1500.0, 70.0, 60, 0,
                   5, &x, &n, &data);
  for (size_t r = 0; r < 2; r++)
    {
      dp_async_dsss_receiver_state_t *rx
          = _slow_rx (code, 60.0, 5000.0, 0.0, 0.0);
      DP_REQUIRE (rx != NULL && rx->acq->window_bins > 1);
      DP_CHECK (dp_async_dsss_receiver_set_rank (rx, r) == DP_OK);
      (void)_until_seeded (rx, x, n);
      DP_CHECK_MSG ((rx->state != ASYNC_DSSS_RX_SEARCHING) == (r == 0),
                    r ? "rank 1 seeded on the one signal's copy"
                      : "rank 0 missed the signal");
      dp_async_dsss_receiver_destroy (rx);
    }
  free (x);
  free (data);
  return 0;
}

/* A ranked or excluded search takes no sidelobe (#2525). One emitter's
 * dwell list also holds its code's Doppler-offset sidelobes, hundreds of
 * chips from its phase and up to dp_acq_dsl_db() below it (design
 * acq-surface.md §8), and noise rides on them. A search that skips the
 * strongest peak must not take one as a second signal: on a stream holding
 * A alone, neither a rank-1 receiver nor a rank-0 receiver with A excluded
 * may leave searching. Draws: data throughout, the specification search (+-5
 * kHz, five tiles), two C/N0 and three Dopplers, one near a tile centre and
 * one on a tile edge, three noise seeds and starts each. Without the sidelobe
 * rule both seed on a sidelobe in every draw. Sabotage: drop the rule
 * (adr_pick()'s dsl term) -> red; leave the excluded peak out of the
 * strongest-statistic bound -> the excluded search seeds. */
static int
_test_ranked_search_takes_no_sidelobe (void)
{
  uint8_t code[1023];
  dp_dsss_gold_1023 (code);
  const double fs      = SLOW_CR * SLOW_SPC;
  const double cn0s[2] = { 60.0, 70.0 }, dops[3] = { 200.0, 1500.0, -2600.0 };
  int          taken = 0;
  for (size_t c = 0; c < 2; c++)
    for (size_t f = 0; f < 3; f++)
      for (uint32_t seed = 1; seed <= 3; seed++)
        {
          float _Complex *x;
          size_t          n;
          double         *data;
          dp_dsss_capture (code, 1023, SLOW_SPC, fs, fs / SLOW_RS, dops[f],
                           cn0s[c], 60, 300 * seed, seed, &x, &n, &data);
          dp_async_dsss_receiver_state_t *r0
              = _slow_rx (code, 47.0, 5000.0, 0.0, 0.0);
          dp_async_dsss_receiver_state_t *r1
              = _slow_rx (code, 47.0, 5000.0, 0.0, 0.0);
          dp_async_dsss_receiver_state_t *rx
              = _slow_rx (code, 47.0, 5000.0, 0.0, 0.0);
          DP_REQUIRE (r0 && r1 && rx);
          /* The sidelobe is the searcher's to compute on first read; the
             receiver reads it at create, so no push() pays for the sweep.
             Sabotage: drop that read -> red. */
          DP_CHECK (r0->acq->dsl_ready);
          (void)_until_seeded (r0, x, n);
          DP_REQUIRE (r0->state != ASYNC_DSSS_RX_SEARCHING);
          const double pa = r0->seed_chip_phase;
          DP_CHECK (dp_async_dsss_receiver_set_rank (r1, 1) == DP_OK);
          DP_CHECK (dp_async_dsss_receiver_set_exclusions (rx, &pa, 1)
                    == DP_OK);
          (void)_until_seeded (r1, x, n);
          (void)_until_seeded (rx, x, n);
          if (r1->state != ASYNC_DSSS_RX_SEARCHING
              || rx->state != ASYNC_DSSS_RX_SEARCHING)
            {
              DP_TEST_ERR ("  %.0f dB-Hz %+.0f Hz seed %u: A at %.1f, rank 1 "
                           "at %.1f (%d), excluded at %.1f (%d)\n",
                           cn0s[c], dops[f], seed, pa, r1->seed_chip_phase,
                           r1->state, rx->seed_chip_phase, rx->state);
              taken++;
            }
          dp_async_dsss_receiver_destroy (r0);
          dp_async_dsss_receiver_destroy (r1);
          dp_async_dsss_receiver_destroy (rx);
          free (x);
          free (data);
        }
  /* The deep block's thinnest margin (review 5606641242): D = 90 on the
     the specification frame (450 code-only symbols of 500), a strong emitter
     on a tile edge, where its own peak has lost the most and its sidelobe has
     not. */
  {
    float _Complex *x;
    size_t          n;
    double         *data;
    dp_dsss_windowed_capture (code, 1023, SLOW_SPC, fs, fs / SLOW_RS, 1500.0,
                              80.0, 600, 300, 7, 450, 500, &x, &n, &data);
    dp_async_dsss_receiver_state_t *r0
        = _slow_rx (code, 47.0, 5000.0, 450.0 / SLOW_RS, 500.0);
    dp_async_dsss_receiver_state_t *r1
        = _slow_rx (code, 47.0, 5000.0, 450.0 / SLOW_RS, 500.0);
    dp_async_dsss_receiver_state_t *rx
        = _slow_rx (code, 47.0, 5000.0, 450.0 / SLOW_RS, 500.0);
    DP_REQUIRE (r0 && r1 && rx);
    DP_REQUIRE (dp_async_dsss_receiver_get_coherent_bins (r0) == 90);
    (void)_until_seeded (r0, x, n);
    DP_REQUIRE (r0->state != ASYNC_DSSS_RX_SEARCHING);
    const double pa = r0->seed_chip_phase;
    DP_CHECK (dp_async_dsss_receiver_set_rank (r1, 1) == DP_OK);
    DP_CHECK (dp_async_dsss_receiver_set_exclusions (rx, &pa, 1) == DP_OK);
    (void)_until_seeded (r1, x, n);
    (void)_until_seeded (rx, x, n);
    DP_CHECK_MSG (r1->state == ASYNC_DSSS_RX_SEARCHING,
                  "D=90: rank 1 took a sidelobe");
    DP_CHECK_MSG (rx->state == ASYNC_DSSS_RX_SEARCHING,
                  "D=90: the excluded search took a sidelobe");
    dp_async_dsss_receiver_destroy (r0);
    dp_async_dsss_receiver_destroy (r1);
    dp_async_dsss_receiver_destroy (rx);
    free (x);
    free (data);
  }
  DP_CHECK_MSG (taken == 0, "a ranked or excluded search took a sidelobe");
  return 0;
}

/* While refining, status().chip_phase names the signal held: the seed's
 * phase advanced by the clock dilation since, the phase the hand-over
 * starts the live chain from -- not the unbuilt live Dll's 0 (#2525). A
 * controller excludes this phase; at 0 the next searcher would take the
 * refining receiver's signal. The carrier is set so the advance is not
 * zero. Sabotage: read the live Dll while refining -> 0. */
static int
_test_refining_reports_the_seed_phase (void)
{
  const double carrier = 2.2e6, dop = 2000.0, seed_phase = 3.25;
  dp_async_dsss_receiver_state_t *rx = dp_async_dsss_receiver_create (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, 60.0, 1e-2, 0.9, 5000.0, 4, 8, 0, 0.5,
      4, 14.0, 32, 8, false, 100000, carrier, 0.0, false, 0.0, 0.0);
  DP_REQUIRE (rx != NULL);
  DP_CHECK (dp_async_dsss_receiver_seed (rx, seed_phase, dop, 50.0) == DP_OK);
  float _Complex *z = calloc (2000, sizeof *z);
  float _Complex out[4096];
  (void)dp_async_dsss_receiver_steps (rx, z, 2000, out, 4096);
  DP_REQUIRE (rx->state == ASYNC_DSSS_RX_REFINING);
  const double want = fmod (seed_phase + dop / carrier * (2000.0 / 4.0), 7.0);
  const async_dsss_receiver_status_t st = dp_async_dsss_receiver_status (rx);
  DP_CHECK (fabs (st.chip_phase - want) < 1e-9);
  DP_CHECK (st.chip_phase > seed_phase); /* the dilation moved it */
  DP_CHECK (dp_async_dsss_receiver_get_chip_phase (rx) == st.chip_phase);
  free (z);
  dp_async_dsss_receiver_destroy (rx);
  return 0;
}

/* The two setters' domains and their blob (#2022): rank below the list,
 * phases finite and at most 64, folded into [0, code_len); a refusal
 * leaves the value as it was; both travel, and a blob only set_exclusions()
 * could not leave is refused. Sabotage: drop rank or the list from the
 * blob -> the restored receiver differs; skip the fold -> 9 reads 9. */
static int
_test_rank_and_exclusions_domain_and_blob (void)
{
  dp_async_dsss_receiver_state_t *a = _searching_rx (60.0, 35714.29, 0, false);
  dp_async_dsss_receiver_state_t *b = _searching_rx (60.0, 35714.29, 0, false);
  DP_REQUIRE (a && b);
  DP_CHECK (dp_async_dsss_receiver_set_rank (a, ASYNC_DSSS_RX_MAX_PEAKS)
            == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_get_rank (a) == 0);
  DP_CHECK (dp_async_dsss_receiver_set_rank (a, 3) == DP_OK);

  const double ok[3] = { 9.0, -0.5, 2.25 };
  DP_CHECK (dp_async_dsss_receiver_set_exclusions (a, ok, 3) == DP_OK);
  DP_CHECK (a->excl[0] == 2.0 && a->excl[1] == 6.5 && a->excl[2] == 2.25);
  const double bad[2] = { 1.0, NAN };
  DP_CHECK (dp_async_dsss_receiver_set_exclusions (a, bad, 2)
            == DP_ERR_INVALID);
  double many[ASYNC_DSSS_RX_MAX_EXCLUSIONS + 1] = { 0 };
  DP_CHECK (dp_async_dsss_receiver_set_exclusions (
                a, many, ASYNC_DSSS_RX_MAX_EXCLUSIONS + 1)
            == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_set_exclusions (a, NULL, 1)
            == DP_ERR_INVALID);
  DP_CHECK (dp_async_dsss_receiver_get_n_exclusions (a) == 3
            && a->excl[0] == 2.0); /* refusals left it */

  const size_t   nb   = dp_async_dsss_receiver_state_bytes (a);
  unsigned char *blob = malloc (nb);
  dp_async_dsss_receiver_get_state (a, blob);
  DP_REQUIRE (dp_async_dsss_receiver_set_state (b, blob) == DP_OK);
  DP_CHECK (dp_async_dsss_receiver_get_rank (b) == 3);
  DP_CHECK (dp_async_dsss_receiver_get_n_exclusions (b) == 3);
  DP_CHECK (memcmp (a->excl, b->excl, sizeof a->excl) == 0);

  async_dsss_receiver_extra_t *e
      = (async_dsss_receiver_extra_t *)(blob + sizeof (dp_state_hdr_t));
  const async_dsss_receiver_extra_t keep = *e;
  e->rank                                = ASYNC_DSSS_RX_MAX_PEAKS;
  DP_CHECK (dp_async_dsss_receiver_set_state (b, blob) == DP_ERR_INVALID);
  *e         = keep;
  e->excl[1] = 7.0; /* not folded */
  DP_CHECK (dp_async_dsss_receiver_set_state (b, blob) == DP_ERR_INVALID);
  *e         = keep;
  e->excl[5] = 1.0; /* past n_excl: not zeroed */
  DP_CHECK (dp_async_dsss_receiver_set_state (b, blob) == DP_ERR_INVALID);
  *e        = keep;
  e->n_excl = ASYNC_DSSS_RX_MAX_EXCLUSIONS + 1;
  DP_CHECK (dp_async_dsss_receiver_set_state (b, blob) == DP_ERR_INVALID);
  free (blob);
  dp_async_dsss_receiver_destroy (a);
  dp_async_dsss_receiver_destroy (b);
  return 0;
}

int
main (void)
{
  (void)_test_arg_validation ();
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
  (void)_test_seeded_blob_round_trip ();
  (void)_test_carry_tail_is_zeros ();
  (void)_test_status_record ();
  DP_CHECK (_test_tlm_lifecycle_probes () == 0);
  DP_CHECK (_test_tlm_searching_and_refused () == 0);
  DP_CHECK (_test_tlm_names_documented () == 0);
  DP_CHECK (_test_slow_time_depth_from_physical_inputs () == 0);
  DP_CHECK (_test_threads_change_nothing_but_cost () == 0);
  DP_CHECK (_test_dwells_start_on_the_block_grid () == 0);
  DP_CHECK (_test_depth_finds_what_one_epoch_misses () == 0);
  DP_CHECK (_test_ranks_and_exclusions_split_two_signals () == 0);
  DP_CHECK (_test_near_far_range_holds_with_many_signals () == 0);
  DP_CHECK (_test_two_emitters_sidelobes_stack () == 0);
  DP_CHECK (_test_rank_and_exclusions_domain_and_blob () == 0);
  DP_CHECK (_test_one_signal_holds_one_rank () == 0);
  DP_CHECK (_test_ranked_search_takes_no_sidelobe () == 0);
  DP_CHECK (_test_refining_reports_the_seed_phase () == 0);

  DP_TEST_END ("test_async_dsss_receiver_core");
}
