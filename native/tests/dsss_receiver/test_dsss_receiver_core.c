/*
 * test_dsss_receiver_core.c — composed DsssReceiver C-level tests.
 *
 * Covers: argument validation, a real Acquisition -> Dll -> RateConverter ->
 * MpskReceiver run over a synthetic continuous DSSS-BPSK signal (searching
 * -> tracking transition, correct decode via a lag search over known data --
 * the physics of *why* this composition works is already covered by
 * Acquisition/Dll/MpskReceiver's own dedicated tests and this repo's
 * async-DSSS-receiver gallery story; this test is about the NEW object
 * wiring those four correctly, not re-proving the underlying DSP), the
 * state-serialization round trip in both the searching and tracking phases,
 * and an envelope-reject check.
 */
#include "doppler/costas/costas_core.h"
#include "doppler/dll/dll_core.h"
#include "doppler/dp_complex.h"
#include "doppler/dsss_receiver/dsss_receiver_core.h"
#include "doppler/dsss_receiver/sym_hold_private.h"
#include "dp_dsss_test.h"
#include "dp_rng_test.h"
#include "dp_state_test.h"
#include "dp_sym_test.h"
#include "dp_test.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A length-7 maximal-length sequence (one period) -- same fixture
 * test_correlator_search.c uses for its own fast, real (not mocked) unit
 * tests. */
static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

static int
_test_arg_validation (void)
{
  DP_CHECK (dp_dsss_receiver_create (NULL, 0, 1e6, 1e3, (1e6) * (2), 2, 55.0,
                                     1e-3, 0.9, 100.0, 4, 8, 0)
            == NULL);
  /* Refused by create itself, where each reached a dp_xnn build and
     aborted the process (doppler#2103): the searcher's own domain (a pfa of
     2), and a NaN rate, whose ratio sizes the chain's resampler. The
     control is the same receiver at good values. */
  {
    dp_dsss_receiver_state_t *ok = dp_dsss_receiver_create (
        CODE7, 7, 1e6, 1e3, (1e6) * (2), 2, 55.0, 1e-3, 0.9, 100.0, 4, 8, 0);
    DP_CHECK (ok != NULL);
    dp_dsss_receiver_destroy (ok);
  }
  DP_CHECK (dp_dsss_receiver_create (CODE7, 7, 1e6, 1e3, (1e6) * (2), 2, 55.0,
                                     2.0, 0.9, 100.0, 4, 8, 0)
            == NULL); /* pfa */
  DP_CHECK (dp_dsss_receiver_create (CODE7, 7, 1e6, NAN, (1e6) * (2), 2, 55.0,
                                     1e-3, 0.9, 100.0, 4, 8, 0)
            == NULL); /* symbol_rate */
  /* A symbol rate at either extreme: 5e-324 underflowed the chain's
     resampling ratio to 0, which the resampler refuses under dp_xnn, and
     one past the chip rate sized its scratch at ~4e12 samples. */
  DP_CHECK (dp_dsss_receiver_create (CODE7, 7, 1e6, 5e-324, (1e6) * (2), 2,
                                     55.0, 1e-3, 0.9, 100.0, 4, 8, 0)
            == NULL);
  DP_CHECK (dp_dsss_receiver_create (CODE7, 7, 1e6, 1e15, (1e6) * (2), 2, 55.0,
                                     1e-3, 0.9, 100.0, 4, 8, 0)
            == NULL);
  /* A 1-chip code: refused, where the Dll built under dp_xnn used to abort
     the process once it refused the code (doppler#2103). */
  DP_CHECK (dp_dsss_receiver_create (CODE7, 1, 1e6, 1e3, (1e6) * (2), 2, 55.0,
                                     1e-3, 0.9, 100.0, 4, 8, 0)
            == NULL);
  DP_CHECK (dp_dsss_receiver_create (CODE7, 7, 0.0, 1e3, (0.0) * (2), 2, 55.0,
                                     1e-3, 0.9, 100.0, 4, 8, 0)
            == NULL); /* chip_rate <= 0 */
  DP_CHECK (dp_dsss_receiver_create (CODE7, 7, 1e6, 1e3, (1e6) * (2), 3, 55.0,
                                     1e-3, 0.9, 100.0, 4, 8, 0)
            == NULL); /* m not in {2,4,8} */
  DP_CHECK (dp_dsss_receiver_create (CODE7, 7, 1e6, 1e3, (1e6) * (2), 2, 55.0,
                                     1e-3, 0.9, 100.0, 0, 8, 0)
            == NULL); /* segments < 1 */
  /* The rate: a whole number of samples a chip, to a part in 1e9. 2.5 a
     chip is refused (this receiver's code periods are whole samples), and
     so is less than one; two a chip off by a part in 1e12 is two.
     Sabotage, each red: drop the whole-number test (2.5 builds), or
     round instead of refusing below one. */
  DP_CHECK (dp_dsss_receiver_create (CODE7, 7, 1e6, 1e3, 2.5e6, 2, 55.0, 1e-3,
                                     0.9, 100.0, 4, 8, 0)
            == NULL);
  DP_CHECK (dp_dsss_receiver_create (CODE7, 7, 1e6, 1e3, 0.6e6, 2, 55.0, 1e-3,
                                     0.9, 100.0, 4, 8, 0)
            == NULL);
  {
    dp_dsss_receiver_state_t *two
        = dp_dsss_receiver_create (CODE7, 7, 1e6, 1e3, 2e6 * (1.0 + 1e-12), 2,
                                   55.0, 1e-3, 0.9, 100.0, 4, 8, 0);
    DP_CHECK (two != NULL && two->spc == 2);
    dp_dsss_receiver_destroy (two);
  }

  dp_dsss_receiver_state_t *rx
      = dp_dsss_receiver_create (CODE7, 7, 1.0e6, 35714.29, (1.0e6) * (4), 2,
                                 70.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  DP_CHECK (rx != NULL);
  if (rx)
    {
      DP_CHECK (dp_dsss_receiver_get_tracking (rx) == 0);
      DP_CHECK (dp_dsss_receiver_get_segments (rx) == 4);
      DP_CHECK (dp_dsss_receiver_get_sps (rx) == 8);
      /* `n` is MpskReceiver's m_out: terminal outputs per symbol since the
         cascade rebuild, not the retired NDA arm's dumps per symbol. So it
         derives to the coherent-bound default, not the old "largest divisor
         of sps in {4,2,1}" (which gave 4 and did not decode). */
      DP_CHECK (dp_dsss_receiver_get_n (rx) == MPSK_RX_M_OUT_DEFAULT);
      DP_CHECK (dp_dsss_receiver_get_chip_phase (rx) == 0.0);
      DP_CHECK (dp_dsss_receiver_get_code_rate (rx) == 1.0);
      /* settle_time is the demodulator's, at the bandwidths this receiver
         builds it with: the header's 778.5 symbols. Sabotaged by returning
         half of it. */
      DP_CHECK (dp_dsss_receiver_get_settle_time (rx)
                == dp_loop_filter_settle_time (DSSS_RX_DEMOD_BN_TIMING,
                                               DSSS_RX_DEMOD_ZETA,
                                               DP_LOOP_FILTER_SETTLE_EPS)
                       + dp_loop_filter_settle_time (
                           DSSS_RX_DEMOD_BN_CARRIER, DSSS_RX_DEMOD_ZETA,
                           DP_LOOP_FILTER_SETTLE_EPS));
      DP_CHECK_NEAR (dp_dsss_receiver_get_settle_time (rx), 778.47, 0.01);
      dp_dsss_receiver_destroy (rx);
    }
  return 0;
}

/* Stream `x` through `rx` in fixed-size chunks, collecting every emitted
 * symbol; return the symbol count and fill `*syms_out` (caller frees). */
static size_t
_stream (dp_dsss_receiver_state_t *rx, const float _Complex *x, size_t n,
         size_t chunk, float _Complex **syms_out)
{
  float _Complex *syms = malloc (n * sizeof *syms); /* generous upper bound */
  size_t          n_syms = 0;
  for (size_t pos = 0; pos < n; pos += chunk)
    {
      size_t take = (pos + chunk <= n) ? chunk : (n - pos);
      n_syms += dp_dsss_receiver_steps (rx, x + pos, take, syms + n_syms,
                                        n - n_syms);
    }
  *syms_out = syms;
  return n_syms;
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
  const size_t n_sym       = 300;
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 90.0;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 7,
                   &x, &n, &data);

  /* Sizing cn0_dbhz=70.0 (comfortably below the injected cn0=90.0, but not
   * the old 45.0): since the embedded engine is always continuous now (task
   * #67's fix -- never coherently combines, coherent_bins pinned to 1), its
   * per-look code-phase estimate comes from a single sf=7-chip epoch's own
   * correlation, not an old joint-search's multi-epoch coherent combining.
   * At 45.0 the auto-sizer rides the internal 256-look non-coherent ceiling
   * (genuinely under-powered) and the resulting handoff seed is imprecise
   * enough for this unusually SHORT 7-chip code to leave a persistent
   * partial-decorrelation BER floor (~0.25-0.3, not settling-transient --
   * confirmed by direct measurement across the whole decoded run). 70.0
   * keeps n_noncoh well off the ceiling, matching this test's own stated
   * scope (validating the composed object's wiring, not re-proving
   * Acquisition's own precision-vs-sizing trade-offs, already covered by
   * Acquisition's own dedicated tests). */
  dp_dsss_receiver_state_t *rx
      = dp_dsss_receiver_create (CODE7, sf, 1.0e6, sym_rate, (1.0e6) * (spc),
                                 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }

  float _Complex *syms;
  size_t          n_syms = _stream (rx, x, n, te, &syms);

  DP_CHECK (dp_dsss_receiver_get_tracking (rx) == 1);
  DP_CHECK (n_syms > 20);
  DP_CHECK (dp_dsss_receiver_get_cn0_dbhz_est (rx) > 0.0);

  double ber = dp_dsss_best_ber (syms, n_syms, data, n_sym + 4);
  DP_CHECK (ber < 0.05);

  /* ── state-serialization round trip, while tracking ─────────────────── */
  /* Its Costas child grew by its third-order filter, its own version did
     not move, and a blob one child's growth smaller is refused by its size
     (docs/design/costas.md 3.3). */
  DP_STATE_SHRUNK_TEST (
      dp_dsss_receiver, rx,
      (sizeof (dp_state_filter_state_t) + 2 * sizeof (int32_t)));
  size_t cb   = dp_dsss_receiver_state_bytes (rx);
  void  *blob = malloc (cb);
  dp_dsss_receiver_get_state (rx, blob);

  dp_dsss_receiver_state_t *rx2
      = dp_dsss_receiver_create (CODE7, sf, 1.0e6, sym_rate, (1.0e6) * (spc),
                                 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  DP_CHECK (rx2 != NULL);
  if (rx2)
    {
      DP_CHECK (dp_dsss_receiver_set_state (rx2, blob) == DP_OK);
      DP_CHECK (dp_dsss_receiver_get_tracking (rx2) == 1);
      DP_CHECK (fabs (dp_dsss_receiver_get_chip_phase (rx2)
                      - dp_dsss_receiver_get_chip_phase (rx))
                < 1e-9);

      /* a corrupted envelope must be rejected, not reinterpreted. */
      ((char *)blob)[0] ^= (char)0xFF;
      DP_CHECK (dp_dsss_receiver_set_state (rx2, blob) == DP_ERR_INVALID);
      dp_dsss_receiver_destroy (rx2);
    }
  free (blob);

  /* ── state-serialization round trip, while searching ─────────────────── */
  dp_dsss_receiver_state_t *rx3
      = dp_dsss_receiver_create (CODE7, sf, 1.0e6, sym_rate, (1.0e6) * (spc),
                                 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  DP_CHECK (rx3 != NULL);
  if (rx3)
    {
      size_t cb3   = dp_dsss_receiver_state_bytes (rx3);
      void  *blob3 = malloc (cb3);
      dp_dsss_receiver_get_state (rx3, blob3);

      dp_dsss_receiver_state_t *rx4 = dp_dsss_receiver_create (
          CODE7, sf, 1.0e6, sym_rate, (1.0e6) * (spc), 2, 70.0, 1e-2, 0.9,
          500.0, 4, 8, 0);
      DP_CHECK (rx4 != NULL);
      if (rx4)
        {
          DP_CHECK (dp_dsss_receiver_set_state (rx4, blob3) == DP_OK);
          DP_CHECK (dp_dsss_receiver_get_tracking (rx4) == 0);
          dp_dsss_receiver_destroy (rx4);
        }
      free (blob3);
      dp_dsss_receiver_destroy (rx3);
    }

  /* ── reset() returns to searching ─────────────────────────────────────── */
  dp_dsss_receiver_reset (rx);
  DP_CHECK (dp_dsss_receiver_get_tracking (rx) == 0);
  DP_CHECK (dp_dsss_receiver_get_chip_phase (rx) == 0.0);

  free (syms);
  free (x);
  free (data);
  dp_dsss_receiver_destroy (rx);
  return 0;
}

/* ── Isolated proof for the planned pre-despread carrier composition ──────
 * (task #93/#94, ~/.claude/plans/crystalline-knitting-hopper.md): does
 * costas_wipeoff() -> dp_dll_steps(segments=4) -> costas_update(), chunked one
 * code period at a time, actually track a Doppler RATE (not just a static
 * residual) when composed from the raw primitives -- before that exact
 * sequence gets wired into dsss_receiver_core.c's own internals (task #95).
 * Mirrors this session's own Python validation methodology (a ramp, not a
 * static offset; bn_fll=0 vs bn_fll>0 to prove the mechanism is NEEDED, not
 * just present) at the specification-derived bn_carrier=0.01/bn_fll=0.03
 * operating point, using the real segments=4 Dll DsssReceiver already relies
 * on. */
static double
_run_ramp_composition (const uint8_t *code, size_t sf, size_t spc, double fs,
                       double rate_hz_per_s, size_t n_periods, double cn0_dbhz,
                       double bn_fll, uint32_t seed)
{
  size_t          tsamps = sf * spc;
  size_t          n      = n_periods * tsamps;
  float _Complex *x      = calloc (n, sizeof *x);
  /* The shipped noise at the stated C/N0 (dp_dsss_test.h, #689). */
  dp_dsss_add_awgn (x, n, fs, cn0_dbhz, seed);
  for (size_t idx = 0; idx < n; idx++)
    {
      double t               = (double)idx / fs;
      double ph              = 2.0 * M_PI * (0.5 * rate_hz_per_s * t * t);
      float _Complex carrier = (float _Complex) (cos (ph) + I * sin (ph));
      size_t cph             = (idx / spc) % sf;
      float  csgn            = (code[cph] & 1u) ? -1.0f : 1.0f;
      x[idx] += csgn * carrier;
    }

  dp_dll_state_t *dll
      = dp_dll_create (code, sf, spc, 0.0, 0.002, 0.707, 0.5, 4);
  dp_costas_state_t car;
  dp_costas_init (&car, 0.01, 0.707, 0.0, tsamps, bn_fll);

  float _Complex *wiped = malloc (tsamps * sizeof *wiped);
  /* The buffer is the bound for one period's samples (dp_dll_steps_max_out):
     a call of one period can close two periods at segments > 1, and each
     close emits all of its partials. */
  const size_t    cap   = dp_dll_steps_max_out (dll, tsamps);
  float _Complex *parts = malloc (cap * sizeof *parts);
  size_t          most  = 0;
  for (size_t p = 0; p < n_periods; p++)
    {
      const float _Complex *chunk = x + p * tsamps;
      for (size_t i = 0; i < tsamps; i++)
        wiped[i] = costas_wipeoff (&car, chunk[i]);
      const size_t n_out = dp_dll_steps (dll, wiped, tsamps, parts, cap);
      most               = n_out > most ? n_out : most;
      if (n_out == 0)
        continue;
      /* One prompt a period, as the receiver forms it
         (dsss_rx_carrier_update_from_partials(): each partial aligned by
         its own real part's sign, so a data transition inside the period
         does not cancel the sum). */
      float _Complex prompt = 0.0f;
      for (size_t k = 0; k < n_out; k++)
        prompt += parts[k] * (crealf (parts[k]) >= 0.0f ? 1.0f : -1.0f);
      costas_update (&car, prompt);
    }
  DP_CHECK_MSG (most < cap, "no period's call filled the partials' capacity");

  double tracked_hz  = dp_costas_get_norm_freq (&car) * fs;
  double true_at_end = rate_hz_per_s * (double)n_periods * (double)tsamps / fs;

  free (parts);
  free (wiped);
  free (x);
  dp_dll_destroy (dll);
  return fabs (tracked_hz - true_at_end);
}

static int
_test_carrier_dll_composition_ramp (void)
{
  /* sf=7 (CODE7) makes the per-period rate absurdly fast (~143 kHz),
   * trivially tracking a 500 Hz/s ramp with a bare PLL -- not
   * representative of the specification's real geometry. Use a PRBS-1023 code
   * + spc=2 to reproduce the same tsamps=2046/fs=4.092e6 operating point this
   * session's real_costas_fll_3_10db.png investigation already validated
   * the standalone Costas object against. */
  const size_t sf            = 1023;
  const size_t spc           = 2;
  const double fs            = 4.092e6;
  const double rate_hz_per_s = 500.0; /* the specification's worst case */
  const size_t n_periods     = 2700;  /* matches this session's own
                                          Python floor-sweep scale */
  /* A stressed, not comfortable, operating point: noise power 0.18
     against the unit signal, 7.4 dB a sample. It was written as a bare
     sigma of 0.6, which the hand-scaled noise this test used before #689
     delivered as 0.6^2 / 2; stated here as the C/N0 it always ran at,
     10 log10 (fs / 0.18). */
  const double cn0_dbhz = 10.0 * log10 (fs / 0.18);

  uint8_t *code = malloc (sf);
  uint32_t cst  = 7;
  for (size_t i = 0; i < sf; i++)
    code[i] = (uint8_t)(dp_bit (&cst) > 0 ? 0u : 1u);

  double err_off = _run_ramp_composition (code, sf, spc, fs, rate_hz_per_s,
                                          n_periods, cn0_dbhz, 0.0, 11);
  double err_on  = _run_ramp_composition (code, sf, spc, fs, rate_hz_per_s,
                                          n_periods, cn0_dbhz, 0.03, 11);
  free (code);

  /* The mechanism must be NEEDED: a bare PLL (bn_fll=0) should not track
   * this ramp anywhere near as well as bn_fll=0.03 -- proves this isn't
   * passing by accident (e.g. a ramp too gentle to matter). */
  DP_CHECK (err_on < err_off);
  /* And it must actually WORK: bounded absolute error at the specification's
   * own worst-case rate, generous vs. the Python-validated ~63 Hz mean (this
   * is a single-seed C smoke test, not a calibrated statistical sweep --
   * that's what task #97's integration test is for). */
  DP_CHECK (err_on < 300.0);

  return 0;
}

/* Sustained Doppler rate through the full Acquisition -> carrier(bn_fll) ->
 * Dll -> RateConverter -> MpskReceiver chain, at the specification's
 * operating-point ratios (docs/design/async-dsss-receiver.md §1:
 * `chip_rate=3.069e6`, `code_len=1023`, `symbol_rate=2700`, CCSDS Gold-1023
 * with async BPSK), against its 500 Hz/s worst-case ramp: the receiver must
 * stay locked and decode. `chip_rate/(code_len*symbol_rate)` ~= 1.111
 * periods/symbol, NOT an integer, so the data-symbol clock is genuinely
 * ASYNCHRONOUS to the code-epoch clock and symbol boundaries drift across
 * period boundaries over the run (the case `segments>1` exists for). A
 * synchronous `symbol_rate = chip_rate/code_len` would skip the async-data
 * dimension, which is the hard part. cn0 is moderately (not maximally)
 * stressed: the isolated C composition test above covers the mechanism's
 * SNR floor; this test proves the FULL composed receiver survives a
 * sustained RATE and async data together. */
static int
_test_sustained_doppler_rate (void)
{

  const size_t sf        = 1023;
  const size_t spc       = 2;
  const double chip_rate = 3.069e6; /* the specification's Gold-1023 rate */
  const double fs        = chip_rate * (double)spc;
  const double sym_rate  = 2700.0; /* the specification's async BPSK rate --
                                       chip_rate/(sf*sym_rate) ~= 1.111
                                       periods/symbol, deliberately
                                       NOT an integer (see above). */
  const double tsym          = fs / sym_rate;
  const size_t te            = sf * spc;
  const double rate_hz_per_s = 500.0; /* the specification's worst case */
  const size_t n_sym         = 2430;  /* ~2700 code periods' worth of
                                          duration at this async ratio,
                                          matching this session's own
                                          Python floor-sweep/isolated-
                                          composition-test scale */
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 70.5; /* moderately stressed, see above */

  uint8_t *code = malloc (sf);
  uint32_t cst  = 13;
  for (size_t i = 0; i < sf; i++)
    code[i] = (uint8_t)(dp_bit (&cst) > 0 ? 0u : 1u);

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_ramp_capture (code, sf, spc, fs, tsym, rate_hz_per_s, cn0, n_sym,
                        pre_silence, 21, &x, &n, &data);

  dp_dsss_receiver_state_t *rx4 = dp_dsss_receiver_create (
      code, sf, chip_rate, sym_rate, (chip_rate) * (spc), 2, cn0, 1e-2, 0.9,
      500.0, 4, 8, 0);
  dp_dsss_receiver_state_t *rx1 = dp_dsss_receiver_create (
      code, sf, chip_rate, sym_rate, (chip_rate) * (spc), 2, cn0, 1e-2, 0.9,
      500.0, 1, 8, 0);
  DP_CHECK (rx4 != NULL && rx1 != NULL);
  if (!rx4 || !rx1)
    {
      if (rx4)
        dp_dsss_receiver_destroy (rx4);
      if (rx1)
        dp_dsss_receiver_destroy (rx1);
      free (code);
      free (x);
      free (data);
      return 1;
    }

  float _Complex *syms4, *syms1;
  size_t          n_syms4 = _stream (rx4, x, n, te, &syms4);
  size_t          n_syms1 = _stream (rx1, x, n, te, &syms1);

  double ber4 = dp_dsss_best_ber (syms4, n_syms4, data, n_sym + 4);
  double ber1 = dp_dsss_best_ber (syms1, n_syms1, data, n_sym + 4);

  DP_CHECK (dp_dsss_receiver_get_tracking (rx4) == 1);
  /* A receiver that lost lock partway through (the ~117 Hz/s bare-PLL
   * cliff this session's own investigation found -- the specification's 500
   * Hz/s is ~4x past it) would emit far fewer symbols than the run's own
   * length; staying locked through most of it is the actual claim under test.
   */
  DP_CHECK (n_syms4 > (n_sym / 2));
  /* BER is scored by searching an unknown lag and polarity, so it is never
     asserted alone (see dp_sym_test.h): both truth-free validators must agree
     that the constellation is actually clean. Printed as well as asserted --
     a regression that moves these is worth seeing, not just failing on. */
  double evm4 = dp_test_evm_db_hard (syms4, n_syms4);
  double snr4 = dp_test_m2m4_snr_db (syms4, n_syms4);
  printf ("  segments=4: n=%zu ber=%.4f evm=%.1f dB m2m4=%.1f dB\n", n_syms4,
          ber4, evm4, snr4);
  DP_CHECK (ber4 < 0.05);
  DP_CHECK (evm4 < -8.0);
  DP_CHECK (snr4 > 8.0);
  /* segments=1 on the same capture: the one-segment loop at its stated bn
   * must hold lock and decode, held to the segments=4 arm's EVM bound. That
   * bound, -8 dB, is where the constellation stops being error-free at this
   * record length: hard-decision BPSK at an EVM of -8 dB has a BER of
   * Q(sqrt(2 * 10^0.8)) = 1.9e-4, 0.46 errors expected over the record's
   * ~2431 symbols. The error bound follows from it: at most 2 errors, a
   * 1.2% false red at the EVM bound itself (Poisson, mean 0.46). BER is never
   * asserted alone, hence the pair. Both fail on a loop whose integral path
   * is not unit gain (doppler#3234: ~400 errors, EVM -2 dB on this capture).
   *
   * What this does NOT show is that segments > 1 is needed for decoding: BER
   * does not separate the two arms here (EVM does, by 12 dB), and the
   * premise that it must is unshown (doppler#3259). */
  double evm1 = dp_test_evm_db_hard (syms1, n_syms1);
  printf ("  segments=1: n=%zu ber=%.4f evm=%.1f dB\n", n_syms1, ber1, evm1);
  DP_CHECK (n_syms1 > (n_sym / 2));
  DP_CHECK (ber1 * (double)n_syms1 <= 2.0);
  DP_CHECK (evm1 < -8.0);

  free (syms4);
  free (syms1);
  free (x);
  free (data);
  free (code);
  dp_dsss_receiver_destroy (rx4);
  dp_dsss_receiver_destroy (rx1);
  return 0;
}

/* State-serialization round trip that specifically exercises the new
 * carry buffer (task #97): _stream()'s own default chunk size (te,
 * exactly one code period) would always leave car_carry_len at 0 between
 * calls, which would never actually test the variable-length carry path.
 * Deliberately chunk by a size that is NOT a multiple of te so the
 * carrier loop's carry buffer holds a genuine nonzero, mid-period
 * leftover at the snapshot point; a correctly round-tripped carry means
 * a resumed instance stays period-aligned and produces IDENTICAL further
 * output to the un-interrupted original -- a lost/misaligned carry would
 * desync the two almost immediately. */
static int
_test_carry_buffer_state_roundtrip (void)
{

  const size_t sf          = 7;
  const size_t spc         = 4;
  const double fs          = 1.0e6 * (double)spc;
  const double sym_rate    = 35714.29;
  const double tsym        = fs / sym_rate;
  const size_t te          = sf * spc;
  const size_t n_sym       = 300;
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 90.0;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 7,
                   &x, &n, &data);

  dp_dsss_receiver_state_t *rx
      = dp_dsss_receiver_create (CODE7, sf, 1.0e6, sym_rate, (1.0e6) * (spc),
                                 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  DP_CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return 1;
    }

  size_t odd_chunk = te + te / 3; /* deliberately NOT a multiple
                                      of te (one code period). */
  float _Complex *warm_syms;
  size_t          half   = n / 2;
  size_t          warm_n = _stream (rx, x, half, odd_chunk, &warm_syms);
  free (warm_syms);
  (void)warm_n;
  DP_CHECK (dp_dsss_receiver_get_tracking (rx) == 1);
  /* Direct struct-field peek (not a public getter -- reasonable for a C
   * unit test with full struct visibility): confirms the test actually
   * exercises the nonzero-carry case it claims to, rather than silently
   * passing on a lucky zero. */
  DP_CHECK (rx->car_carry_len > 0 && rx->car_carry_len < rx->tsamps);

  size_t cb   = dp_dsss_receiver_state_bytes (rx);
  void  *blob = malloc (cb);
  memset (blob, 0xA5, cb); /* poisoned: a byte get_state skips shows */
  dp_dsss_receiver_get_state (rx, blob);

  /* The carry region is the tsamps samples before the held symbols' store
     (#2469), which ends the blob: the live carry, then zeros, never the
     earlier periods still in the buffer's tail
     (#2076's sibling). Writing the whole buffer again turns this red. The
     source's own tail is checked non-zero first, so the test cannot pass
     on a buffer that happened to hold zeros there. */
  {
    const size_t         fc    = sizeof (float _Complex);
    const unsigned char *src   = (const unsigned char *)rx->car_carry_buf;
    size_t               dirty = 0;
    for (size_t i = rx->car_carry_len * fc; i < rx->tsamps * fc; i++)
      dirty += src[i] != 0;
    DP_CHECK (dirty > 0);
    const unsigned char *carry
        = (const unsigned char *)blob + cb - (rx->tsamps + rx->hold_cap) * fc;
    DP_CHECK (memcmp (carry, rx->car_carry_buf, rx->car_carry_len * fc) == 0);
    size_t nonzero = 0;
    for (size_t i = rx->car_carry_len * fc; i < rx->tsamps * fc; i++)
      nonzero += carry[i] != 0;
    DP_CHECK (nonzero == 0);
  }

  dp_dsss_receiver_state_t *rx2
      = dp_dsss_receiver_create (CODE7, sf, 1.0e6, sym_rate, (1.0e6) * (spc),
                                 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  DP_CHECK (rx2 != NULL);
  if (rx2)
    {
      /* A live carry is 0..tsamps-1 samples, so a blob claiming tsamps is
         refused: steps never leaves one, and get_state never writes it.
         The field is found by offsetof on the public extra, and checked
         against the live value before it is patched. */
      {
        unsigned char *cp = malloc (cb);
        DP_REQUIRE (cp != NULL);
        memcpy (cp, blob, cb);
        const size_t at = sizeof (dp_state_hdr_t)
                          + offsetof (dsss_receiver_extra_t, car_carry_len);
        uint64_t     v;
        memcpy (&v, cp + at, sizeof v);
        DP_CHECK (v == rx->car_carry_len);
        v = rx->tsamps - 1;
        memcpy (cp + at, &v, sizeof v);
        DP_CHECK (dp_dsss_receiver_set_state (rx2, cp) == DP_OK);
        v = rx->tsamps;
        memcpy (cp + at, &v, sizeof v);
        DP_CHECK (dp_dsss_receiver_set_state (rx2, cp) == DP_ERR_INVALID);
        free (cp);
      }
      DP_CHECK (dp_dsss_receiver_set_state (rx2, blob) == DP_OK);
      DP_CHECK (rx2->car_carry_len == rx->car_carry_len);

      float _Complex *rest1, *rest2;
      size_t          rest_n = n - half;
      size_t          k1 = _stream (rx, x + half, rest_n, odd_chunk, &rest1);
      size_t          k2 = _stream (rx2, x + half, rest_n, odd_chunk, &rest2);
      DP_CHECK (k1 == k2);
      size_t kmin = k1 < k2 ? k1 : k2;
      for (size_t i = 0; i < kmin; i++)
        DP_CHECK (cabsf (rest1[i] - rest2[i]) < 1e-4f);
      free (rest1);
      free (rest2);
      dp_dsss_receiver_destroy (rx2);
    }
  free (blob);
  free (x);
  free (data);
  dp_dsss_receiver_destroy (rx);
  return 0;
}

/* Exercise the read-only accessors and the three raw sub-loop reconfigure
 * entry points (search grid / lock detector / track chain) plus the chain
 * rejection paths — thin delegations the algorithm tests never call, reachable
 * on a freshly created (pre-lock) receiver. */
static int
_test_accessor_coverage (void)
{
  dp_dsss_receiver_state_t *rx
      = dp_dsss_receiver_create (CODE7, 7, 1.0e6, 35714.29, (1.0e6) * (4), 2,
                                 70.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  DP_CHECK (rx != NULL);
  if (!rx)
    return 1;

  (void)dp_dsss_receiver_get_lock (rx);
  (void)dp_dsss_receiver_get_norm_freq (rx);
  (void)dp_dsss_receiver_get_doppler_hz (rx);
  (void)dp_dsss_receiver_steps_max_out (rx, 0);

  DP_CHECK (dp_dsss_receiver_configure_search_raw (rx, 1, 1) == 0);
  DP_CHECK (dp_dsss_receiver_configure_search_raw (rx, 100000, 1)
            == DP_ERR_INVALID);
  dp_dsss_receiver_configure_lock_raw (rx, 12.0, 6.0, 8, 0.1, 3, 3);
  DP_CHECK (dp_dsss_receiver_configure_chain_raw (rx, 4, 8, 4) == 0);
  DP_CHECK (dp_dsss_receiver_configure_chain_raw (rx, 0, 8, 4) == -1);
  DP_CHECK (dp_dsss_receiver_configure_chain_raw (rx, 4, 8, 3) == -1);
  /* An n outside dp_ratesync_m_ok() that still divides sps: odd, under 2,
     over RATESYNC_MAX_M. Each passed the old n >= 1 && sps % n == 0 check
     and aborted in the rebuild's MpskReceiver create (doppler#2426). Each
     refusal leaves the receiver on its grid. */
  DP_CHECK (dp_dsss_receiver_configure_chain_raw (rx, 4, 6, 3) == -1);
  DP_CHECK (dp_dsss_receiver_configure_chain_raw (rx, 4, 8, 1) == -1);
  DP_CHECK (dp_dsss_receiver_configure_chain_raw (rx, 4, 10, 10) == -1);
  DP_CHECK (dp_dsss_receiver_configure_chain_raw (rx, 4, 8, -2) == -1);
  DP_CHECK (rx->segments == 4 && rx->sps == 8 && rx->n == 4);
  dp_dsss_receiver_destroy (rx);

  /* create's derived n is one configure_chain_raw accepts (doppler#2650).
     create picks 2*floor(sps/2) capped at 8 -- 2 at sps 3, 4 at 5, 8 at 10,
     none of which divides sps -- and the old divisor check refused the
     receiver's own grid when re-applied. One rule now serves both. */
  static const size_t odd_sps[] = { 3, 5, 10 };
  for (size_t k = 0; k < sizeof odd_sps / sizeof *odd_sps; k++)
    {
      dp_dsss_receiver_state_t *r = dp_dsss_receiver_create (
          CODE7, 7, 1.0e6, 35714.29, (1.0e6) * (4), 2, 70.0, 1e-2, 0.9, 500.0,
          4, odd_sps[k], 0);
      DP_REQUIRE (r != NULL);
      const int n = dp_dsss_receiver_get_n (r);
      DP_CHECK ((size_t)n == mpsk_rx_derive_m_out ((double)odd_sps[k], 0));
      DP_CHECK (dp_dsss_receiver_configure_chain_raw (r, 4, odd_sps[k], n)
                == 0);
      DP_CHECK (dp_dsss_receiver_get_n (r) == n);
      dp_dsss_receiver_destroy (r);
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
  const size_t sf = 7, spc = 4, te = sf * spc, n_sym = 300;
  const double fs = 1.0e6 * (double)spc, tsym = fs / 35714.29;

  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, 90.0, n_sym, te * 5 + 3, 7,
                   &x, &n, &data);

  dp_dsss_receiver_state_t *one
      = dp_dsss_receiver_create (CODE7, sf, 1.0e6, 35714.29, (1.0e6) * (spc),
                                 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  dp_dsss_receiver_state_t *chunked
      = dp_dsss_receiver_create (CODE7, sf, 1.0e6, 35714.29, (1.0e6) * (spc),
                                 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  DP_CHECK (one != NULL && chunked != NULL);
  if (!one || !chunked)
    {
      dp_dsss_receiver_destroy (one);
      dp_dsss_receiver_destroy (chunked);
      free (x);
      free (data);
      return 1;
    }
  /* The premise: frames after a hit that decide nothing. */
  DP_CHECK (one->acq->search->n_noncoh > 1);

  float _Complex *a, *b;
  size_t          n_a = _stream (one, x, n, n, &a);
  size_t          n_b = _stream (chunked, x, n, te, &b);
  DP_CHECK (dp_dsss_receiver_get_tracking (one) == 1);
  DP_CHECK (n_a > 20);
  DP_CHECK (n_a == n_b && memcmp (a, b, n_a * sizeof *a) == 0);

  free (a);
  free (b);
  dp_dsss_receiver_destroy (one);
  dp_dsss_receiver_destroy (chunked);
  free (x);
  free (data);
  return 0;
}

/* A call of a few samples that completes a carried period keeps the
 * period's partials. The tracking chain wipes and despreads a whole code
 * period at a time, carrying the remainder, so a 1-sample call that
 * completes the carry runs a period through the Dll, which emits its
 * `segments` partials (4 here). Its scratch held x_len before #3281's
 * audit, 1 here, and dp_dll_steps() drops what does not fit silently
 * (#3282): 3 partials a period lost. The case: the hand-off geometry above
 * fed as calls of te - 1 and 1 samples, which must equal one call over the
 * capture bit for bit. Sabotage: the Dll's scratch back to x_len -> the
 * chunked run loses partials and differs, red. */
static int
_test_small_call_keeps_a_carried_period (void)
{
  const size_t    sf = 7, spc = 4, te = sf * spc, n_sym = 300;
  const double    fs = 1.0e6 * (double)spc, tsym = fs / 35714.29;
  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, 90.0, n_sym, te * 5 + 3, 7,
                   &x, &n, &data);
  dp_dsss_receiver_state_t *one = dp_dsss_receiver_create (
      CODE7, sf, 1.0e6, 35714.29, fs, 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  dp_dsss_receiver_state_t *chunked = dp_dsss_receiver_create (
      CODE7, sf, 1.0e6, 35714.29, fs, 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  DP_REQUIRE (one != NULL && chunked != NULL);
  float _Complex *a;
  const size_t    n_a = _stream (one, x, n, n, &a);
  float _Complex *b   = malloc (n * sizeof *b);
  DP_REQUIRE (b != NULL);
  size_t n_b = 0;
  for (size_t pos = 0, k = 0; pos < n; k++)
    {
      size_t take = (k & 1u) ? 1 : te - 1;
      take        = pos + take <= n ? take : n - pos;
      n_b += dp_dsss_receiver_steps (chunked, x + pos, take, b + n_b, n - n_b);
      pos += take;
    }
  DP_REQUIRE (dp_dsss_receiver_get_tracking (one) == 1 && n_a > 20);
  printf ("  calls of %zu and 1 samples: %zu symbols against one call's "
          "%zu\n",
          te - 1, n_b, n_a);
  DP_CHECK_MSG (n_a == n_b && memcmp (a, b, n_a * sizeof *a) == 0,
                "a 1-sample call that completes a carried period keeps its "
                "partials");
  free (a);
  free (b);
  dp_dsss_receiver_destroy (one);
  dp_dsss_receiver_destroy (chunked);
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
  const size_t    sf = 7, spc = 4, te = sf * spc, n_sym = 300;
  const double    fs = 1.0e6 * (double)spc, tsym = fs / 35714.29;
  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (CODE7, sf, spc, fs, tsym, 0.0, 90.0, n_sym, te * 5 + 3, 7,
                   &x, &n, &data);
  const size_t cut = 2 * te + 5; /* inside the silence: frames and a carry */

  dp_dsss_receiver_state_t *a
      = dp_dsss_receiver_create (CODE7, sf, 1.0e6, 35714.29, (1.0e6) * (spc),
                                 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  dp_dsss_receiver_state_t *ref
      = dp_dsss_receiver_create (CODE7, sf, 1.0e6, 35714.29, (1.0e6) * (spc),
                                 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  dp_dsss_receiver_state_t *b
      = dp_dsss_receiver_create (CODE7, sf, 1.0e6, 35714.29, (1.0e6) * (spc),
                                 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  dp_dsss_receiver_state_t *whole
      = dp_dsss_receiver_create (CODE7, sf, 1.0e6, 35714.29, (1.0e6) * (spc),
                                 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  DP_CHECK (a && ref && b && whole);
  if (!a || !ref || !b || !whole)
    {
      dp_dsss_receiver_destroy (a);
      dp_dsss_receiver_destroy (ref);
      dp_dsss_receiver_destroy (b);
      dp_dsss_receiver_destroy (whole);
      free (x);
      free (data);
      return 1;
    }
  float _Complex *o1 = malloc (n * sizeof *o1), *o2 = malloc (n * sizeof *o2);
  DP_CHECK (o1 && o2);

  /* (a) Reconfigured mid-search: the engine restarts at 0; a fresh
     receiver configured the same way and fed the same rest is the truth. */
  DP_CHECK (dp_dsss_receiver_steps (a, x, cut, o1, n) == 0);
  DP_CHECK (dp_dsss_receiver_configure_search_raw (a, 1, 4) == 0);
  DP_CHECK (dp_dsss_receiver_configure_search_raw (ref, 1, 4) == 0);
  size_t na = dp_dsss_receiver_steps (a, x + cut, n - cut, o1, n);
  size_t nr = dp_dsss_receiver_steps (ref, x + cut, n - cut, o2, n);
  DP_CHECK (dp_dsss_receiver_get_tracking (a) == 1);
  DP_CHECK (na > 20 && na == nr && memcmp (o1, o2, na * sizeof *o1) == 0);

  /* (b) Restored mid-search into a fresh receiver: the uninterrupted run
     over the whole capture is the truth. */
  dp_dsss_receiver_state_t *src
      = dp_dsss_receiver_create (CODE7, sf, 1.0e6, 35714.29, (1.0e6) * (spc),
                                 2, 58.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  DP_CHECK (src != NULL);
  if (src)
    {
      DP_CHECK (dp_dsss_receiver_steps (src, x, cut, o1, n) == 0);
      void *blob = malloc (dp_dsss_receiver_state_bytes (src));
      DP_CHECK (blob != NULL);
      if (blob)
        {
          dp_dsss_receiver_get_state (src, blob);
          DP_CHECK (dp_dsss_receiver_set_state (b, blob) == DP_OK);
          size_t nb = dp_dsss_receiver_steps (b, x + cut, n - cut, o1, n);
          size_t nw = dp_dsss_receiver_steps (whole, x, n, o2, n);
          DP_CHECK (dp_dsss_receiver_get_tracking (b) == 1);
          DP_CHECK (nb > 20 && nb == nw
                    && memcmp (o1, o2, nb * sizeof *o1) == 0);
          free (blob);
        }
      dp_dsss_receiver_destroy (src);
    }

  free (o1);
  free (o2);
  dp_dsss_receiver_destroy (a);
  dp_dsss_receiver_destroy (ref);
  dp_dsss_receiver_destroy (b);
  dp_dsss_receiver_destroy (whole);
  free (x);
  free (data);
  return 0;
}

/* #2469's geometry in C: a 127-chip code at 1 Mcps and 250 ksym/s, 4
 * samples a chip -- 31.75 symbols a code period, legal here (create()
 * refuses only a symbol rate above the chip rate), so the call that
 * completes a carried period makes the whole period's symbols at once. */
static dp_dsss_receiver_state_t *
_rx127 (const uint8_t *code)
{
  return dp_dsss_receiver_create (code, 127, 1e6, 250e3, 4e6, 2, 70.0, 1e-3,
                                  0.9, 100.0, 4, 8, 0);
}

/* No symbol is dropped (#2469). Three receivers on one capture at #2469's
 * geometry, one sample a call:
 * - B with out sized by steps_max_out(), so it never holds;
 * - A at capacity ONE, called again on the same sample until it consumes
 *   it, then with no input until nothing is held: A's stream is B's,
 *   symbol for symbol, and A held (a period's 31 symbols arrive in one
 *   call);
 * - C never drains (max_out 0): once it holds, a call takes nothing and
 *   keeps the hold, and a call with room delivers B's first symbols and
 *   consumes again.
 * Then the held symbols round-trip: C, holding, restored into a fresh
 * twin, and the two deliver the same stream; a blob claiming more than the
 * store holds is refused. Sabotages: discard the held symbols -> A short,
 * red; consume while holding -> C takes input, red; leave the store out of
 * set_state -> the twin differs, red. */
static int
_test_no_symbol_is_dropped (void)
{
  int      fails = 0;
  uint8_t  code[127];
  uint32_t st = 0x1234567u;
  for (size_t i = 0; i < 127; i++)
    code[i] = (uint8_t)(dp_xs32 (&st) & 1u);
  float _Complex *x;
  size_t          n;
  double         *data;
  const size_t    n_sym = 3000;
  dp_dsss_capture (code, 127, 4, 4e6, 16.0, 0.0, 70.0, n_sym, 3 * 508, 11, &x,
                   &n, &data);
  dp_dsss_receiver_state_t *a = _rx127 (code), *b = _rx127 (code),
                           *c = _rx127 (code), *t = _rx127 (code);
  DP_REQUIRE (a && b && c && t);
  const size_t    capb = dp_dsss_receiver_steps_max_out (b, 1);
  const size_t    most = 2 * n_sym + capb;
  float _Complex *ob   = malloc (capb * sizeof *ob),
                 *oa   = malloc (17 * sizeof *oa);
  float _Complex *sa   = malloc (most * sizeof *sa),
                 *sb   = malloc (most * sizeof *sb);
  DP_REQUIRE (ob && oa && sa && sb);
  size_t la = 0, lb = 0;
  int    held = 0, wrote_past = 0;
  for (size_t g = 0; g < 16; g++)
    oa[1 + g] = 12345.0f;
  for (size_t pos = 0; pos < n; pos++)
    {
      const size_t kb = dp_dsss_receiver_steps (b, x + pos, 1, ob, capb);
      DP_REQUIRE (lb + kb <= most);
      memcpy (sb + lb, ob, kb * sizeof *ob);
      lb += kb;
      do
        {
          const size_t ka = dp_dsss_receiver_steps (a, x + pos, 1, oa, 1);
          DP_REQUIRE (la + ka <= most);
          memcpy (sa + la, oa, ka * sizeof *oa);
          la += ka;
          held |= a->hold_len > 0;
        }
      while (dp_dsss_receiver_get_consumed (a) == 0);
    }
  size_t ka;
  while ((ka = dp_dsss_receiver_steps (a, x, 0, oa, 1)) > 0)
    {
      DP_REQUIRE (la + ka <= most);
      memcpy (sa + la, oa, ka * sizeof *oa);
      la += ka;
    }
  for (size_t g = 0; g < 16; g++)
    wrote_past |= oa[1 + g] != 12345.0f;
  DP_CHECK_MSG (held, "at capacity 1, a call held symbols (the premise)");
  DP_CHECK_MSG (la == lb && memcmp (sa, sb, lb * sizeof *sa) == 0,
                "held and delivered in order: A's stream is B's");
  if (la != lb)
    DP_TEST_ERR ("  A gave %zu symbols, B %zu\n", la, lb);
  DP_CHECK_MSG (wrote_past == 0, "nothing is written past max_out");
  DP_CHECK (lb > n_sym * 3 / 4);

  /* Never draining. */
  size_t pos = 0;
  while (pos < n && c->hold_len == 0)
    (void)dp_dsss_receiver_steps (c, x + pos++, 1, oa, 0);
  DP_REQUIRE (c->hold_len > 0);
  const size_t was = c->hold_len;
  DP_CHECK (dp_dsss_receiver_steps (c, x + pos, 1, oa, 0) == 0);
  DP_CHECK_MSG (dp_dsss_receiver_get_consumed (c) == 0 && c->hold_len == was,
                "held, a call of no room takes nothing and keeps the hold");

  /* The held symbols ride the blob: one size per object, holding or not. */
  const size_t sz = dp_dsss_receiver_state_bytes (c);
  DP_CHECK (sz == dp_dsss_receiver_state_bytes (t));
  uint8_t *blob = malloc (sz);
  DP_REQUIRE (blob);
  dp_dsss_receiver_get_state (c, blob);
  DP_CHECK (dp_dsss_receiver_set_state (t, blob) == DP_OK
            && t->hold_len == was);
  const size_t    capc = dp_dsss_receiver_steps_max_out (c, 508);
  float _Complex *oc   = malloc (capc * sizeof *oc),
                 *ot   = malloc (capc * sizeof *ot);
  DP_REQUIRE (oc && ot);
  size_t first = 1, twin_ok = 1;
  for (; pos + 508 <= n; pos += 508)
    {
      const size_t kc = dp_dsss_receiver_steps (c, x + pos, 508, oc, capc);
      const size_t kt = dp_dsss_receiver_steps (t, x + pos, 508, ot, capc);
      if (first)
        {
          DP_CHECK_MSG (kc >= was && memcmp (oc, sb, was * sizeof *oc) == 0,
                        "a call with room delivers the held symbols first");
          DP_CHECK_MSG (dp_dsss_receiver_get_consumed (c) == 508,
                        "and consumes again");
          first = 0;
        }
      twin_ok &= kc == kt && memcmp (oc, ot, kc * sizeof *oc) == 0;
    }
  DP_CHECK_MSG (twin_ok, "the twin delivers the original's stream");
  dsss_receiver_extra_t *ex
      = (dsss_receiver_extra_t *)(blob + sizeof (dp_state_hdr_t));
  ex->hold_len = (uint64_t)t->hold_cap + 1;
  DP_CHECK_MSG (dp_dsss_receiver_set_state (t, blob) == DP_ERR_INVALID,
                "a held count past the store is refused");
  dp_dsss_receiver_destroy (a);
  dp_dsss_receiver_destroy (b);
  dp_dsss_receiver_destroy (c);
  dp_dsss_receiver_destroy (t);
  free (blob);
  free (oa);
  free (ob);
  free (oc);
  free (ot);
  free (sa);
  free (sb);
  free (x);
  free (data);
  return fails;
}

/* A grid change waits for the store (the review of #3397). The store
 * holds what one call of a code period can produce at the receiver's grid,
 * so configure_chain_raw() is refused while symbols are held -- none is
 * dropped and the caller drains first -- and once nothing is held it
 * resizes the store to the new grid's bound, the size a receiver created
 * at that grid has. Sabotages: drop the refusal -> the change is taken
 * while holding, red; skip the resize -> the store keeps the old grid's
 * capacity, red. */
static int
_test_a_grid_change_waits_for_the_store (void)
{
  int      fails = 0;
  uint8_t  code[127];
  uint32_t st = 0x1234567u;
  for (size_t i = 0; i < 127; i++)
    code[i] = (uint8_t)(dp_xs32 (&st) & 1u);
  float _Complex *x;
  size_t          n;
  double         *data;
  dp_dsss_capture (code, 127, 4, 4e6, 16.0, 0.0, 70.0, 400, 3 * 508, 11, &x,
                   &n, &data);
  dp_dsss_receiver_state_t *r = _rx127 (code);
  DP_REQUIRE (r);
  float _Complex none[1];
  size_t pos = 0;
  while (pos < n && r->hold_len == 0)
    (void)dp_dsss_receiver_steps (r, x + pos++, 1, none, 0);
  DP_REQUIRE (r->hold_len > 0);
  const size_t was = r->hold_len, old_cap = r->hold_cap;
  /* sps moves the bound (the converter's ratio is proportional to it;
     segments cancels between the Dll's partials and the ratio) */
  const int n16 = (int)mpsk_rx_derive_m_out (16.0, 0);
  DP_CHECK_MSG (dp_dsss_receiver_configure_chain_raw (r, 4, 16, n16) == -1
                    && r->hold_len == was && r->sps == 8,
                "a grid change is refused while symbols are held");
  const size_t    cap = dp_dsss_receiver_steps_max_out (r, 0);
  float _Complex *o   = malloc (cap * sizeof *o);
  DP_REQUIRE (o);
  DP_CHECK (dp_dsss_receiver_steps (r, x, 0, o, cap) == was);
  DP_REQUIRE (r->hold_len == 0);
  DP_CHECK (dp_dsss_receiver_configure_chain_raw (r, 4, 16, n16) == 0);
  dp_dsss_receiver_state_t *f = dp_dsss_receiver_create (
      code, 127, 1e6, 250e3, 4e6, 2, 70.0, 1e-3, 0.9, 100.0, 4, 16, 0);
  DP_REQUIRE (f);
  DP_CHECK (f->hold_cap != old_cap); /* the premise: the bound moved */
  DP_CHECK_MSG (r->hold_cap == f->hold_cap
                    && dp_dsss_receiver_state_bytes (r)
                           == dp_dsss_receiver_state_bytes (f),
                "the store is resized to the new grid's bound");
  dp_dsss_receiver_destroy (f);
  dp_dsss_receiver_destroy (r);
  free (o);
  free (x);
  free (data);
  return fails;
}

#ifdef NDEBUG
/* A call that overruns its bound fills every slot it is handed. */
static size_t
_overrun (void *ctx, const float _Complex *x, size_t x_len,
          float _Complex *out, size_t max_out)
{
  (void)ctx;
  (void)x;
  (void)x_len;
  for (size_t i = 0; i < max_out; i++)
    out[i] = (float _Complex) (float)(i + 1);
  return max_out;
}

/* A wrong bound cannot write past the store in a Release build (the
 * review of #3397). The gate takes a call whose bound fits the room plus
 * the store, so the call must be handed no more than the bound: a call
 * that overruns it then stops there, and what is left after out takes its
 * room fits the store. Here a store of 2, no room, and a bound of 2 -- the
 * gate's edge -- with a call that fills whatever it is handed. Sabotage:
 * hand it bound + 1 in Release too, as before -> three symbols written
 * into a store of two (hold_len 3; ASan names the heap write). A Debug
 * build aborts on the assert instead, so this runs in Release only. */
static int
_test_a_wrong_bound_cannot_write_past_the_store (void)
{
  int             fails = 0;
  float _Complex *hold  = malloc (2 * sizeof *hold);
  float _Complex *scr   = NULL;
  size_t          len = 0, scap = 0, consumed = 0;
  float _Complex x[1] = { 0 }, out[1];
  DP_REQUIRE (hold);
  const size_t k = dp_sym_hold_steps (hold, &len, 2, &scr, &scap, &consumed, 2,
                                      _overrun, NULL, x, 1, out, 0);
  DP_CHECK (k == 0 && consumed == 1);
  DP_CHECK_MSG (len <= 2, "the store holds no more than its capacity");
  /* What was held is read back, so the stores into the store are live and
     an optimizer cannot drop them: an overrun is then a write ASan sees,
     not a dead store. */
  volatile float sink = 0.0f;
  for (size_t i = 0; i < len; i++)
    sink += crealf (hold[i]);
  (void)sink;
  free (hold);
  free (scr);
  return fails;
}
#endif

int
main (void)
{
  (void)_test_arg_validation ();
  (void)_test_acquire_and_decode ();
  (void)_test_handoff_resumes_at_the_hit ();
  (void)_test_small_call_keeps_a_carried_period ();
  (void)_test_handoff_survives_reconfigure_and_restore ();
  (void)_test_carrier_dll_composition_ramp ();
  (void)_test_sustained_doppler_rate ();
  (void)_test_carry_buffer_state_roundtrip ();
  (void)_test_accessor_coverage ();
  DP_CHECK (_test_no_symbol_is_dropped () == 0);
  DP_CHECK (_test_a_grid_change_waits_for_the_store () == 0);
#ifdef NDEBUG
  DP_CHECK (_test_a_wrong_bound_cannot_write_past_the_store () == 0);
#endif
  DP_TEST_END ("test_dsss_receiver_core");
}
