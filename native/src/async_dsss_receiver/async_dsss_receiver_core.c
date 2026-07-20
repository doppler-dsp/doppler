#include "async_dsss_receiver/async_dsss_receiver_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Largest divisor of sps in {4, 2, 1} -- MpskReceiver's own carrier-arm
 * count, mirroring dsss_receiver_core.c's own _derive_n(). */
static int
_derive_n (size_t sps)
{
  if (sps % 4 == 0)
    return 4;
  if (sps % 2 == 0)
    return 2;
  return 1;
}

/* Allocate a fresh refine-stage chain (frozen carrier + collection Dll +
 * RateConverter + CarrierAcquisition, plus their scratch buffers) from
 * the given hand-off phase/frequency, without touching `s`'s existing
 * children -- the fail-safe half of the "allocate everything first"
 * regrid discipline (dsss_receiver_core.c's own _build_chain is the
 * precedent). `s->refine_segments` is assumed already set (fixed for the
 * object's lifetime -- see async_dsss_receiver_create()). On any
 * failure, frees whatever it already allocated and returns -1. */
static int
_build_refine_chain (async_dsss_receiver_state_t *s, double chip_phase,
                     double doppler_hz_est, costas_state_t *car_frozen_out,
                     dll_state_t **refine_dll_out,
                     RateConverter_state_t **refine_rc_out,
                     carrier_acq_state_t **ca_out,
                     float complex **dll_out_buf_out, size_t *dll_out_cap_out,
                     float complex **rc_out_buf_out, size_t *rc_out_cap_out)
{
  /* Frozen carrier: costas_update() is never called on this instance --
   * the direct C equivalent of Python's freeze_carrier=True. bn/bn_fll/
   * tsamps below are inert placeholders; only costas_wipeoff()'s phase
   * accumulator is ever touched. */
  double front_end_rate = s->chip_rate * (double)s->spc;
  costas_init (car_frozen_out, ASYNC_DSSS_RX_BN_CARRIER, 0.707,
              doppler_hz_est / front_end_rate, s->tsamps,
              ASYNC_DSSS_RX_BN_FLL);

  dll_state_t *refine_dll
      = dll_create (s->code, s->code_len, s->spc, chip_phase,
                   ASYNC_DSSS_RX_DLL_BN, 0.707, 0.5, s->refine_segments);
  if (!refine_dll)
    return -1;

  double partial_rate
      = s->chip_rate * (double)s->refine_segments / (double)s->code_len;
  double target_rate = (double)s->refine_samples_per_symbol * s->symbol_rate;
  if (partial_rate <= 0.0 || target_rate <= 0.0)
    {
      dll_destroy (refine_dll);
      return -1;
    }

  RateConverter_state_t *refine_rc
      = RateConverter_create (target_rate / partial_rate, 0);
  if (!refine_rc)
    {
      dll_destroy (refine_dll);
      return -1;
    }

  /* design_snr/resolution_hz: freq_refine.refine_seed_carrier_acq()'s own
   * formula, ported verbatim (see objects/async_dsss_receiver.toml's
   * refine_design_margin_db doc comment for why this empirical derating
   * is used as-is rather than re-derived). */
  double effective_cn0_dbhz = s->cn0_dbhz - s->refine_design_margin_db;
  double design_snr
      = sqrt (pow (10.0, effective_cn0_dbhz / 10.0) / target_rate);
  double resolution_hz = target_rate / (double)s->refine_n_fft;

  carrier_acq_state_t *ca = carrier_acq_create (
      target_rate, s->symbol_rate, resolution_hz, s->refine_zero_pad,
      0 /* window=hann */, 0.0f, NULL, 0, s->pfa, s->pd, design_snr,
      s->refine_sequential, s->refine_max_n_blocks);
  if (!ca)
    {
      RateConverter_destroy (refine_rc);
      dll_destroy (refine_dll);
      return -1;
    }

  float complex *dll_out_buf
      = malloc (s->refine_segments * sizeof (*dll_out_buf));
  if (!dll_out_buf)
    {
      carrier_acq_destroy (ca);
      RateConverter_destroy (refine_rc);
      dll_destroy (refine_dll);
      return -1;
    }

  size_t rc_out_cap
      = (size_t)((double)s->refine_segments * refine_rc->rate) + 64;
  float complex *rc_out_buf = malloc (rc_out_cap * sizeof (*rc_out_buf));
  if (!rc_out_buf)
    {
      free (dll_out_buf);
      carrier_acq_destroy (ca);
      RateConverter_destroy (refine_rc);
      dll_destroy (refine_dll);
      return -1;
    }

  *refine_dll_out = refine_dll;
  *refine_rc_out  = refine_rc;
  *ca_out         = ca;
  *dll_out_buf_out = dll_out_buf;
  *dll_out_cap_out = s->refine_segments;
  *rc_out_buf_out  = rc_out_buf;
  *rc_out_cap_out  = rc_out_cap;
  return 0;
}

static void
_free_refine_chain (async_dsss_receiver_state_t *s)
{
  carrier_acq_destroy (s->ca);
  RateConverter_destroy (s->refine_rc);
  dll_destroy (s->refine_dll);
  free (s->refine_dll_out_buf);
  free (s->refine_rc_out_buf);
  s->ca               = NULL;
  s->refine_rc        = NULL;
  s->refine_dll       = NULL;
  s->refine_dll_out_buf = NULL;
  s->refine_rc_out_buf  = NULL;
}

/* Build a fresh refine chain and, only on success, swap it in for `s`'s
 * current one -- the "allocate everything first" half of the regrid
 * discipline applied at the call site. Also resets refine_samples_fed
 * and the shared carry buffer (fresh chain: no leftover partial-period
 * tail). Returns 0/-1 like _build_refine_chain. */
static int
_rebuild_refine_chain (async_dsss_receiver_state_t *s, double chip_phase,
                       double doppler_hz_est)
{
  costas_state_t         car_frozen;
  dll_state_t           *refine_dll = NULL;
  RateConverter_state_t *refine_rc  = NULL;
  carrier_acq_state_t   *ca         = NULL;
  float complex         *dll_out_buf = NULL;
  size_t                 dll_out_cap = 0;
  float complex         *rc_out_buf  = NULL;
  size_t                 rc_out_cap  = 0;

  if (_build_refine_chain (s, chip_phase, doppler_hz_est, &car_frozen,
                          &refine_dll, &refine_rc, &ca, &dll_out_buf,
                          &dll_out_cap, &rc_out_buf, &rc_out_cap)
      != 0)
    return -1;

  _free_refine_chain (s);
  s->car_frozen        = car_frozen;
  s->refine_dll        = refine_dll;
  s->refine_rc         = refine_rc;
  s->ca                = ca;
  s->refine_dll_out_buf = dll_out_buf;
  s->refine_dll_out_cap = dll_out_cap;
  s->refine_rc_out_buf  = rc_out_buf;
  s->refine_rc_out_cap  = rc_out_cap;
  s->refine_samples_fed = 0;
  s->car_carry_len      = 0;
  return 0;
}

/* Allocate a fresh live-tracking chain (Dll/RateConverter/MpskReceiver +
 * the pre-despread carrier loop), mirroring dsss_receiver_core.c's own
 * _build_chain, with ONE deliberate difference: costas_init()'s tsamps
 * here is the PER-PARTIAL interval (tsamps/segments), not the whole code
 * period -- costas_update() is called once per dll_steps()-emitted
 * partial directly (see _track_period()), matching car_update_
 * windows=True's per-window cadence validated in the Python prototype;
 * no combine-and-sign-align step. */
static int
_build_track_chain (async_dsss_receiver_state_t *s, double chip_phase,
                    double doppler_hz_est, size_t segments, size_t sps,
                    int n, costas_state_t *car_out, dll_state_t **dll_out,
                    RateConverter_state_t **rc_out,
                    mpsk_receiver_state_t **rx_out)
{
  double partial_rate = s->chip_rate * (double)segments / (double)s->code_len;
  double target_rate  = (double)sps * s->symbol_rate;
  if (partial_rate <= 0.0 || target_rate <= 0.0)
    return -1;

  dll_state_t *dll = dll_create (s->code, s->code_len, s->spc, chip_phase,
                                ASYNC_DSSS_RX_DLL_BN, 0.707, 0.5, segments);
  if (!dll)
    return -1;

  RateConverter_state_t *rc
      = RateConverter_create (target_rate / partial_rate, 0);
  if (!rc)
    {
      dll_destroy (dll);
      return -1;
    }

  /* MpskReceiver's own carrier loop is seeded at 0, NOT doppler_hz_est
   * again -- same fix, same reasoning, as dsss_receiver_core.c's own
   * _build_chain() (see its comment): the pre-despread Costas loop below
   * is the stage responsible for removing the FULL physical Doppler, so
   * by the time a sample reaches MpskReceiver only a small residual
   * should remain. Re-seeding MpskReceiver with the full doppler_hz_est
   * (evaluated at target_rate=sps*symbol_rate) is a double count that
   * aliases past Nyquist at large offsets (e.g. 15000/21600=0.694
   * cycles/sample -> ~-0.306 after wrap), far outside MpskReceiver's own
   * carrier_nda pull-in range -- confirmed as the direct cause of total
   * lock failure on a real static-offset case (task #148), fixed here. */
  mpsk_receiver_state_t *rx = mpsk_receiver_create (
      s->m, sps, n, MPSK_RX_PULSE_IANDD, 0.35, 8, 0.01, 0.707, 0.01, 1, 0.3,
      0.0, 30, s->differential);
  if (!rx)
    {
      RateConverter_destroy (rc);
      dll_destroy (dll);
      return -1;
    }

  /* Per-CODE-PERIOD cadence (tsamps = one whole period), matching
   * dsss_receiver_core.c's own already-validated mechanism -- NOT the
   * per-partial cadence this object originally tried. Direct
   * measurement (this object's own SPEC-scenario test) found per-
   * partial cadence (costas_update() called once per dll_steps()
   * partial, at the SAME bn_fll) does NOT track SPEC's 500Hz/s ramp:
   * k_fll = 4*bn_fll is a FIXED gain per call, but the FLL cross-
   * product discriminator's own output scales with the TIME interval
   * between the two prompts it correlates -- calling it 4x more often
   * (this object's segments=4) shrinks that interval 4x, weakening the
   * discriminator's real error signal at a fixed gain rather than
   * proportionally speeding up tracking. This reproduces (and now
   * empirically confirms, not just repeats unverified) the OLD
   * DsssReceiver's own retired code comment claiming per-partial
   * cadence "measurably made tracking WORSE" -- see _track_period()'s
   * own combine-and-sign-align step below, which restores the proven
   * per-period cadence. */
  double front_end_rate = s->chip_rate * (double)s->spc;
  costas_init (car_out, ASYNC_DSSS_RX_BN_CARRIER, 0.707,
              doppler_hz_est / front_end_rate, s->tsamps,
              ASYNC_DSSS_RX_BN_FLL);

  *dll_out = dll;
  *rc_out  = rc;
  *rx_out  = rx;
  return 0;
}

static void
_free_track_chain (async_dsss_receiver_state_t *s)
{
  mpsk_receiver_destroy (s->rx);
  RateConverter_destroy (s->rc);
  dll_destroy (s->dll);
  s->rx  = NULL;
  s->rc  = NULL;
  s->dll = NULL;
}

static int
_rebuild_track_chain (async_dsss_receiver_state_t *s, double chip_phase,
                      double doppler_hz_est, size_t segments, size_t sps,
                      int n)
{
  costas_state_t         car;
  dll_state_t           *dll = NULL;
  RateConverter_state_t *rc  = NULL;
  mpsk_receiver_state_t *rx  = NULL;
  if (_build_track_chain (s, chip_phase, doppler_hz_est, segments, sps, n,
                          &car, &dll, &rc, &rx)
      != 0)
    return -1;
  _free_track_chain (s);
  s->car           = car;
  s->dll           = dll;
  s->rc            = rc;
  s->rx            = rx;
  s->segments      = segments;
  s->sps           = sps;
  s->n             = n;
  s->car_carry_len = 0;
  return 0;
}

/* One frozen-carrier-wiped code period through refine_dll -> refine_rc ->
 * CarrierAcquisition. `period` must be exactly s->tsamps samples.
 * carrier_acq_steps() is documented as a no-op once ready (or the
 * give-up cap) is reached, so calling it past that point is harmless.
 *
 * `refine_dll`'s own `segments` MUST stay 1 for this collection to be
 * usable at all -- dll_core.h's own doc on the `segments` parameter
 * draws the line explicitly: `segments=1` is "a coherent full-epoch
 * integrate-and-dump," `segments>1`'s own natural, unshifted per-chunk
 * OUTPUT (dll_core.c's own segments>1 emit-block comment) carries no
 * epoch-to-epoch sign guarantee at all -- a real data-bit transition
 * landing inside a natural window is exactly what the lookback search
 * exists to detect, and the two halves of such an epoch genuinely carry
 * two different bits with two different signs (see _track_period()'s/
 * _carrier_update_from_partials()'s own per-partial sign-alignment,
 * needed there for exactly that reason -- a per-EPOCH decision cannot
 * substitute for it, confirmed directly: task #151's investigation tried
 * exactly that at the dll_core.c primitive level and it corrupted real
 * decoded data). Carrier Acquisition needs the opposite of any of this:
 * phase-coherent samples across the WHOLE collection window to FFT. No
 * amount of post-processing on segments>1's own output can retrofit that
 * kind of coherence onto a stream whose natural chunking was never
 * designed to preserve it; `refine_max_error_db`'s own dll_lookback_
 * segments() derivation can in principle pick segments>1 for a large
 * enough tolerance, but doing so here would silently hand this
 * collection a stream this coherent estimator can't use correctly.
 * `async_dsss_receiver_create()`'s own `refine_max_error_db=100.0`
 * default keeps this at segments=1 today for exactly this reason --
 * deliberately, not merely because segments>1 "measured worse." */
static void
_refine_period (async_dsss_receiver_state_t *s, const float complex *period)
{
  for (size_t i = 0; i < s->tsamps; i++)
    s->car_wiped_buf[i] = costas_wipeoff (&s->car_frozen, period[i]);

  size_t n_dll = dll_steps (s->refine_dll, s->car_wiped_buf, s->tsamps,
                           s->refine_dll_out_buf, s->refine_dll_out_cap);
  if (n_dll == 0)
    return;

  size_t n_rc = RateConverter_execute (s->refine_rc, s->refine_dll_out_buf,
                                       n_dll, s->refine_rc_out_buf,
                                       s->refine_rc_out_cap);
  if (n_rc == 0)
    return;

  carrier_acq_steps (s->ca, s->refine_rc_out_buf, n_rc);
}

/* Refine-stage entry point: buffer raw samples into whole `tsamps`-sample
 * periods (the shared car_carry_buf -- refine and track never run
 * concurrently) and run _refine_period() on each complete one. */
static void
_process_refine (async_dsss_receiver_state_t *s, const float complex *x,
                 size_t x_len)
{
  size_t pos = 0;

  if (s->car_carry_len > 0)
    {
      size_t need = s->tsamps - s->car_carry_len;
      size_t take = (need <= x_len) ? need : x_len;
      memcpy (s->car_carry_buf + s->car_carry_len, x, take * sizeof (*x));
      s->car_carry_len += take;
      pos = take;
      if (s->car_carry_len < s->tsamps)
        return;
      _refine_period (s, s->car_carry_buf);
      s->car_carry_len = 0;
    }

  while (pos + s->tsamps <= x_len)
    {
      _refine_period (s, x + pos);
      pos += s->tsamps;
    }

  size_t leftover = x_len - pos;
  if (leftover > 0)
    memcpy (s->car_carry_buf, x + pos, leftover * sizeof (*x));
  s->car_carry_len = leftover;
}

/* One carrier-wiped code period through dll -> ONE costas_update() call
 * per period, on a DATA-WIPED sum of that period's emitted partials --
 * dsss_receiver_core.c's own proven _carrier_update_from_partials()
 * mechanism, ported verbatim (per-partial cadence was tried and found to
 * NOT track SPEC's 500Hz/s ramp, see _build_track_chain()'s own comment).
 * Sign-aligning each partial by its own real-part sign before adding
 * avoids the bit-transition self-cancellation a bare coherent sum would
 * suffer.
 *
 * Confirmed load-bearing FOUR separate times across task #151's
 * investigation now, on grounds that held up under direct re-measurement
 * every time (see git history / project memory CHECKPOINT 37 for the
 * first three). The fourth: dll_steps()'s own segments>1 emit block was
 * tried carrying the winning lookback candidate's own coherent sign
 * through to its OUTPUT (chunk_p[]), on the theory that it's a higher-
 * SNR, one-shot decision than this per-partial real-part guess and could
 * replace it. That output is not internal scratch, though -- it is the
 * actual despread symbol stream this object also hands to RateConverter/
 * mpsk_receiver for demodulation, and dsss_receiver_core.c's own
 * analogous chain confirmed the fix corrupted real data outright
 * (`test_dsss_receiver.py::test_acquires_and_decodes`: clean decode ->
 * ber=0.465) whenever the natural epoch genuinely straddles a data-bit
 * transition -- exactly the case a shifted candidate winning is meant to
 * detect, where the two halves of that epoch carry two REAL, different
 * bits and a single epoch-wide sign silently overwrites one of them.
 * Reverted at the primitive; this per-partial decision-directed guess
 * (the same technique costas_update()'s own bn_fll cross-product term
 * already applies between consecutive symbols: "both prompts are
 * data-wiped [by their own Re sign] so a BPSK bit flip ... does not
 * corrupt the cross product", costas_core.h) stays the only sign source
 * for THIS internal carrier-tracking sum -- a value that, unlike
 * dll_steps()'s own output, is never handed onward as data. Costas'
 * phase/frequency discriminator only cares about the sum's relative
 * alignment to the carrier, not an absolute data-bit reference, so this
 * data-wiping is exactly as safe here as it already is inside
 * costas_update() itself. Writes emitted partials into dll_out (capacity
 * max_out); returns count. */
static size_t
_track_period (async_dsss_receiver_state_t *s, const float complex *period,
              float complex *dll_out, size_t max_out)
{
  for (size_t i = 0; i < s->tsamps; i++)
    s->car_wiped_buf[i] = costas_wipeoff (&s->car, period[i]);
  size_t n_out
      = dll_steps (s->dll, s->car_wiped_buf, s->tsamps, dll_out, max_out);
  if (n_out > 0)
    {
      float complex sum = 0.0f;
      for (size_t i = 0; i < n_out; i++)
        {
          float sign = (crealf (dll_out[i]) >= 0.0f) ? 1.0f : -1.0f;
          sum += dll_out[i] * sign;
        }
      costas_update (&s->car, sum);
    }
  return n_out;
}

static size_t
_track_carrier_dll (async_dsss_receiver_state_t *s, const float complex *x,
                    size_t x_len, float complex *dll_out, size_t max_out)
{
  size_t emitted = 0;
  size_t pos     = 0;

  if (s->car_carry_len > 0)
    {
      size_t need = s->tsamps - s->car_carry_len;
      size_t take = (need <= x_len) ? need : x_len;
      memcpy (s->car_carry_buf + s->car_carry_len, x, take * sizeof (*x));
      s->car_carry_len += take;
      pos = take;
      if (s->car_carry_len < s->tsamps)
        return 0;
      emitted += _track_period (s, s->car_carry_buf, dll_out + emitted,
                                max_out - emitted);
      s->car_carry_len = 0;
    }

  while (pos + s->tsamps <= x_len)
    {
      emitted += _track_period (s, x + pos, dll_out + emitted,
                                max_out - emitted);
      pos += s->tsamps;
    }

  size_t leftover = x_len - pos;
  if (leftover > 0)
    memcpy (s->car_carry_buf, x + pos, leftover * sizeof (*x));
  s->car_carry_len = leftover;

  return emitted;
}

static size_t
_track_chain (async_dsss_receiver_state_t *s, const float complex *x,
             size_t x_len, float complex *out, size_t max_out)
{
  if (x_len == 0)
    return 0;

  float complex *dll_out = malloc (x_len * sizeof *dll_out);
  if (!dll_out)
    return 0;
  size_t n_dll = _track_carrier_dll (s, x, x_len, dll_out, x_len);

  size_t         rc_cap = (size_t)((double)n_dll * s->rc->rate) + 64;
  float complex *rc_out = malloc (rc_cap * sizeof *rc_out);
  if (!rc_out)
    {
      free (dll_out);
      return 0;
    }
  size_t n_rc = RateConverter_execute (s->rc, dll_out, n_dll, rc_out, rc_cap);
  free (dll_out);

  size_t n_out = mpsk_receiver_steps (s->rx, rc_out, n_rc, out, max_out);
  free (rc_out);
  return n_out;
}

async_dsss_receiver_state_t *
async_dsss_receiver_create (const uint8_t *code, size_t code_len,
                            double chip_rate, double symbol_rate,
                            size_t spc, int m, double cn0_dbhz, double pfa,
                            double pd, double doppler_uncertainty,
                            size_t segments, size_t sps, int differential,
                            double refine_max_error_db,
                            size_t refine_samples_per_symbol,
                            double refine_design_margin_db,
                            size_t refine_n_fft, size_t refine_zero_pad,
                            bool refine_sequential,
                            size_t refine_max_n_blocks)
{
  if (!code || code_len < 1 || chip_rate <= 0.0 || symbol_rate <= 0.0
      || spc < 1 || (m != 2 && m != 4 && m != 8) || segments < 1 || sps < 1
      || refine_samples_per_symbol < 1 || refine_n_fft < 1
      || refine_zero_pad < 1)
    return NULL;

  async_dsss_receiver_state_t *obj = calloc (1, sizeof (*obj));
  if (!obj)
    return NULL;

  obj->code = malloc (code_len);
  if (!obj->code)
    {
      free (obj);
      return NULL;
    }
  memcpy (obj->code, code, code_len);
  obj->code_len = code_len;

  obj->acq = acq_create_continuous (obj->code, code_len, spc, chip_rate,
                                    symbol_rate, cn0_dbhz,
                                    doppler_uncertainty, pfa, pd,
                                    0 /* noise_mode=mean */);
  if (!obj->acq)
    {
      free (obj->code);
      free (obj);
      return NULL;
    }

  obj->spc          = spc;
  obj->m            = m;
  obj->differential = differential;
  obj->chip_rate    = chip_rate;
  obj->symbol_rate  = symbol_rate;
  obj->cn0_dbhz     = cn0_dbhz;
  obj->pfa          = pfa;
  obj->pd           = pd;
  obj->state        = 0;

  obj->refine_max_error_db       = refine_max_error_db;
  obj->refine_samples_per_symbol = refine_samples_per_symbol;
  obj->refine_design_margin_db   = refine_design_margin_db;
  obj->refine_n_fft              = refine_n_fft;
  obj->refine_zero_pad           = refine_zero_pad;
  obj->refine_sequential         = refine_sequential;
  obj->refine_max_n_blocks       = refine_max_n_blocks;

  /* tsamps (one code period, samples) is fixed for this object's entire
   * lifetime -- refine_segments is likewise fixed (depends only on
   * tsamps/refine_max_error_db, both construction-time invariants), so
   * both the shared carry scratch and refine_segments are computed ONCE
   * here, never per-rebuild. */
  obj->tsamps        = code_len * spc;
  obj->refine_segments
      = dll_lookback_segments (obj->tsamps, refine_max_error_db);
  obj->car_wiped_buf = malloc (obj->tsamps * sizeof (*obj->car_wiped_buf));
  obj->car_carry_buf = malloc (obj->tsamps * sizeof (*obj->car_carry_buf));
  if (!obj->car_wiped_buf || !obj->car_carry_buf)
    {
      free (obj->car_wiped_buf);
      free (obj->car_carry_buf);
      acq_destroy (obj->acq);
      free (obj->code);
      free (obj);
      return NULL;
    }
  obj->car_carry_len = 0;

  /* Placeholder chains (phase 0, no Doppler) -- always allocated, seeded
   * for real the moment a hit fires (fixed shape, same rationale
   * dsss_receiver_core.c's own state struct doc comment gives). */
  if (_build_refine_chain (obj, 0.0, 0.0, &obj->car_frozen, &obj->refine_dll,
                          &obj->refine_rc, &obj->ca, &obj->refine_dll_out_buf,
                          &obj->refine_dll_out_cap, &obj->refine_rc_out_buf,
                          &obj->refine_rc_out_cap)
      != 0)
    {
      free (obj->car_wiped_buf);
      free (obj->car_carry_buf);
      acq_destroy (obj->acq);
      free (obj->code);
      free (obj);
      return NULL;
    }
  obj->refine_samples_fed = 0;

  if (_build_track_chain (obj, 0.0, 0.0, segments, sps, _derive_n (sps),
                         &obj->car, &obj->dll, &obj->rc, &obj->rx)
      != 0)
    {
      _free_refine_chain (obj);
      free (obj->car_wiped_buf);
      free (obj->car_carry_buf);
      acq_destroy (obj->acq);
      free (obj->code);
      free (obj);
      return NULL;
    }
  obj->segments = segments;
  obj->sps      = sps;
  obj->n        = _derive_n (sps);
  obj->car_carry_len = 0; /* both placeholder builds share this buffer */

  obj->seed_chip_phase     = 0.0;
  obj->seed_doppler_hz_est = 0.0;
  obj->doppler_hz_est      = 0.0;
  obj->cn0_dbhz_est        = 0.0;
  obj->samples_fed         = 0;
  return obj;
}

void
async_dsss_receiver_destroy (async_dsss_receiver_state_t *state)
{
  if (!state)
    return;
  _free_track_chain (state);
  _free_refine_chain (state);
  free (state->car_wiped_buf);
  free (state->car_carry_buf);
  acq_destroy (state->acq);
  free (state->code);
  free (state);
}

void
async_dsss_receiver_reset (async_dsss_receiver_state_t *state)
{
  acq_reset (state->acq);
  /* Best-effort: on OOM, leave the current chains in place rather than
   * signal a failure this void-returning lifecycle function can't report
   * (matches dsss_receiver_reset()'s own contract). */
  _rebuild_refine_chain (state, 0.0, 0.0);
  _rebuild_track_chain (state, 0.0, 0.0, state->segments, state->sps,
                       state->n);
  state->state             = 0;
  state->seed_chip_phase   = 0.0;
  state->seed_doppler_hz_est = 0.0;
  state->doppler_hz_est    = 0.0;
  state->cn0_dbhz_est      = 0.0;
  state->samples_fed       = 0;
}

size_t
async_dsss_receiver_steps_max_out (async_dsss_receiver_state_t *state)
{
  (void)state;
  return 0; /* caller falls back to allocating x_len -- always sufficient,
               symbols emitted <= raw samples in. */
}

size_t
async_dsss_receiver_steps (async_dsss_receiver_state_t *state,
                          const float complex *x, size_t x_len,
                          float complex *out, size_t max_out)
{
  if (x_len == 0)
    return 0;

  if (state->state == 0) /* searching */
    {
      uint64_t     before = state->samples_fed;
      acq_result_t hit;
      state->samples_fed += x_len;
      size_t n_hits = acq_push (state->acq, x, x_len, &hit, 1);
      if (n_hits == 0)
        return 0;

      /* Same "exact unconsumed tail of THIS call" technique
       * dsss_receiver_core.c's own steps() uses. */
      uint64_t total_after = before + (uint64_t)x_len;
      uint64_t consumed    = state->acq->samples_consumed;
      uint64_t tail64
          = (total_after > consumed) ? (total_after - consumed) : 0;
      size_t tail_len = (tail64 > (uint64_t)x_len) ? x_len : (size_t)tail64;
      const float complex *tail = x + (x_len - tail_len);

      acq_handoff_t ho;
      acq_build_handoff (state->acq, &hit, state->code_len, state->spc, &ho);

      if (_rebuild_refine_chain (state, ho.chip_phase, ho.doppler_hz_est)
          != 0)
        return 0; /* stay searching; try again on the next hit */

      state->state              = 1; /* refining */
      state->seed_chip_phase    = ho.chip_phase;
      state->seed_doppler_hz_est = ho.doppler_hz_est;
      state->doppler_hz_est     = ho.doppler_hz_est;
      state->cn0_dbhz_est       = ho.cn0_dbhz_est;

      return async_dsss_receiver_steps (state, tail, tail_len, out, max_out);
    }

  if (state->state == 1) /* refining */
    {
      uint64_t before = state->refine_samples_fed;
      state->refine_samples_fed += x_len;
      _process_refine (state, x, x_len);

      carrier_acq_state_t *ca  = state->ca;
      size_t cap = ca->sequential ? ca->max_n_blocks : ca->dwell_target;
      if (!ca->ready && ca->n_blocks < cap)
        return 0; /* still refining, no output yet */

      double refined_doppler_hz_est
          = state->seed_doppler_hz_est
            + (ca->ready ? ca->residual_hz : 0.0); /* give-up: unrefined */

      /* samples_consumed_refine: freq_refine.refine_seed_carrier_acq()'s
       * own elapsed-time formula, ported verbatim -- a resampled chain
       * has no exact raw-sample-to-block correspondence to track through
       * RateConverter's own filter delay; this approximation is already
       * validated to ~tens-of-Hz accuracy at SPEC's own real rate (see
       * the plan doc). Uses `refine_n_fft` (the RAW per-block sample
       * count `carrier_acq_create()` consumes per `n_blocks` increment)
       * -- NOT `ca->nfft` (the zero-padded PSD *transform* length,
       * `next_pow2(refine_n_fft*refine_zero_pad)`, a different and much
       * larger number). */
      double target_rate = (double)state->refine_samples_per_symbol
                           * state->symbol_rate;
      double elapsed_s = (double)ca->n_blocks
                         * (double)state->refine_n_fft / target_rate;
      double front_end_rate = state->chip_rate * (double)state->spc;
      uint64_t samples_consumed_refine
          = (uint64_t)llround (elapsed_s * front_end_rate);

      /* Round UP to a whole number of code periods (tsamps) -- critical,
       * not cosmetic: the live tracking chain below is seeded with
       * `seed_chip_phase`, the code phase at the ORIGINAL handoff, not
       * wherever the refine-stage Dll's own tracking drifted to. That
       * reuse is only valid if the live chain's first sample is an EXACT
       * whole number of code periods after the handoff -- one whole
       * period is by definition zero net code-phase advance, so the
       * phase at any such boundary equals the phase at the handoff
       * itself. Python's own e2e_acq_to_despreader.py relies on exactly
       * this (`n_epochs_used`'s own ceiling-to-whole-epoch rounding
       * before slicing `track_rx`); omitting it here was a real bug --
       * confirmed directly: without this rounding, the live chain is
       * seeded with a code phase valid for a DIFFERENT (non-epoch-
       * aligned) stream position than the one it's actually fed,
       * misaligning Dll's correlation from the very first sample
       * (total decode failure, reproduced even at Es/N0=30dB where
       * noise cannot be the explanation). */
      uint64_t n_periods_consumed
          = (samples_consumed_refine + (uint64_t)state->tsamps - 1)
            / (uint64_t)state->tsamps;
      samples_consumed_refine = n_periods_consumed * (uint64_t)state->tsamps;

      /* Only a tail within THIS call's own x can be safely recovered --
       * a prior call's buffer is already gone (the same rule
       * dsss_receiver_core.c's own search->track transition follows).
       * If the estimate implies more was unused than this call
       * provided, there's nothing left to recover; start tracking from
       * zero backlog rather than guess. */
      uint64_t fed_total = before + (uint64_t)x_len;
      uint64_t unused
          = (fed_total > samples_consumed_refine)
                ? (fed_total - samples_consumed_refine)
                : 0;
      size_t                tail_len = (unused > (uint64_t)x_len)
                                          ? 0
                                          : (size_t)unused;
      const float complex *tail     = x + (x_len - tail_len);

      if (_rebuild_track_chain (state, state->seed_chip_phase,
                               refined_doppler_hz_est, state->segments,
                               state->sps, state->n)
          != 0)
        return 0; /* stay refining; ca is already at its ready/give-up
                     state, so this retries every subsequent call until
                     an allocation succeeds -- an OOM-only degenerate
                     path, matching dsss_receiver_core.c's own rebuild-
                     failure handling. */

      state->state         = 2; /* tracking */
      state->doppler_hz_est = refined_doppler_hz_est;

      return async_dsss_receiver_steps (state, tail, tail_len, out, max_out);
    }

  return _track_chain (state, x, x_len, out, max_out);
}

int
async_dsss_receiver_configure_search_raw (async_dsss_receiver_state_t *state,
                                          size_t doppler_bins,
                                          size_t n_noncoh)
{
  return acq_configure_search_raw (state->acq, doppler_bins, n_noncoh);
}

void
async_dsss_receiver_configure_lock_raw (async_dsss_receiver_state_t *state,
                                       double up_thresh, double down_thresh,
                                       size_t n_looks, double alpha,
                                       uint32_t n_up, uint32_t n_down)
{
  dll_configure_lock_raw (state->dll, up_thresh, down_thresh, n_looks, alpha,
                         n_up, n_down);
}

int
async_dsss_receiver_configure_chain_raw (async_dsss_receiver_state_t *state,
                                        size_t segments, size_t sps, int n)
{
  if (segments < 1 || sps < 1 || n < 1 || (int)(sps % (size_t)n) != 0)
    return -1;

  double chip_phase      = dll_get_code_phase (state->dll);
  double old_target_rate = (double)state->sps * state->symbol_rate;
  double doppler_hz_now
      = mpsk_receiver_get_norm_freq (state->rx) * old_target_rate;

  return _rebuild_track_chain (state, chip_phase, doppler_hz_now, segments,
                              sps, n);
}

int
async_dsss_receiver_get_tracking (const async_dsss_receiver_state_t *state)
{
  return state->state == 2;
}
int
async_dsss_receiver_get_refining (const async_dsss_receiver_state_t *state)
{
  return state->state == 1;
}
double
async_dsss_receiver_get_doppler_hz (const async_dsss_receiver_state_t *state)
{
  return state->doppler_hz_est;
}
double
async_dsss_receiver_get_cn0_dbhz_est (
    const async_dsss_receiver_state_t *state)
{
  return state->cn0_dbhz_est;
}
size_t
async_dsss_receiver_get_segments (const async_dsss_receiver_state_t *state)
{
  return state->segments;
}
size_t
async_dsss_receiver_get_sps (const async_dsss_receiver_state_t *state)
{
  return state->sps;
}
int
async_dsss_receiver_get_n (const async_dsss_receiver_state_t *state)
{
  return state->n;
}
double
async_dsss_receiver_get_chip_phase (const async_dsss_receiver_state_t *state)
{
  return dll_get_code_phase (state->dll);
}
double
async_dsss_receiver_get_code_rate (const async_dsss_receiver_state_t *state)
{
  return dll_get_code_rate (state->dll);
}
double
async_dsss_receiver_get_lock (const async_dsss_receiver_state_t *state)
{
  return mpsk_receiver_get_lock (state->rx);
}
double
async_dsss_receiver_get_norm_freq (const async_dsss_receiver_state_t *state)
{
  return mpsk_receiver_get_norm_freq (state->rx);
}

size_t
async_dsss_receiver_state_bytes (const async_dsss_receiver_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (async_dsss_receiver_extra_t)
         + acq_state_bytes (s->acq) + costas_state_bytes (&s->car_frozen)
         + dll_state_bytes (s->refine_dll)
         + RateConverter_state_bytes (s->refine_rc)
         + carrier_acq_state_bytes (s->ca) + costas_state_bytes (&s->car)
         + dll_state_bytes (s->dll) + RateConverter_state_bytes (s->rc)
         + mpsk_receiver_state_bytes (s->rx)
         + s->tsamps * sizeof (float _Complex);
}

void
async_dsss_receiver_get_state (const async_dsss_receiver_state_t *s,
                              void *blob)
{
  DP_GET_OPEN (ASYNC_DSSS_RECEIVER_STATE_MAGIC,
              ASYNC_DSSS_RECEIVER_STATE_VERSION,
              async_dsss_receiver_state_bytes (s));
  async_dsss_receiver_extra_t extra = {
    .state               = (uint8_t)s->state,
    .seed_chip_phase     = s->seed_chip_phase,
    .seed_doppler_hz_est = s->seed_doppler_hz_est,
    .doppler_hz_est      = s->doppler_hz_est,
    .cn0_dbhz_est        = s->cn0_dbhz_est,
    .segments            = (uint64_t)s->segments,
    .sps                 = (uint64_t)s->sps,
    .n                   = (uint64_t)s->n,
    .refine_segments     = (uint64_t)s->refine_segments,
    .refine_samples_fed  = s->refine_samples_fed,
    .car_carry_len       = (uint64_t)s->car_carry_len,
  };
  dp_w_bytes (&_w, &extra, sizeof extra);
  DP_W_CHILD (&_w, acq, s->acq);
  DP_W_CHILD (&_w, costas, &s->car_frozen);
  DP_W_CHILD (&_w, dll, s->refine_dll);
  DP_W_CHILD (&_w, RateConverter, s->refine_rc);
  DP_W_CHILD (&_w, carrier_acq, s->ca);
  DP_W_CHILD (&_w, costas, &s->car);
  DP_W_CHILD (&_w, dll, s->dll);
  DP_W_CHILD (&_w, RateConverter, s->rc);
  DP_W_CHILD (&_w, mpsk_receiver, s->rx);
  dp_w_cf32 (&_w, s->car_carry_buf, s->tsamps);
}

int
async_dsss_receiver_set_state (async_dsss_receiver_state_t *s,
                              const void *blob)
{
  DP_SET_OPEN (ASYNC_DSSS_RECEIVER_STATE_MAGIC,
              ASYNC_DSSS_RECEIVER_STATE_VERSION,
              async_dsss_receiver_state_bytes (s));
  async_dsss_receiver_extra_t extra;
  dp_r_bytes (&_r, &extra, sizeof extra);
  if (extra.segments != (uint64_t)s->segments || extra.sps != (uint64_t)s->sps
      || extra.n != (uint64_t)s->n
      || extra.refine_segments != (uint64_t)s->refine_segments
      || extra.car_carry_len > (uint64_t)s->tsamps)
    return DP_ERR_INVALID;
  DP_R_CHILD (&_r, acq, s->acq);
  DP_R_CHILD (&_r, costas, &s->car_frozen);
  DP_R_CHILD (&_r, dll, s->refine_dll);
  DP_R_CHILD (&_r, RateConverter, s->refine_rc);
  DP_R_CHILD (&_r, carrier_acq, s->ca);
  DP_R_CHILD (&_r, costas, &s->car);
  DP_R_CHILD (&_r, dll, s->dll);
  DP_R_CHILD (&_r, RateConverter, s->rc);
  DP_R_CHILD (&_r, mpsk_receiver, s->rx);
  dp_r_cf32 (&_r, s->car_carry_buf, s->tsamps);
  s->state               = extra.state;
  s->seed_chip_phase     = extra.seed_chip_phase;
  s->seed_doppler_hz_est = extra.seed_doppler_hz_est;
  s->doppler_hz_est      = extra.doppler_hz_est;
  s->cn0_dbhz_est        = extra.cn0_dbhz_est;
  s->refine_samples_fed  = extra.refine_samples_fed;
  s->car_carry_len       = (size_t)extra.car_carry_len;
  return DP_OK;
}
