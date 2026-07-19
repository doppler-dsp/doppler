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
#include "costas/costas_core.h"
#include "dll/dll_core.h"
#include "dsss_receiver/dsss_receiver_core.h"
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(cond)                                                           \
  do                                                                          \
    {                                                                         \
      if (!(cond))                                                            \
        {                                                                     \
          fprintf (stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
          _fails++;                                                           \
        }                                                                     \
    }                                                                         \
  while (0)

/* A length-7 maximal-length sequence (one period) -- same fixture
 * test_acq_core.c uses for its own fast, real (not mocked) unit tests. */
static const uint8_t CODE7[7] = { 1, 1, 1, 0, 1, 0, 0 };

/* xorshift32 PRBS, +-1 -- same generator test_despreader_core.c's own
 * make_code()/make_signal() use, for a longer synthetic spreading code. */
static int
prbs (uint32_t *st)
{
  uint32_t x = *st;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *st = x;
  return (x & 1u) ? -1 : 1;
}

/* Unit-variance complex Gaussian (Box-Muller from xorshift) -- same
 * generator as test_acq_core.c/test_dll_core.c/test_symsync_core.c (no
 * shared test-utils header exists for it yet). */
static float complex
cgauss (uint32_t *st)
{
  *st ^= *st << 13;
  *st ^= *st >> 17;
  *st ^= *st << 5;
  uint32_t a = *st;
  *st ^= *st << 13;
  *st ^= *st >> 17;
  *st ^= *st << 5;
  uint32_t b   = *st;
  double   u1  = ((double)a + 1.0) / 4294967297.0;
  double   u2  = ((double)b + 1.0) / 4294967297.0;
  double   mag = sqrt (-log (u1));
  double   th  = 6.283185307179586 * u2;
  return (float)(mag * cos (th)) + (float)(mag * sin (th)) * I;
}

static int
_test_arg_validation (void)
{
  int _fails = 0;
  CHECK (dsss_receiver_create (NULL, 0, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9, 100.0,
                               4, 8, 0)
         == NULL);
  CHECK (dsss_receiver_create (CODE7, 7, 0.0, 1e3, 2, 2, 55.0, 1e-3, 0.9,
                               100.0, 4, 8, 0)
         == NULL); /* chip_rate <= 0 */
  CHECK (dsss_receiver_create (CODE7, 7, 1e6, 1e3, 2, 3, 55.0, 1e-3, 0.9,
                               100.0, 4, 8, 0)
         == NULL); /* m not in {2,4,8} */
  CHECK (dsss_receiver_create (CODE7, 7, 1e6, 1e3, 2, 2, 55.0, 1e-3, 0.9,
                               100.0, 0, 8, 0)
         == NULL); /* segments < 1 */

  dsss_receiver_state_t *rx = dsss_receiver_create (
      CODE7, 7, 1.0e6, 35714.29, 4, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  CHECK (rx != NULL);
  if (rx)
    {
      CHECK (dsss_receiver_get_tracking (rx) == 0);
      CHECK (dsss_receiver_get_segments (rx) == 4);
      CHECK (dsss_receiver_get_sps (rx) == 8);
      CHECK (dsss_receiver_get_n (rx) == 4); /* sps=8 -> largest divisor 4 */
      CHECK (dsss_receiver_get_chip_phase (rx) == 0.0);
      CHECK (dsss_receiver_get_code_rate (rx) == 1.0);
      dsss_receiver_destroy (rx);
    }
  return _fails;
}

/* Build a continuous, code-spread BPSK capture: silence, then
 * data[si] * code[cph] * exp(j*2*pi*doppler_hz/fs*idx) + AWGN. Mirrors this
 * repo's own Python story's make_signal(), simplified (no asynchronous
 * symbol/code-epoch clock stress -- that physics is already validated at
 * the Acquisition/Dll level; this test is about the composed object's
 * wiring) and sized for a fast C test. */
static void
_make_signal (const uint8_t *code, size_t sf, size_t spc, double fs,
              double tsym, double doppler_hz, double cn0_dbhz, size_t n_sym,
              size_t pre_silence, uint32_t seed, float complex **x_out,
              size_t *n_out, double **data_out)
{
  float *csign = malloc (sf * sizeof *csign);
  for (size_t i = 0; i < sf; i++)
    csign[i] = code[i] & 1 ? -1.0f : 1.0f;

  double  *data = malloc ((n_sym + 4) * sizeof *data);
  uint32_t st   = seed ? seed : 1;
  for (size_t i = 0; i < n_sym + 4; i++)
    {
      st ^= st << 13;
      st ^= st >> 17;
      st ^= st << 5;
      data[i] = (st & 1u) ? 1.0 : -1.0;
    }

  size_t         n   = (size_t)((double)n_sym * tsym) + 4 * sf * spc;
  size_t         tot = pre_silence + n;
  float complex *x   = calloc (tot, sizeof *x);

  double amp_snr = sqrt (pow (10.0, cn0_dbhz / 10.0) / fs);
  double sigma   = 1.0 / amp_snr;
  for (size_t i = 0; i < tot; i++)
    x[i] = (float complex) (sigma / sqrt (2.0)) * cgauss (&st);

  for (size_t idx = 0; idx < n; idx++)
    {
      size_t si = (size_t)((double)idx / tsym);
      if (si >= n_sym + 4)
        si = n_sym + 3;
      size_t cph = (idx / spc) % sf;
      double ph = 2.0 * 3.14159265358979323846 * doppler_hz / fs * (double)idx;
      float complex carrier = (float complex) (cos (ph) + I * sin (ph));
      x[pre_silence + idx] += (float)(data[si] * csign[cph]) * carrier;
    }

  free (csign);
  *x_out    = x;
  *n_out    = tot;
  *data_out = data;
}

/* Stream `x` through `rx` in fixed-size chunks, collecting every emitted
 * symbol; return the symbol count and fill `*syms_out` (caller frees). */
static size_t
_stream (dsss_receiver_state_t *rx, const float complex *x, size_t n,
         size_t chunk, float complex **syms_out)
{
  float complex *syms   = malloc (n * sizeof *syms); /* generous upper bound */
  size_t         n_syms = 0;
  for (size_t pos = 0; pos < n; pos += chunk)
    {
      size_t take = (pos + chunk <= n) ? chunk : (n - pos);
      n_syms += dsss_receiver_steps (rx, x + pos, take, syms + n_syms,
                                     n - n_syms);
    }
  *syms_out = syms;
  return n_syms;
}

/* Best-lag BER over the back half of the recovered symbols against the
 * known data (absorbing the composed chain's settling transient and
 * pipeline delay -- the existence of a clean near-zero-error lag is the
 * proof, the same pattern this repo's gallery examples use in Python). */
static double
_best_ber (const float complex *syms, size_t n_syms, const double *data,
           size_t n_sym)
{
  if (n_syms < 20)
    return 1.0;
  size_t lo = n_syms / 2, hi = n_syms;
  double best = 1.0;
  for (int lag = -20; lag <= 20; lag++)
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
        continue; /* not enough overlap at this lag to be meaningful */
      double ber = (double)errs / (double)cnt;
      if (ber < best)
        best = ber;
      if (1.0 - ber < best) /* phase/sign ambiguity: inverted bits */
        best = 1.0 - ber;
    }
  return best;
}

static int
_test_acquire_and_decode (void)
{
  int _fails = 0;

  const size_t sf          = 7;
  const size_t spc         = 4;
  const double fs          = 1.0e6 * (double)spc;
  const double sym_rate    = 35714.29;
  const double tsym        = fs / sym_rate;
  const size_t te          = sf * spc;
  const size_t n_sym       = 300;
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 90.0;

  float complex *x;
  size_t         n;
  double        *data;
  _make_signal (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 7, &x,
                &n, &data);

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
  dsss_receiver_state_t *rx = dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return _fails + 1;
    }

  float complex *syms;
  size_t         n_syms = _stream (rx, x, n, te, &syms);

  CHECK (dsss_receiver_get_tracking (rx) == 1);
  CHECK (n_syms > 20);
  CHECK (dsss_receiver_get_cn0_dbhz_est (rx) > 0.0);

  double ber = _best_ber (syms, n_syms, data, n_sym + 4);
  CHECK (ber < 0.05);

  /* ── state-serialization round trip, while tracking ─────────────────── */
  size_t cb   = dsss_receiver_state_bytes (rx);
  void  *blob = malloc (cb);
  dsss_receiver_get_state (rx, blob);

  dsss_receiver_state_t *rx2 = dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  CHECK (rx2 != NULL);
  if (rx2)
    {
      CHECK (dsss_receiver_set_state (rx2, blob) == DP_OK);
      CHECK (dsss_receiver_get_tracking (rx2) == 1);
      CHECK (fabs (dsss_receiver_get_chip_phase (rx2)
                   - dsss_receiver_get_chip_phase (rx))
             < 1e-9);

      /* a corrupted envelope must be rejected, not reinterpreted. */
      ((char *)blob)[0] ^= (char)0xFF;
      CHECK (dsss_receiver_set_state (rx2, blob) == DP_ERR_INVALID);
      dsss_receiver_destroy (rx2);
    }
  free (blob);

  /* ── state-serialization round trip, while searching ─────────────────── */
  dsss_receiver_state_t *rx3 = dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  CHECK (rx3 != NULL);
  if (rx3)
    {
      size_t cb3   = dsss_receiver_state_bytes (rx3);
      void  *blob3 = malloc (cb3);
      dsss_receiver_get_state (rx3, blob3);

      dsss_receiver_state_t *rx4 = dsss_receiver_create (
          CODE7, sf, 1.0e6, sym_rate, spc, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0);
      CHECK (rx4 != NULL);
      if (rx4)
        {
          CHECK (dsss_receiver_set_state (rx4, blob3) == DP_OK);
          CHECK (dsss_receiver_get_tracking (rx4) == 0);
          dsss_receiver_destroy (rx4);
        }
      free (blob3);
      dsss_receiver_destroy (rx3);
    }

  /* ── reset() returns to searching ─────────────────────────────────────── */
  dsss_receiver_reset (rx);
  CHECK (dsss_receiver_get_tracking (rx) == 0);
  CHECK (dsss_receiver_get_chip_phase (rx) == 0.0);

  free (syms);
  free (x);
  free (data);
  dsss_receiver_destroy (rx);
  return _fails;
}

/* ── Isolated proof for the planned pre-despread carrier composition ──────
 * (task #93/#94, ~/.claude/plans/crystalline-knitting-hopper.md): does
 * costas_wipeoff() -> dll_steps(segments=4) -> costas_update(), chunked one
 * code period at a time, actually track a Doppler RATE (not just a static
 * residual) when composed from the raw primitives -- before that exact
 * sequence gets wired into dsss_receiver_core.c's own internals (task #95).
 * Mirrors this session's own Python validation methodology (a ramp, not a
 * static offset; bn_fll=0 vs bn_fll>0 to prove the mechanism is NEEDED, not
 * just present) at the SPEC-derived bn_carrier=0.01/bn_fll=0.03 operating
 * point, using the real segments=4 Dll DsssReceiver already relies on. */
static double
_run_ramp_composition (const uint8_t *code, size_t sf, size_t spc, double fs,
                       double rate_hz_per_s, size_t n_periods, double sigma,
                       double bn_fll, uint32_t seed)
{
  size_t         tsamps = sf * spc;
  size_t         n      = n_periods * tsamps;
  float complex *x      = malloc (n * sizeof *x);
  uint32_t       nst    = seed;
  for (size_t idx = 0; idx < n; idx++)
    {
      double        t       = (double)idx / fs;
      double        ph      = 2.0 * M_PI * (0.5 * rate_hz_per_s * t * t);
      float complex carrier = (float complex) (cos (ph) + I * sin (ph));
      size_t        cph     = (idx / spc) % sf;
      float         csgn    = (code[cph] & 1u) ? -1.0f : 1.0f;
      float complex noise
          = (float complex) (sigma / sqrt (2.0)) * cgauss (&nst);
      x[idx] = csgn * carrier + noise;
    }

  dll_state_t   *dll = dll_create (code, sf, spc, 0.0, 0.002, 0.707, 0.5, 4);
  costas_state_t car;
  costas_init (&car, 0.01, 0.707, 0.0, tsamps, bn_fll);

  float complex *wiped = malloc (tsamps * sizeof *wiped);
  for (size_t p = 0; p < n_periods; p++)
    {
      const float complex *chunk = x + p * tsamps;
      for (size_t i = 0; i < tsamps; i++)
        wiped[i] = costas_wipeoff (&car, chunk[i]);
      float complex prompt;
      /* exactly one period's worth of raw samples in; ordinarily exactly
       * one prompt out, but Dll's own tracked code_rate need not be
       * precisely 1.0 -- don't hard-assert the count, mirroring the
       * production loop's own planned discipline (task #95's own doc). */
      size_t n_out = dll_steps (dll, wiped, tsamps, &prompt, 1);
      if (n_out == 1)
        costas_update (&car, prompt);
    }

  double tracked_hz  = costas_get_norm_freq (&car) * fs;
  double true_at_end = rate_hz_per_s * (double)n_periods * (double)tsamps / fs;

  free (wiped);
  free (x);
  dll_destroy (dll);
  return fabs (tracked_hz - true_at_end);
}

static int
_test_carrier_dll_composition_ramp (void)
{
  int _fails = 0;
  /* sf=7 (CODE7) makes the per-period rate absurdly fast (~143 kHz),
   * trivially tracking a 500 Hz/s ramp with a bare PLL -- not
   * representative of SPEC's real geometry. Use a PRBS-1023 code + spc=2
   * to reproduce the same tsamps=2046/fs=4.092e6 operating point this
   * session's real_costas_fll_3_10db.png investigation already validated
   * the standalone Costas object against. */
  const size_t sf            = 1023;
  const size_t spc           = 2;
  const double fs            = 4.092e6;
  const double rate_hz_per_s = 500.0; /* SPEC's corrected worst case */
  const size_t n_periods     = 2700;  /* matches this session's own
                                          Python floor-sweep scale */
  const double sigma = 0.6;           /* a stressed, not comfortable,
                                          operating point */

  uint8_t *code = malloc (sf);
  uint32_t cst  = 7;
  for (size_t i = 0; i < sf; i++)
    code[i] = (uint8_t)(prbs (&cst) > 0 ? 0u : 1u);

  double err_off = _run_ramp_composition (code, sf, spc, fs, rate_hz_per_s,
                                          n_periods, sigma, 0.0, 11);
  double err_on  = _run_ramp_composition (code, sf, spc, fs, rate_hz_per_s,
                                          n_periods, sigma, 0.03, 11);
  free (code);

  /* The mechanism must be NEEDED: a bare PLL (bn_fll=0) should not track
   * this ramp anywhere near as well as bn_fll=0.03 -- proves this isn't
   * passing by accident (e.g. a ramp too gentle to matter). */
  CHECK (err_on < err_off);
  /* And it must actually WORK: bounded absolute error at SPEC's own
   * worst-case rate, generous vs. the Python-validated ~63 Hz mean (this
   * is a single-seed C smoke test, not a calibrated statistical sweep --
   * that's what task #97's integration test is for). */
  CHECK (err_on < 300.0);

  return _fails;
}

/* Build a continuous, code-spread BPSK capture like _make_signal(), but
 * with a linear Doppler RAMP (chirp) instead of a fixed residual --
 * f(t) = rate_hz_per_s * t, phase = the integral of that. */
static void
_make_ramp_signal (const uint8_t *code, size_t sf, size_t spc, double fs,
                   double tsym, double rate_hz_per_s, double cn0_dbhz,
                   size_t n_sym, size_t pre_silence, uint32_t seed,
                   float complex **x_out, size_t *n_out, double **data_out)
{
  float *csign = malloc (sf * sizeof *csign);
  for (size_t i = 0; i < sf; i++)
    csign[i] = code[i] & 1 ? -1.0f : 1.0f;

  double  *data = malloc ((n_sym + 4) * sizeof *data);
  uint32_t st   = seed ? seed : 1;
  for (size_t i = 0; i < n_sym + 4; i++)
    {
      st ^= st << 13;
      st ^= st >> 17;
      st ^= st << 5;
      data[i] = (st & 1u) ? 1.0 : -1.0;
    }

  size_t         n   = (size_t)((double)n_sym * tsym) + 4 * sf * spc;
  size_t         tot = pre_silence + n;
  float complex *x   = calloc (tot, sizeof *x);

  double amp_snr = sqrt (pow (10.0, cn0_dbhz / 10.0) / fs);
  double sigma   = 1.0 / amp_snr;
  for (size_t i = 0; i < tot; i++)
    x[i] = (float complex) (sigma / sqrt (2.0)) * cgauss (&st);

  for (size_t idx = 0; idx < n; idx++)
    {
      size_t si = (size_t)((double)idx / tsym);
      if (si >= n_sym + 4)
        si = n_sym + 3;
      size_t cph = (idx / spc) % sf;
      double t   = (double)idx / fs;
      double ph = 2.0 * 3.14159265358979323846 * (0.5 * rate_hz_per_s * t * t);
      float complex carrier = (float complex) (cos (ph) + I * sin (ph));
      x[pre_silence + idx] += (float)(data[si] * csign[cph]) * carrier;
    }

  free (csign);
  *x_out    = x;
  *n_out    = tot;
  *data_out = data;
}

/* The first sustained-Doppler-rate regression this object has ever had
 * (task #18/#93/#97): the full Acquisition -> carrier(bn_fll) -> Dll ->
 * RateConverter -> MpskReceiver chain, at SPEC's own real operating-point
 * RATIOS (`chip_rate=3.069e6`, `code_len=1023`, `symbol_rate=2700` --
 * CCSDS Gold-1023 @ 3.069 Mcps / async BPSK @ 2700 bps, `SPEC.md`'s own
 * numbers), against a 500 Hz/s ramp -- SPEC's corrected worst case -- must
 * stay locked and decode. Critically, `chip_rate/(code_len*symbol_rate)`
 * ~= 1.111 periods/symbol -- NOT an integer -- so the data-symbol clock is
 * genuinely ASYNCHRONOUS to the code-epoch clock (symbol boundaries drift
 * across period boundaries over the run, exactly the scenario `segments>1`
 * exists to handle; this project's whole "async despreader" story, tasks
 * #40-60). An earlier draft of this test picked `symbol_rate =
 * chip_rate/code_len` for convenience -- an exact 1 period/symbol, i.e. a
 * SYNCHRONOUS clock -- which would have proven the carrier-rate mechanism
 * survives in combination with a clock relationship SPEC doesn't actually
 * have, silently skipping the one dimension (async data) this whole
 * codebase's history says is the hard part. Fixed to use SPEC's real
 * ratio, caught by the user asking "have you tested the SPEC use-case with
 * async data?" before this was committed. cn0 is moderately (not
 * maximally) stressed: the SNR-floor behavior of the mechanism itself is
 * already covered by the Python floor sweep (FINISHING_PLAN.md) and the
 * isolated C composition test above; this test's own job is proving the
 * FULL composed receiver survives a sustained RATE *and* async data
 * end-to-end together, not re-proving the SNR floor. */
static int
_test_sustained_doppler_rate (void)
{
  int _fails = 0;

  const size_t sf        = 1023;
  const size_t spc       = 2;
  const double chip_rate = 3.069e6; /* SPEC.md's own Gold-1023 rate */
  const double fs        = chip_rate * (double)spc;
  const double sym_rate  = 2700.0; /* SPEC.md's own async BPSK rate --
                                       chip_rate/(sf*sym_rate) ~= 1.111
                                       periods/symbol, deliberately
                                       NOT an integer (see above). */
  const double tsym          = fs / sym_rate;
  const size_t te            = sf * spc;
  const double rate_hz_per_s = 500.0; /* SPEC's corrected worst case */
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
    code[i] = (uint8_t)(prbs (&cst) > 0 ? 0u : 1u);

  float complex *x;
  size_t         n;
  double        *data;
  _make_ramp_signal (code, sf, spc, fs, tsym, rate_hz_per_s, cn0, n_sym,
                     pre_silence, 21, &x, &n, &data);

  dsss_receiver_state_t *rx4 = dsss_receiver_create (
      code, sf, chip_rate, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 4, 8, 0);
  dsss_receiver_state_t *rx1 = dsss_receiver_create (
      code, sf, chip_rate, sym_rate, spc, 2, cn0, 1e-2, 0.9, 500.0, 1, 8, 0);
  CHECK (rx4 != NULL && rx1 != NULL);
  if (!rx4 || !rx1)
    {
      if (rx4)
        dsss_receiver_destroy (rx4);
      if (rx1)
        dsss_receiver_destroy (rx1);
      free (code);
      free (x);
      free (data);
      return _fails + 1;
    }

  float complex *syms4, *syms1;
  size_t         n_syms4 = _stream (rx4, x, n, te, &syms4);
  size_t         n_syms1 = _stream (rx1, x, n, te, &syms1);

  double ber4 = _best_ber (syms4, n_syms4, data, n_sym + 4);
  double ber1 = _best_ber (syms1, n_syms1, data, n_sym + 4);

  CHECK (dsss_receiver_get_tracking (rx4) == 1);
  /* A receiver that lost lock partway through (the ~117 Hz/s bare-PLL
   * cliff this session's own investigation found -- SPEC's 500 Hz/s is
   * ~4x past it) would emit far fewer symbols than the run's own length;
   * staying locked through most of it is the actual claim under test. */
  CHECK (n_syms4 > (n_sym / 2));
  CHECK (ber4 < 0.05);
  /* The async-lookback mechanism must be NEEDED, not incidental: on the
   * IDENTICAL genuinely-async (~1.111 periods/symbol) signal, `segments=1`
   * (no lookback -- a plain coherent full-epoch dump) must decode
   * measurably WORSE than `segments=4` (DsssReceiver's own default) --
   * proving the new pre-despread carrier composition still lets
   * `dll_steps()`'s segments>1 lookback do real, load-bearing work,
   * exactly as it did before this session's carrier-loop addition.
   * Confirmed directly: ber4=0.0000, ber1=0.1702 on the same run. */
  CHECK (ber1 > ber4);
  CHECK (ber1 > 0.05);

  free (syms4);
  free (syms1);
  free (x);
  free (data);
  free (code);
  dsss_receiver_destroy (rx4);
  dsss_receiver_destroy (rx1);
  return _fails;
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
  int _fails = 0;

  const size_t sf          = 7;
  const size_t spc         = 4;
  const double fs          = 1.0e6 * (double)spc;
  const double sym_rate    = 35714.29;
  const double tsym        = fs / sym_rate;
  const size_t te          = sf * spc;
  const size_t n_sym       = 300;
  const size_t pre_silence = te * 5 + 3;
  const double cn0         = 90.0;

  float complex *x;
  size_t         n;
  double        *data;
  _make_signal (CODE7, sf, spc, fs, tsym, 0.0, cn0, n_sym, pre_silence, 7, &x,
                &n, &data);

  dsss_receiver_state_t *rx = dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  CHECK (rx != NULL);
  if (!rx)
    {
      free (x);
      free (data);
      return _fails + 1;
    }

  size_t odd_chunk = te + te / 3; /* deliberately NOT a multiple
                                      of te (one code period). */
  float complex *warm_syms;
  size_t         half   = n / 2;
  size_t         warm_n = _stream (rx, x, half, odd_chunk, &warm_syms);
  free (warm_syms);
  (void)warm_n;
  CHECK (dsss_receiver_get_tracking (rx) == 1);
  /* Direct struct-field peek (not a public getter -- reasonable for a C
   * unit test with full struct visibility): confirms the test actually
   * exercises the nonzero-carry case it claims to, rather than silently
   * passing on a lucky zero. */
  CHECK (rx->car_carry_len > 0 && rx->car_carry_len < rx->tsamps);

  size_t cb   = dsss_receiver_state_bytes (rx);
  void  *blob = malloc (cb);
  dsss_receiver_get_state (rx, blob);

  dsss_receiver_state_t *rx2 = dsss_receiver_create (
      CODE7, sf, 1.0e6, sym_rate, spc, 2, 70.0, 1e-2, 0.9, 500.0, 4, 8, 0);
  CHECK (rx2 != NULL);
  if (rx2)
    {
      CHECK (dsss_receiver_set_state (rx2, blob) == DP_OK);
      CHECK (rx2->car_carry_len == rx->car_carry_len);

      float complex *rest1, *rest2;
      size_t         rest_n = n - half;
      size_t         k1 = _stream (rx, x + half, rest_n, odd_chunk, &rest1);
      size_t         k2 = _stream (rx2, x + half, rest_n, odd_chunk, &rest2);
      CHECK (k1 == k2);
      size_t kmin = k1 < k2 ? k1 : k2;
      for (size_t i = 0; i < kmin; i++)
        CHECK (cabsf (rest1[i] - rest2[i]) < 1e-4f);
      free (rest1);
      free (rest2);
      dsss_receiver_destroy (rx2);
    }
  free (blob);
  free (x);
  free (data);
  dsss_receiver_destroy (rx);
  return _fails;
}

int
main (void)
{
  int _fails = 0;
  _fails += _test_arg_validation ();
  _fails += _test_acquire_and_decode ();
  _fails += _test_carrier_dll_composition_ramp ();
  _fails += _test_sustained_doppler_rate ();
  _fails += _test_carry_buffer_state_roundtrip ();
  if (_fails)
    {
      fprintf (stderr, "test_dsss_receiver_core FAILED (%d)\n", _fails);
      return 1;
    }
  printf ("test_dsss_receiver_core PASSED\n");
  return 0;
}
