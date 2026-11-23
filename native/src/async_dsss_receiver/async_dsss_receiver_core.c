#include "doppler/async_dsss_receiver/async_dsss_receiver_core.h"
#include "doppler/async_dsss_receiver/async_dsss_receiver_private.h"
#include "doppler/detection/detection_core.h"
#include "doppler/util/util_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Defined below, beside the refine chain that uses it. */
static dp_carrier_acq_state_t *
adr_new_carrier_acq (const dp_async_dsss_receiver_state_t *s,
                     double                                target_rate);

/* The tracker's symbol period in Dll partials: `segments` partials a code
   period, chip_rate / code_len periods a second, symbol_rate symbols. One
   formula for the build site that sets the period and the checks that
   vouch for it. */
static double
adr_partials_per_symbol (size_t segments, double chip_rate, size_t code_len,
                         double symbol_rate)
{
  return (double)segments * chip_rate / ((double)code_len * symbol_rate);
}

/* Whether a tracker chain at `segments` partials and `sps` samples a symbol
   can be built. Its Dll's arguments are inside dp_dll_params_ok(). sps >= 2,
   since sps = 1 carries no m_out (dsss_receiver_core.c, gh-782). The symbol
   is no faster than a chip, and at most 2^20 partials long at ANY segments:
   that keeps the chain's resampling ratio, sps over the partials, in
   [sps / 2^20, sps * code_len / segments], where a tiny symbol rate had
   underflowed it to 0 and a huge one sized the scratch at ~4e12 samples, both
   aborting. Under 2 partials the tracker keeps per-partial looks; any longer
   period the Dll's aid takes. create() and configure_chain_raw() both refuse
   outside it, so the chain built mid-stream under dp_xnn never meets a
   refusal (doppler#2103). */
static int
adr_track_chain_ok (size_t code_len, double chip_rate, double symbol_rate,
                    size_t segments, size_t sps, double spc)
{
  const double partials
      = adr_partials_per_symbol (segments, chip_rate, code_len, symbol_rate);
  return sps >= 2
         /* the Dll's samples a chip, not the SymbolSync's sps a symbol */
         && dp_dll_params_ok (code_len, spc, 0.0, ASYNC_DSSS_RX_DLL_BN,
                              ASYNC_DSSS_RX_DLL_ZETA,
                              ASYNC_DSSS_RX_DLL_SPACING, segments)
         && symbol_rate <= chip_rate && partials <= DLL_AID_MAX_PERIOD
         && (segments < 2 || partials < 2.0
             || (dp_dll_symbol_period_ok (segments, partials)
                 /* set_symbol_period() also refuses a bandwidth whose gains
                    overflow at the period's interval: the same check here,
                    so the hand-off's set can never be refused (#2103). */
                 && dp_loop_filter_params_ok (ASYNC_DSSS_RX_DLL_BN,
                                              ASYNC_DSSS_RX_DLL_ZETA,
                                              partials / (double)segments)));
}

/* MpskReceiver's terminal outputs per symbol (`m_out`) for a chain at
   `sps`: the one rule, mpsk_rx_derive_m_out() (mpsk_rx_loops.h), on the
   complex face's inclusive bound, as DsssReceiver and MpskReceiver itself
   derive it. Even, in [2, 8], since create() refuses sps < 2. */
static int
adr_m_out (size_t sps)
{
  return (int)mpsk_rx_derive_m_out ((double)sps, 0);
}

/* Reset the symbol-lock detector's RUNNING state (config -- alpha, lockdet
 * thresholds/counts -- is set once in create()). Called on every fresh
 * track-chain build so each pass starts unlocked. */
static void
adr_reset_lock (dp_async_dsss_receiver_state_t *s)
{
  s->lock_num    = 0.0;
  s->lock_den    = 0.0;
  s->lock_metric = 0.0;
  dp_lockdet_reset (&s->sym_lockdet);
}

/* The code phase the live chain starts from: the seed's, advanced by the
 * clock dilation the Doppler implies over the refine (samples_consumed_refine
 * samples). Off when carrier_freq_hz == 0: no carrier, no dilation. The one
 * formula the hand-off uses, for the refined value and the seed fallback. */
static double
adr_handover_phase (const dp_async_dsss_receiver_state_t *state,
                    double doppler_hz_est, uint64_t samples_consumed_refine)
{
  /* The nominal advance over the refine first: zero mod code_len when the
     refine consumed whole periods of a whole-sample period (the integer
     case, exactly as before), and the leftover fraction of a period when a
     period is not a whole number of samples. */
  const double chips_elapsed = (double)samples_consumed_refine / state->spc;
  /* Reduced first, so whole periods give exactly 0.0 and the seed passes
     through bit for bit. */
  double phase = dp_fmod_pos (
      state->seed_chip_phase
          + dp_fmod_pos (chips_elapsed, (double)state->code_len),
      (double)state->code_len);
  if (state->carrier_freq_hz > 0.0)
    {
      double dilation = doppler_hz_est / state->carrier_freq_hz;
      phase           = dp_fmod_pos (phase + dilation * chips_elapsed,
                                     (double)state->code_len);
    }
  return phase;
}

/* A tracking receiver's code phase at the stream's next sample. The Dll's
 * own phase is at the next sample IT runs, and the live chain runs whole
 * tsamps-sample periods, carrying the rest (car_carry_buf), so the stream's
 * next sample is car_carry_len past it. Fed whole periods (as the pool
 * feeds it) at a whole-sample period the carry is 0; at a fractional period
 * it is a constant remainder, up to a whole period -- at 2.2 samples a chip,
 * 2,140 samples, which read ~50 chips off and let the pool's search re-seed
 * every held signal (#3063) -- and fed arbitrary blocks it is anything.
 * Carried at the rate the code NCO runs, the loop's rate plus the carrier
 * aid (dll_core.h: code_rate excludes the aid). */
static double
adr_live_chip_phase (const dp_async_dsss_receiver_state_t *s)
{
  const double ahead = (double)s->car_carry_len / s->spc
                       * (dp_dll_get_code_rate (s->dll) + s->dll->rate_aid);
  return dp_fmod_pos (dp_dll_get_code_phase (s->dll) + ahead,
                      (double)s->code_len);
}

/* Every state transition goes through here so the two running clocks stay
 * honest: `state_samples` counts from the entry, and the release clock
 * (`both_down_samples`) only ever runs inside tracking. */
static void
adr_enter (dp_async_dsss_receiver_state_t *s, int state)
{
  s->state         = state;
  s->state_samples = 0;
  /* Lost KEEPS the release clock and goes on counting it (the status
   * record's both_down_samples: "in lost it keeps counting -- samples since
   * the flags dropped"); every other entry restarts it. */
  if (state != ASYNC_DSSS_RX_LOST)
    s->both_down_samples = 0;
}

/* The carrier loop's design point and block lock, from the design C/N0.
 * gamma is the update's SNR, C/N0 over a code period. The loop's jitter is
 * the unit denominator's: its error, divided by the slope, has variance
 * (1 + 1/(2 gamma)) / (2 gamma) an update, and the closed loop passes 2 bn
 * of it (pin 18 of test_carrier_nda_core.c measures that law), so
 * phase_var = bn (1 + 1/(2 gamma)) / gamma: 1/200 rad^2 at the rule's bn.
 * The count is the least whose ratio of sums detects at Pd
 * ASYNC_DSSS_RX_CAR_LOCK_PD against the exact threshold at Pfa
 * ASYNC_DSSS_RX_CAR_LOCK_PFA. */
static double
adr_carrier_gamma (double cn0_dbhz, double chip_rate, size_t code_len)
{
  return pow (10.0, cn0_dbhz / 10.0) * (double)code_len / chip_rate;
}

uint32_t
dp_async_dsss_receiver_carrier_lock_n_noncoh (double cn0_dbhz,
                                              double chip_rate,
                                              size_t code_len)
{
  const double g = adr_carrier_gamma (cn0_dbhz, chip_rate, code_len);
  if (!(isfinite (g) && g > 0.0))
    return 0u;
  const double bn
      = dp_async_dsss_receiver_carrier_bn (cn0_dbhz, chip_rate, code_len);
  const double var = bn * (1.0 + 1.0 / (2.0 * g)) / g;
  const int    b = dp_det_ratio_block_len (g, var, ASYNC_DSSS_RX_CAR_LOCK_PFA,
                                           ASYNC_DSSS_RX_CAR_LOCK_PD,
                                           (int)CARRIER_NDA_BLOCK_MAX_LEN);
  return b > 0 ? (uint32_t)b : 0u;
}

/* The fine check's threshold on PolynomialPhaseEstimator's snr_db (the
   peak's dB less the spectrum's mean dB) over n sums: the peak must clear
   the largest of n exponential noise bins at pfa, t = ln(n / pfa) bin
   means, and an exponential bin's mean dB sits 10 gamma_E / ln 10 (2.51)
   dB under the dB of its mean. */
static double
adr_fine_thr_db (size_t n, double pfa)
{
  return 10.0 * log10 (log ((double)n / pfa))
         + 10.0 * 0.57721566490153286 / log (10.0);
}

/* The Kaiser window PolynomialPhaseEstimator applies (50 dB), at n points:
   its peak-bin share p = (sum w)^2 / (n sum w^2) and the frequency
   estimate's variance over the rectangular bound, eta = sum w^2 m^2 (n^3 /
   12) / (sum w m^2)^2, with m the centred index. */
static void
adr_fine_window (size_t n, double *p, double *eta)
{
  float *w = dp_xmallocn (n, sizeof *w);
  dp_kaiser_window (w, n, (float)dp_kaiser_beta_for_sidelobe (50.0));
  double sw = 0.0, sw2 = 0.0, sw2m2 = 0.0, swm2 = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      const double m = (double)i - 0.5 * (double)(n - 1), wi = (double)w[i];
      sw += wi;
      sw2 += wi * wi;
      sw2m2 += wi * wi * m * m;
      swm2 += wi * m * m;
    }
  free (w);
  *p   = sw * sw / ((double)n * sw2);
  *eta = sw2m2 * ((double)n * (double)n * (double)n / 12.0) / (swm2 * swm2);
}

/* The window's coherent loss for a tone chirping at r cycles a sample
   squared over n samples, dB: |sum w e^{j pi r m^2}|^2 / (sum w)^2 with m
   centred, where the even phase puts the peak at the centre frequency. */
static double
adr_fine_chirp_loss_db (size_t n, double r)
{
  float *w = dp_xmallocn (n, sizeof *w);
  dp_kaiser_window (w, n, (float)dp_kaiser_beta_for_sidelobe (50.0));
  double re = 0.0, im = 0.0, sw = 0.0;
  for (size_t i = 0; i < n; i++)
    {
      const double m = (double)i - 0.5 * (double)(n - 1);
      re += (double)w[i] * cos (M_PI * r * m * m);
      im += (double)w[i] * sin (M_PI * r * m * m);
      sw += (double)w[i];
    }
  free (w);
  return -10.0 * log10 ((re * re + im * im) / (sw * sw));
}

/* Does a dwell of d blocks meet p_ho at one squared sum's SNR gs? With the
   rate axis off (max_rate 0) a tone chirping at r loses the window's chirp
   loss at that length; with it on the estimator searches max_rate and its
   rows join the union and the check. Fills out's dwell fields when it
   does. */
static int
adr_fine_meets (size_t d, int k, double T, double gs, double r,
                double max_rate, double block_s, double pfa, double p_ho,
                int m, async_dsss_receiver_fine_t *out)
{
  const size_t n_s = (size_t)llround ((double)k * (double)d * block_s / T);
  if (n_s < 4)
    return 0;
  size_t n_rate = 1;
  if (max_rate > 0.0)
    {
      /* The estimator's own grid at this length. */
      dp_ppe_state_t *p = dp_ppe_create (n_s, max_rate);
      if (!p)
        return 0;
      n_rate = p->n_rate;
      dp_ppe_destroy (p);
    }
  double pw, eta;
  adr_fine_window (n_s, &pw, &eta);
  const double loss_db
      = max_rate == 0.0 && r > 0.0 ? adr_fine_chirp_loss_db (n_s, r) : 0.0;
  const double rho   = (double)n_s * gs * pow (10.0, -loss_db / 10.0);
  const double cells = (double)n_s * (double)n_rate;
  const double t     = log (cells / pfa);
  const double p_w   = 0.5 * (cells - 1.0) * exp (-pw * rho / 2.0);
  const double miss
      = 1.0 - dp_marcum_q (1, sqrt (2.0 * pw * rho), sqrt (2.0 * t));
  if (p_w + miss > 1.0 - p_ho)
    return 0;
  const double dur = (double)n_s * T / (double)k;
  out->blocks      = d;
  out->n_s         = n_s;
  out->n_rate      = n_rate;
  out->rho         = rho;
  out->p_w         = p_w;
  out->p_miss      = miss;
  out->thr_db      = adr_fine_thr_db ((size_t)cells, pfa);
  out->sd_hz = sqrt (6.0 * eta) / (2.0 * M_PI * dur * sqrt (rho)) / (double)m;
  return 1;
}

/* The least dwell that meets p_ho, up to max_blocks; 0 if none. Without a
   chirp loss the rates fall as the dwell grows, so the search doubles to a
   dwell that meets and bisects for the least. With one (the axis off at a
   nonzero rate) the loss can outgrow the gain, so it scans, and only up
   to max_blocks. */
static size_t
adr_fine_dwell (int k, double T, double gs, double r, double max_rate,
                double block_s, double pfa, double p_ho, size_t max_blocks,
                int m, async_dsss_receiver_fine_t *out)
{
  if (max_rate == 0.0 && r > 0.0)
    {
      for (size_t d = 1; d <= max_blocks; d++)
        if (adr_fine_meets (d, k, T, gs, r, max_rate, block_s, pfa, p_ho, m,
                            out))
          return d;
      return 0;
    }
  size_t hi = 1;
  while (
      !adr_fine_meets (hi, k, T, gs, r, max_rate, block_s, pfa, p_ho, m, out))
    {
      if (hi >= max_blocks)
        return 0;
      hi = hi > max_blocks / 2 ? max_blocks : 2 * hi;
    }
  size_t lo = hi / 2; /* fails, or 0 */
  while (hi - lo > 1)
    {
      const size_t               mid = lo + (hi - lo) / 2;
      async_dsss_receiver_fine_t tmp = *out;
      if (adr_fine_meets (mid, k, T, gs, r, max_rate, block_s, pfa, p_ho, m,
                          &tmp))
        hi = mid;
      else
        lo = mid;
    }
  (void)adr_fine_meets (hi, k, T, gs, r, max_rate, block_s, pfa, p_ho, m, out);
  return hi;
}

int
dp_async_dsss_receiver_fine_design (
    double cn0_dbhz, double chip_rate, size_t code_len, double p_flip,
    double symbol_rate, int m, size_t refine_segments, double e_hz,
    double rate_hz_s, double eps_chip, double block_s, double pfa, double p_ho,
    size_t max_blocks, async_dsss_receiver_fine_t *out)
{
  if (m != 2 || !isfinite (cn0_dbhz) || !(chip_rate > 0.0) || code_len == 0
      || !(p_flip >= 0.0 && p_flip <= 1.0) || !(symbol_rate > 0.0)
      || refine_segments == 0 || !(e_hz >= 0.0) || !isfinite (e_hz)
      || !(rate_hz_s >= 0.0) || !isfinite (rate_hz_s)
      || !(eps_chip >= 0.0 && eps_chip < 1.0) || !(block_s > 0.0)
      || !(pfa > 0.0 && pfa < 1.0) || !(p_ho > 0.0 && p_ho < 1.0))
    return DP_ERR_INVALID;
  const double T = (double)code_len / chip_rate;
  int          k = 0;
  for (size_t d = 1; d <= refine_segments; d++)
    if (refine_segments % d == 0 && (double)d / (2.0 * m * T) > e_hz)
      {
        k = (int)d;
        break;
      }
  if (!k)
    return DP_ERR_INVALID;
  const double gamma = pow (10.0, cn0_dbhz / 10.0) * T;
  const double rho_r = 1.0 - p_flip / 4.0, kap = 1.0 - p_flip / 3.0;
  const double x    = e_hz * T / (double)k;
  const double sinc = x == 0.0 ? 1.0 : sin (M_PI * x) / (M_PI * x);
  const double g = gamma / (double)k * rho_r * rho_r / kap * (1.0 - eps_chip)
                   * (1.0 - eps_chip) * sinc * sinc;
  const double q = T * symbol_rate / (double)k;
  const double c = 1.0 - q / 3.0, v = (1.0 - 0.4 * q) - c * c;
  const double gs = c * c * g * g / (4.0 * g + 2.0 + v * g * g);
  out->k          = k;
  out->g          = g;
  out->gamma_sq   = gs;
  /* The rate-free dwell's chirp loss, reported; then the two ways to
     meet p_ho under the rate: the axis off, the loss taken at each length
     (A), or the axis on with its rows (B). The shorter dwell is the
     design; a tie keeps the axis off. */
  const double               r = (double)m * rate_hz_s * (T / k) * (T / k);
  async_dsss_receiver_fine_t a = *out, bb = *out; /* k, g, gamma_sq */
  if (!adr_fine_dwell (k, T, gs, 0.0, 0.0, block_s, pfa, p_ho, max_blocks, m,
                       out))
    return DP_ERR_INVALID;
  out->chirp_loss_db = r > 0.0 ? adr_fine_chirp_loss_db (out->n_s, r) : 0.0;
  out->max_rate      = 0.0;
  if (r == 0.0)
    return DP_OK;
  /* B first: past its dwell A cannot win, so A scans only that far. */
  const size_t db = adr_fine_dwell (k, T, gs, r, r, block_s, pfa, p_ho,
                                    max_blocks, m, &bb);
  const size_t da = adr_fine_dwell (k, T, gs, r, 0.0, block_s, pfa, p_ho,
                                    db ? db : max_blocks, m, &a);
  if (!da && !db)
    return DP_ERR_INVALID;
  const double chirp = out->chirp_loss_db;
  if (da && (!db || da <= db))
    {
      *out          = a;
      out->max_rate = 0.0;
      out->n_rate   = 1;
    }
  else
    {
      *out          = bb;
      out->max_rate = r;
    }
  out->chirp_loss_db = chirp;
  return DP_OK;
}

double
dp_async_dsss_receiver_carrier_bn (double cn0_dbhz, double chip_rate,
                                   size_t code_len)
{
  /* rho = C/N0 T, T one code period: the update's coherent SNR. The rule's
     loop SNR rho S_L / (2 bn) at ASYNC_DSSS_RX_CARRIER_LOOP_SNR_DB is the
     ceiling, counting the squaring loss S_L = 1 / (1 + 1/(2 rho)) of the
     M = 2 detector (the owner's ruling, 2026-11-22); the release's
     bandwidth is the widest taken. */
  const double rho
      = pow (10.0, cn0_dbhz / 10.0) * (double)code_len / chip_rate;
  const double bn
      = rho / (1.0 + 1.0 / (2.0 * rho))
        / (2.0 * pow (10.0, ASYNC_DSSS_RX_CARRIER_LOOP_SNR_DB / 10.0));
  return bn < ASYNC_DSSS_RX_BN_CARRIER ? bn : ASYNC_DSSS_RX_BN_CARRIER;
}

/* Allocate a fresh refine-stage chain (frozen carrier + collection Dll +
 * RateConverter + CarrierAcquisition, plus their scratch buffers) from
 * the given hand-off phase/frequency, without touching `s`'s existing
 * children -- the fail-safe half of the "allocate everything first"
 * regrid discipline (dsss_receiver_core.c's own dsss_rx_build_chain is the
 * precedent). `s->refine_segments` is assumed already set (fixed for the
 * object's lifetime -- see dp_async_dsss_receiver_create()). The sub-object
 * allocations are small and internal, so they are trusted (the house
 * "no error handling for impossible scenarios" rule) -- this build cannot
 * fail. */
static void
adr_build_refine_chain (
    dp_async_dsss_receiver_state_t *s, double chip_phase,
    double doppler_hz_est, dp_costas_state_t *car_frozen_out,
    dp_dll_state_t **refine_dll_out, dp_RateConverter_state_t **refine_rc_out,
    dp_carrier_acq_state_t **ca_out, float _Complex **dll_out_buf_out,
    size_t *dll_out_cap_out, float _Complex **rc_out_buf_out,
    size_t *rc_out_cap_out)
{
  /* Frozen carrier: costas_update() is never called on this instance --
   * the direct C equivalent of Python's freeze_carrier=True. bn/tsamps
   * below are inert placeholders; only costas_wipeoff()'s phase accumulator
   * is ever touched. bn_fll = 0: no FLL anywhere in this object (see the
   * ASYNC_DSSS_RX_BN_CARRIER comment). */
  double front_end_rate = s->chip_rate * (double)s->spc;
  dp_costas_init (car_frozen_out, s->bn_carrier, 0.707,
                  doppler_hz_est / front_end_rate, s->tsamps, 0.0);

  dp_dll_state_t *refine_dll = dp_xnn (
      dp_dll_create (s->code, s->code_len, s->spc, chip_phase,
                     ASYNC_DSSS_RX_DLL_BN, ASYNC_DSSS_RX_DLL_ZETA,
                     ASYNC_DSSS_RX_REFINE_DLL_SPACING, s->refine_segments));

  /* Carrier->code aiding on the collection Dll too: without it the coupled
     code-rate Doppler drifts the code phase over the frozen refine prefix
     (~55 chips at a 50 kHz offset / 2.5 GHz over ~0.9 s), degrading the very
     despread stream CarrierAcquisition estimates from. Seeded off the coarse
     handoff Doppler (the refined value isn't known yet); the ~1 kHz coarse
     residual leaves only a small aiding error. Off when carrier_freq_hz==0. */
  if (s->carrier_freq_hz > 0.0)
    dp_dll_set_rate_aid (refine_dll, doppler_hz_est / s->carrier_freq_hz);

  double partial_rate
      = s->chip_rate * (double)s->refine_segments / (double)s->code_len;
  double target_rate = (double)s->refine_samples_per_symbol * s->symbol_rate;

  dp_RateConverter_state_t *refine_rc
      = dp_xnn (dp_RateConverter_create (target_rate / partial_rate, 0));

  /* design_snr/resolution_hz: freq_refine.refine_seed_carrier_acq()'s own
   * formula, ported verbatim (see objects/async_dsss_receiver.toml's
   * refine_design_margin_db doc comment for why this empirical derating
   * is used as-is rather than re-derived). */
  dp_carrier_acq_state_t *ca = adr_new_carrier_acq (s, target_rate);

  float _Complex *dll_out_buf
      = dp_xmallocn (s->refine_segments, sizeof (*dll_out_buf));

  size_t rc_out_cap
      = (size_t)((double)s->refine_segments * refine_rc->rate) + 64;
  float _Complex *rc_out_buf = dp_xmallocn (rc_out_cap, sizeof (*rc_out_buf));

  *refine_dll_out  = refine_dll;
  *refine_rc_out   = refine_rc;
  *ca_out          = ca;
  *dll_out_buf_out = dll_out_buf;
  *dll_out_cap_out = s->refine_segments;
  *rc_out_buf_out  = rc_out_buf;
  *rc_out_cap_out  = rc_out_cap;
}

static void
adr_free_refine_chain (dp_async_dsss_receiver_state_t *s)
{
  dp_carrier_acq_destroy (s->ca);
  dp_RateConverter_destroy (s->refine_rc);
  dp_dll_destroy (s->refine_dll);
  free (s->refine_dll_out_buf);
  free (s->refine_rc_out_buf);
  s->ca                 = NULL;
  s->refine_rc          = NULL;
  s->refine_dll         = NULL;
  s->refine_dll_out_buf = NULL;
  s->refine_rc_out_buf  = NULL;
}

/* Build a fresh refine chain and, only on success, swap it in for `s`'s
 * current one -- the "allocate everything first" half of the regrid
 * discipline applied at the call site. Also resets refine_samples_fed
 * and the shared carry buffer (fresh chain: no leftover partial-period
 * tail). Cannot fail (the sub-object allocations are trusted). */
static int adr_tlm_attach (dp_async_dsss_receiver_state_t *s);

static void
adr_rebuild_refine_chain (dp_async_dsss_receiver_state_t *s, double chip_phase,
                          double doppler_hz_est)
{
  dp_costas_state_t         car_frozen;
  dp_dll_state_t           *refine_dll  = NULL;
  dp_RateConverter_state_t *refine_rc   = NULL;
  dp_carrier_acq_state_t   *ca          = NULL;
  float _Complex           *dll_out_buf = NULL;
  size_t                    dll_out_cap = 0;
  float _Complex           *rc_out_buf  = NULL;
  size_t                    rc_out_cap  = 0;

  adr_build_refine_chain (s, chip_phase, doppler_hz_est, &car_frozen,
                          &refine_dll, &refine_rc, &ca, &dll_out_buf,
                          &dll_out_cap, &rc_out_buf, &rc_out_cap);

  adr_free_refine_chain (s);
  s->car_frozen         = car_frozen;
  s->refine_dll         = refine_dll;
  s->refine_rc          = refine_rc;
  s->ca                 = ca;
  s->refine_dll_out_buf = dll_out_buf;
  s->refine_dll_out_cap = dll_out_cap;
  s->refine_rc_out_buf  = rc_out_buf;
  s->refine_rc_out_cap  = rc_out_cap;
  s->refine_samples_fed = 0;
  s->refine_periods     = 0;
  s->car_carry_len      = 0;
  s->refine_hist_len    = 0;
  s->refine_hist_head   = 0;
  s->fine_n             = 0;
  s->ho_src             = ASYNC_DSSS_RX_HO_NONE;
  /* The new estimators report under the old names, as the track chain's
     loops do after their rebuild. */
  if (s->tlm)
    (void)adr_tlm_attach (s);
}

/* Allocate a fresh live-tracking chain (Dll/RateConverter/MpskReceiver +
 * the pre-despread carrier loop). The carrier loop is carrier_nda at M = 2,
 * its update period one code period: carrier_nda_update() runs once a
 * period on the Dll's window sum (see adr_track_period()), not once a
 * partial, so each update integrates the whole period. */
static void
adr_build_track_chain (dp_async_dsss_receiver_state_t *s, double chip_phase,
                       double doppler_hz_est, size_t segments, size_t sps,
                       int n, dp_carrier_nda_state_t *car_out,
                       dp_dll_state_t           **dll_out,
                       dp_RateConverter_state_t **rc_out,
                       dp_mpsk_receiver_state_t **rx_out)
{
  double partial_rate = s->chip_rate * (double)segments / (double)s->code_len;
  double target_rate  = (double)sps * s->symbol_rate;

  dp_dll_state_t *dll = dp_xnn (dp_dll_create (
      s->code, s->code_len, s->spc, chip_phase, ASYNC_DSSS_RX_DLL_BN,
      ASYNC_DSSS_RX_DLL_ZETA, ASYNC_DSSS_RX_DLL_SPACING, segments));
  /* The lock detector's looks, sized for THIS operating point rather than
     left at the DLL's default 20 partials: the symbol period in partials is
     known from configuration, so the detector's looks are the symbol-scale
     max-power windows (dp_dll_set_symbol_period, docs/design/
     async-dsss-receiver.md §3.7), and n_looks is det_n_noncoh over the
     window's coherent length at the design C/N0. Measured before this: at
     Es/N0 5.7 dB the default detector read "unlocked" 96% of the time on a
     loop that never lost the code (§12.3). A period under 2 partials (a
     very fast data clock) keeps per-partial looks and only sizes them. */
  if (segments > 1)
    {
      double partials_per_symbol = adr_partials_per_symbol (
          segments, s->chip_rate, s->code_len, s->symbol_rate);
      /* Under 2 partials the tracker keeps per-partial looks; any longer
         period adr_track_chain_ok() vouched for, at create() and in
         configure_chain_raw(), so a refusal here is a defect, not a
         configuration. */
      if (partials_per_symbol >= 2.0
          && dp_dll_set_symbol_period (dll, partials_per_symbol) != DP_OK)
        abort ();
      size_t win  = dp_dll_get_symbol_window (dll);
      size_t look = (win ? win : 1) * (s->tsamps / segments);
      double amp
          = dp_det_cn0_to_snr (s->cn0_dbhz, s->chip_rate * (double)s->spc);
      int nl = dp_det_n_noncoh (amp, (int)look, 0.99, 1e-3, 4000);
      if (nl >= 1)
        {
          (void)dp_dll_configure_lock (dll, 1e-3, (size_t)nl, 0.0);
          /* The drop count from the miss probability the sizing bought
             (1 - 0.99) at a 1e-6 per-decision false-drop budget -- three,
             against configure_lock's fixed two. */
          (void)dp_dll_set_lock_verify (
              dll, dll->lock.n_up, (uint32_t)dp_det_verify_count (0.01, 1e-6));
        }
    }

  /* Carrier->code aiding: the code-rate Doppler is coupled to the carrier
     offset through the same v/c, so feed the refined carrier estimate into
     the code NCO's rate. The discriminator alone cannot pull in that
     constant rate offset at low SNR (~24.6 chips/s at 20 kHz / 2.5 GHz), so
     unaided the code loop walks off even with a correct carrier. Off when
     carrier_freq_hz == 0 (a pure baseband offset with no clock dilation). */
  if (s->carrier_freq_hz > 0.0)
    dp_dll_set_rate_aid (dll, doppler_hz_est / s->carrier_freq_hz);

  dp_RateConverter_state_t *rc
      = dp_xnn (dp_RateConverter_create (target_rate / partial_rate, 0));

  /* MpskReceiver's own carrier loop is seeded at 0, NOT doppler_hz_est
   * again -- same reasoning as dsss_receiver_core.c's own
   * dsss_rx_build_chain(): the pre-despread carrier loop below already removes
   * the FULL physical Doppler, so only a small residual should reach
   * MpskReceiver. Re-seeding it with the full doppler_hz_est (evaluated at
   * target_rate=sps*symbol_rate, a much smaller rate than the front end)
   * double-counts and can alias past Nyquist at large offsets, far
   * outside MpskReceiver's own carrier_nda pull-in range. */
  /* ASYNC_DSSS_RX_DEMOD_BN_TIMING 0.005, matching dsss_receiver_core.c's
     own dsss_rx_build_chain -- see the measurement table there. Short
     version: the rebuild's timing
     loop steers RateSync's accumulator rather than a Farrow interpolator, 0.01
     is too wide for that on a despread stream, and the value is chosen on
     steady-state EVM rather than BER (BPSK's BER saturates first, and the
     BER-optimal 0.001 is ~8 dB worse in EVM). */
  /* `n` is inside dp_ratesync_m_ok(): create() derives it
     (mpsk_rx_derive_m_out) and configure_chain_raw() refuses outside it, so
     this create is never refused on `n` (doppler#2421). */
  dp_mpsk_receiver_state_t *rx = dp_xnn (dp_mpsk_receiver_create (
      s->m, (double)sps, (size_t)n, MPSK_RX_PULSE_IANDD,
      ASYNC_DSSS_RX_DEMOD_RRC_BETA, ASYNC_DSSS_RX_DEMOD_RRC_SPAN,
      ASYNC_DSSS_RX_DEMOD_BN_CARRIER, ASYNC_DSSS_RX_DEMOD_ZETA,
      ASYNC_DSSS_RX_DEMOD_BN_TIMING, ASYNC_DSSS_RX_DEMOD_LOCK_THRESH, 0.0,
      s->differential, MPSK_RX_NUM_PHASES, 1, MPSK_RX_AGC_BW_RATIO));

  /* Per-CODE-PERIOD cadence (tsamps = one whole period), matching
   * dsss_receiver_core.c's own mechanism -- NOT once per dp_dll_steps()
   * partial. The carrier discriminator combines the whole period's coherent-
   * I&D partials (a non-data-aided squaring combine, adr_track_period()) into
   * one full-period error; a per-partial cadence would shrink each update's
   * integration by `segments`, too weak/noisy to steer a clean carrier
   * estimate. Pure PLL, no FLL (bn_fll = 0 below). */
  double front_end_rate = s->chip_rate * (double)s->spc;
  /* carrier_nda at M = 2 through its composition API: the receiver wipes
     each sample (carrier_nda_wipeoff) and hands it the Dll's window sum
     once a period (carrier_nda_update), so its update period is the
     period's samples, and its arm (sps 8, n 1) is never stepped. The unit
     denominator divides the error by the window sum's signal power,
     averaged at ASYNC_DSSS_RX_CAR_DEN_ALPHA less the Dll's window noise,
     clamped below at the design point's share (car_gamma). The lock is the
     ratio of sums of I^2 - Q^2 over I^2 + Q^2 on blocks of car_blk
     updates, at Pfa ASYNC_DSSS_RX_CAR_LOCK_PFA, with the code lock's
     verify counts. Every option is a valid configuration create()
     vouched for, so a refusal here is a defect. */
  dp_carrier_nda_init (car_out, s->bn_carrier, 0.707,
                       doppler_hz_est / front_end_rate, 8, 1, 2);
  if (dp_carrier_nda_set_update_period (car_out, (double)s->tsamps) != DP_OK
      || dp_carrier_nda_set_denominator (car_out, ASYNC_DSSS_RX_CAR_DEN_ALPHA,
                                         s->car_gamma)
             != DP_OK
      || (s->car_blk
          && dp_carrier_nda_set_block_lock (
                 car_out, s->car_blk, ASYNC_DSSS_RX_CAR_LOCK_PFA,
                 (uint32_t)dp_det_verify_count (ASYNC_DSSS_RX_CAR_LOCK_PFA,
                                                1e-6),
                 (uint32_t)dp_det_verify_count (0.01, 1e-6))
                 != DP_OK))
    abort ();

  *dll_out = dll;
  *rc_out  = rc;
  *rx_out  = rx;
}

static void
adr_free_track_chain (dp_async_dsss_receiver_state_t *s)
{
  dp_mpsk_receiver_destroy (s->rx);
  dp_RateConverter_destroy (s->rc);
  dp_dll_destroy (s->dll);
  s->rx  = NULL;
  s->rc  = NULL;
  s->dll = NULL;
}

/* Attach the stored telemetry request (tlm, tlm_prefix, tlm_decim) to every
 * loop the receiver holds right now. set_telemetry calls it, and so does
 * each track-chain rebuild: a seed destroys the Dll, the resampler and the
 * symbol receiver and builds new ones, which would otherwise come up
 * detached. Every name is joined before the first registration, so an
 * overlong prefix is refused with the registry untouched; a child that
 * cannot register fails the whole attach. car_held is attached beside car
 * under the same names, so the same ids: the hold restores car from
 * car_held on entering a coast, and refreshes car_held only while both
 * flags are up, so an attach made with one flag down would otherwise be
 * undone by the next coast -- and the detach clears both, or that restore
 * would hand the carrier a context the caller may have destroyed. The
 * refine stage's chain is not instrumented. */
static int
adr_tlm_attach (dp_async_dsss_receiver_state_t *s)
{
  dp_tlm_t   *t = s->tlm;
  const char *p = s->tlm_prefix;
  uint32_t    d = s->tlm_decim;
  char        car[DP_TLM_NAME_MAX], code[DP_TLM_NAME_MAX], rc[DP_TLM_NAME_MAX],
      demod[DP_TLM_NAME_MAX], acq[DP_TLM_NAME_MAX], sym_lock[DP_TLM_NAME_MAX],
      sym_locked[DP_TLM_NAME_MAX], st[DP_TLM_NAME_MAX],
      sym_num[DP_TLM_NAME_MAX], sym_den[DP_TLM_NAME_MAX],
      code_steer[DP_TLM_NAME_MAX], sync[DP_TLM_NAME_MAX],
      handover[DP_TLM_NAME_MAX], refused[DP_TLM_NAME_MAX],
      ref[DP_TLM_NAME_MAX], ph_car[DP_TLM_NAME_MAX], ph_code[DP_TLM_NAME_MAX],
      cn0[DP_TLM_NAME_MAX], ho_src[DP_TLM_NAME_MAX], had_lock[DP_TLM_NAME_MAX],
      coast[DP_TLM_NAME_MAX], hold[DP_TLM_NAME_MAX], release[DP_TLM_NAME_MAX],
      fine_hz[DP_TLM_NAME_MAX], fine_snr[DP_TLM_NAME_MAX],
      fine_n[DP_TLM_NAME_MAX];
  if (dp_tlm_name_join (car, p, "car") != DP_OK
      || dp_tlm_name_join (code, p, "code") != DP_OK
      || dp_tlm_name_join (rc, p, "rc") != DP_OK
      || dp_tlm_name_join (demod, p, "demod") != DP_OK
      || dp_tlm_name_join (acq, p, "acq") != DP_OK
      || dp_tlm_name_join (sym_lock, p, "sym.lock") != DP_OK
      || dp_tlm_name_join (sym_locked, p, "sym.locked") != DP_OK
      || dp_tlm_name_join (sym_num, p, "sym.num") != DP_OK
      || dp_tlm_name_join (sym_den, p, "sym.den") != DP_OK
      || dp_tlm_name_join (code_steer, p, "code.steer") != DP_OK
      || dp_tlm_name_join (sync, p, "demod.sync") != DP_OK
      || dp_tlm_name_join (st, p, "state") != DP_OK
      || dp_tlm_name_join (handover, p, "handover_hz") != DP_OK
      || dp_tlm_name_join (refused, p, "refused") != DP_OK
      || dp_tlm_name_join (ref, p, "ref") != DP_OK
      || dp_tlm_name_join (ph_car, p, "phase.car") != DP_OK
      || dp_tlm_name_join (ph_code, p, "phase.code") != DP_OK
      || dp_tlm_name_join (cn0, p, "cn0") != DP_OK
      || dp_tlm_name_join (ho_src, p, "ho_src") != DP_OK
      || dp_tlm_name_join (had_lock, p, "had_lock") != DP_OK
      || dp_tlm_name_join (coast, p, "coast") != DP_OK
      || dp_tlm_name_join (hold, p, "hold") != DP_OK
      || dp_tlm_name_join (release, p, "release") != DP_OK
      || dp_tlm_name_join (fine_hz, p, "ref_fine.hz") != DP_OK
      || dp_tlm_name_join (fine_snr, p, "ref_fine.snr_db") != DP_OK
      || dp_tlm_name_join (fine_n, p, "ref_fine.n") != DP_OK)
    return DP_ERR_INVALID;
  /* The searcher first: it is the one child whose attach allocates (a
     scratch context of its own, checked before it registers anything), so
     an out-of-memory refusal comes before any loop has put a name on the
     caller's table (#2846). The loops after it only register. */
  int r
      = s->acq ? dp_correlator_set_telemetry (s->acq, t, acq, d, NULL) : DP_OK;
  if (r == DP_OK)
    r = dp_carrier_nda_set_telemetry (&s->car, t, car, d);
  if (r == DP_OK)
    r = dp_carrier_nda_set_telemetry (&s->car_held, t, car, d);
  if (r == DP_OK)
    r = dp_dll_set_telemetry (s->dll, t, code, d);
  /* The code loop's filter at its own cadence, each steer of the symbol
     aid (async-dsss-receiver-measurements.md §4.11 (as planned), B). */
  if (r == DP_OK)
    r = dp_dll_set_steer_telemetry (s->dll, t, code_steer, d);
  if (r == DP_OK)
    r = dp_RateConverter_set_telemetry (s->rc, t, rc, d);
  if (r == DP_OK)
    r = dp_mpsk_receiver_set_telemetry (s->rx, t, demod, d);
  /* The timing loop's two filter terms, beside its six probes. */
  if (r == DP_OK)
    r = dp_ratesync_loop_set_filter_telemetry (&s->rx->l.timing, t, sync, d);
  /* The refine's estimator: each test beside its threshold, while
     refining (dp_carrier_acq_set_telemetry()). */
  if (r == DP_OK)
    r = dp_carrier_acq_set_telemetry (s->ca, t, ref, d);
  /* The receiver's own decision, the one its users act on: the symbol-lock
     metric and the lockdet's flag, once per emitted symbol. */
  if (r == DP_OK)
    {
      s->tlm_sym_lock   = dp_tlm_probe (t, sym_lock, d);
      s->tlm_sym_locked = dp_tlm_probe (t, sym_locked, d);
      s->tlm_sym_num    = dp_tlm_probe (t, sym_num, d);
      s->tlm_sym_den    = dp_tlm_probe (t, sym_den, d);
      if (s->tlm_sym_lock < 0 || s->tlm_sym_locked < 0 || s->tlm_sym_num < 0
          || s->tlm_sym_den < 0)
        r = DP_ERR_INVALID;
    }
  /* Where the receiver is in its lifecycle, the Doppler its chain was built
     from, and its refused hits: once per steps() call, after it
     (dp_async_dsss_receiver_steps()). The rebuilds re-run this attach, and a
     registered name returns its own id, so these stay one series each. */
  if (r == DP_OK)
    {
      s->tlm_state    = dp_tlm_probe (t, st, d);
      s->tlm_handover = dp_tlm_probe (t, handover, d);
      s->tlm_refused  = dp_tlm_probe (t, refused, d);
      if (s->tlm_state < 0 || s->tlm_handover < 0 || s->tlm_refused < 0)
        r = DP_ERR_INVALID;
    }
  /* With them, once a call: the hand-over's source, the seed's C/N0, and
     the loops' NCO phases at the call's first sample. */
  if (r == DP_OK)
    {
      s->tlm_ho_src     = dp_tlm_probe (t, ho_src, d);
      s->tlm_cn0        = dp_tlm_probe (t, cn0, d);
      s->tlm_phase_car  = dp_tlm_probe (t, ph_car, d);
      s->tlm_phase_code = dp_tlm_probe (t, ph_code, d);
      if (s->tlm_ho_src < 0 || s->tlm_cn0 < 0 || s->tlm_phase_car < 0
          || s->tlm_phase_code < 0)
        r = DP_ERR_INVALID;
    }
  /* The loop control the two flags drive: had_lock, the coast and the
     hold point's re-mark, once a code period where it is decided; and the
     release clock against its confirm interval, once a call. */
  if (r == DP_OK)
    {
      s->tlm_had_lock = dp_tlm_probe (t, had_lock, d);
      s->tlm_coast    = dp_tlm_probe (t, coast, d);
      s->tlm_hold     = dp_tlm_probe (t, hold, d);
      s->tlm_release  = dp_tlm_probe (t, release, d);
      if (s->tlm_had_lock < 0 || s->tlm_coast < 0 || s->tlm_hold < 0
          || s->tlm_release < 0)
        r = DP_ERR_INVALID;
    }
  /* The fine hand-over's estimate, its snr_db and its sums, once at the
     refine's decision. */
  if (r == DP_OK)
    {
      s->tlm_fine_hz  = dp_tlm_probe (t, fine_hz, d);
      s->tlm_fine_snr = dp_tlm_probe (t, fine_snr, d);
      s->tlm_fine_n   = dp_tlm_probe (t, fine_n, d);
      if (s->tlm_fine_hz < 0 || s->tlm_fine_snr < 0 || s->tlm_fine_n < 0)
        r = DP_ERR_INVALID;
    }
  return r;
}

/* Detach every loop. The request itself is the caller's to clear. */
static void
adr_tlm_detach (dp_async_dsss_receiver_state_t *s)
{
  const char *p = s->tlm_prefix;
  (void)dp_carrier_nda_set_telemetry (&s->car, NULL, p, 1);
  (void)dp_carrier_nda_set_telemetry (&s->car_held, NULL, p, 1);
  (void)dp_dll_set_telemetry (s->dll, NULL, p, 1);
  (void)dp_dll_set_steer_telemetry (s->dll, NULL, p, 1);
  (void)dp_RateConverter_set_telemetry (s->rc, NULL, p, 1);
  (void)dp_mpsk_receiver_set_telemetry (s->rx, NULL, p, 1);
  (void)dp_ratesync_loop_set_filter_telemetry (&s->rx->l.timing, NULL, p, 1);
  if (s->acq)
    (void)dp_correlator_set_telemetry (s->acq, NULL, p, 1, NULL);
  (void)dp_carrier_acq_set_telemetry (s->ca, NULL, p, 1);
  s->tlm_sym_lock = s->tlm_sym_locked = -1;
  s->tlm_sym_num = s->tlm_sym_den = -1;
  s->tlm_state = s->tlm_handover = s->tlm_refused = -1;
  s->tlm_ho_src = s->tlm_cn0 = s->tlm_phase_car = s->tlm_phase_code = -1;
  s->tlm_had_lock = s->tlm_coast = s->tlm_hold = s->tlm_release = -1;
  s->tlm_fine_hz = s->tlm_fine_snr = s->tlm_fine_n = -1;
}

/* Size the track chain's per-period scratch for the chain just built. One
 * output per input sample bounds the Dll; the resampler's output is that
 * times its rate plus its own margin. Grow-only, so a regrid to a lower
 * rate leaves the larger buffer in place rather than churning. */
static void
adr_size_track_scratch (dp_async_dsss_receiver_state_t *s)
{
  size_t need_dll = s->tsamps;
  size_t need_rc  = (size_t)((double)s->tsamps * s->rc->rate) + 64;
  if (s->track_dll_out_cap < need_dll)
    {
      free (s->track_dll_out_buf);
      s->track_dll_out_buf
          = dp_xmallocn (need_dll, sizeof *s->track_dll_out_buf);
      s->track_dll_out_cap = need_dll;
    }
  if (s->track_rc_out_cap < need_rc)
    {
      free (s->track_rc_out_buf);
      s->track_rc_out_buf = dp_xmallocn (need_rc, sizeof *s->track_rc_out_buf);
      s->track_rc_out_cap = need_rc;
    }
}

static void
adr_rebuild_track_chain (dp_async_dsss_receiver_state_t *s, double chip_phase,
                         double doppler_hz_est, size_t segments, size_t sps,
                         int n)
{
  dp_carrier_nda_state_t car;
  dp_dll_state_t        *dll = NULL;
  /* Zeroed first, padding included: the loop is copied into car and
     car_held whole, and their blobs are whole-struct snapshots. */
  memset (&car, 0, sizeof car);
  dp_RateConverter_state_t *rc = NULL;
  dp_mpsk_receiver_state_t *rx = NULL;
  adr_build_track_chain (s, chip_phase, doppler_hz_est, segments, sps, n, &car,
                         &dll, &rc, &rx);
  adr_free_track_chain (s);
  s->had_lock      = 0; /* a fresh chain pulls in before it may coast */
  s->car_coasting  = 0;
  s->car           = car;
  s->car_held      = car;
  s->dll           = dll;
  s->rc            = rc;
  s->rx            = rx;
  s->segments      = segments;
  s->sps           = sps;
  s->n             = n;
  s->car_carry_len = 0;
  adr_size_track_scratch (s);
  adr_reset_lock (s); /* fresh symbol-lock per pass */
  /* The rebuilt loops report under the names the old ones did, so the
     attachment -- and every probe id -- survives the seed. Same names, so
     nothing new registers and this cannot fill the table. */
  if (s->tlm)
    (void)adr_tlm_attach (s);
}

/* The residual-carrier estimator on a despread stream at `target_rate`
 * samples per second -- the refine chain's. The resolution is
 * refine_samples_per_symbol * symbol_rate / refine_n_fft, the design SNR the
 * derated C/N0's, the dwell floored at refine_min_blocks (#1265,
 * docs/design/async-dsss-receiver-measurements.md §12.16: det_n_noncoh sized
 * it for detection at the derated C/N0, two blocks at 45 dB-Hz with the
 * shipped margin, while the estimate's noise the chain must pull in from is
 * 210 Hz there and 77 at seven), bounded by the give-up cap. */
static dp_carrier_acq_state_t *
adr_new_carrier_acq (const dp_async_dsss_receiver_state_t *s,
                     double                                target_rate)
{
  /* design_snr/resolution_hz: freq_refine.refine_seed_carrier_acq()'s own
   * formula, ported verbatim (see objects/async_dsss_receiver.toml's
   * refine_design_margin_db doc comment for why this empirical derating
   * is used as-is rather than re-derived). */
  double effective_cn0_dbhz = s->cn0_dbhz - s->refine_design_margin_db;
  double design_snr    = dp_det_cn0_to_snr (effective_cn0_dbhz, target_rate);
  double resolution_hz = (double)s->refine_samples_per_symbol * s->symbol_rate
                         / (double)s->refine_n_fft;
  dp_carrier_acq_state_t *ca = dp_xnn (dp_carrier_acq_create (
      target_rate, s->symbol_rate, resolution_hz, s->refine_zero_pad,
      DP_PSD_WINDOW_HANN, 0.0f, NULL, 0, s->pfa, s->pd, design_snr,
      s->refine_sequential, s->refine_max_n_blocks));
  if (ca->dwell_target < s->refine_min_blocks)
    ca->dwell_target = s->refine_min_blocks;
  /* The fine hand-over's dwell, when it is the longer. */
  if (s->fine_on && ca->dwell_target < s->fine.blocks)
    ca->dwell_target = s->fine.blocks;
  if (ca->dwell_target > ca->max_n_blocks)
    ca->dwell_target = ca->max_n_blocks;
  return ca;
}

/* One frozen-carrier-wiped code period through refine_dll -> refine_rc ->
 * CarrierAcquisition. `period` must be exactly s->tsamps samples.
 * dp_carrier_acq_steps() is documented as a no-op once ready (or the
 * give-up cap) is reached, so calling it past that point is harmless.
 *
 * `refine_dll` MUST oversample the epoch: with asynchronous data the
 * residual carrier rides a ~symbol_rate-wide data-modulated spectrum, and
 * CarrierAcquisition's PSDMF can only locate its centre if that spectrum is
 * sampled above its Nyquist. `dp_dll_lookback_segments(refine_max_error_db)`
 * splits each epoch into `refine_segments` coherent integrate-and-dump
 * windows -- each window is phase-coherent internally (what the per-block
 * FFT consumes), and the several windows per epoch raise the despread rate
 * to `refine_segments * epoch_rate`, comfortably above the data bandwidth
 * before refine_rc feeds the estimator. The degenerate `segments=1` (one
 * dump per epoch, ~epoch_rate ~= 1 sample/symbol here) undersamples that
 * spectrum: any nonzero coarse-handoff residual aliases, and the estimate
 * collapses to near-DC. See objects/async_dsss_receiver.toml's
 * refine_max_error_db comment. */
static void
adr_refine_period (dp_async_dsss_receiver_state_t *s,
                   const float _Complex           *period)
{
  for (size_t i = 0; i < s->tsamps; i++)
    s->car_wiped_buf[i] = costas_wipeoff (&s->car_frozen, period[i]);

  size_t n_dll = dp_dll_steps (s->refine_dll, s->car_wiped_buf, s->tsamps,
                               s->refine_dll_out_buf, s->refine_dll_out_cap);
  if (n_dll == 0)
    return;
  /* The fine hand-over's sums: the Dll's dumps (refine_segments an epoch,
     disjoint, adding up to the window) summed in groups to fine.k a
     period, raised to the M-th power (M = 2: squared), buffered. The
     summation is the anti-alias filter and the dump the decimator. */
  if (s->fine_on)
    {
      const size_t grp = s->refine_segments / (size_t)s->fine.k;
      const size_t add = n_dll / grp;
      if (s->fine_n + add > s->fine_cap)
        {
          /* Full: drop the oldest, so the buffer is the latest dwell. */
          const size_t drop = s->fine_n + add - s->fine_cap;
          memmove (s->fine_buf, s->fine_buf + drop,
                   (s->fine_n - drop) * sizeof *s->fine_buf);
          s->fine_n -= drop;
        }
      for (size_t i = 0; i + grp <= n_dll; i += grp)
        {
          float _Complex z = 0.0f;
          for (size_t j = 0; j < grp; j++)
            z += s->refine_dll_out_buf[i + j];
          s->fine_buf[s->fine_n++] = z * z;
        }
    }

  size_t n_rc
      = dp_RateConverter_execute (s->refine_rc, s->refine_dll_out_buf, n_dll,
                                  s->refine_rc_out_buf, s->refine_rc_out_cap);
  if (n_rc == 0)
    return;

  dp_carrier_acq_steps (s->ca, s->refine_rc_out_buf, n_rc);
}

/* The refine has decided: the carrier estimate is ready, or it has given
 * up at its block cap. The one test both the period loop below and
 * steps() ask, so the loop stops exactly where steps() hands over. */
static int
adr_refine_decided (const dp_async_dsss_receiver_state_t *s)
{
  const dp_carrier_acq_state_t *ca = s->ca;
  size_t cap = ca->sequential ? ca->max_n_blocks : ca->dwell_target;
  if (ca->n_blocks >= cap)
    return 1;
  /* A sequential estimator that fires early still waits out the fine
     dwell. Once ready it counts no more blocks (dp_carrier_acq_steps()
     returns early), so the dwell is also read off the fine buffer, which
     keeps filling: it holds the dwell's sums once it is full but for its
     spare period (fine_cap). Without it a refine that fired early never
     decided. A non-sequential estimator is ready only at its dwell target,
     which is at least the fine dwell, so its decision is unchanged. */
  return ca->ready
         && (!s->fine_on || ca->n_blocks >= s->fine.blocks
             || s->fine_n + (size_t)s->fine.k >= s->fine_cap);
}

/* The first sample a live chain can start on at or past @p at: the smallest
 * ceil(k * period), k whole periods from the hand-off, that is >= at. With a
 * fractional period (17.5) the starts are 0, 18, 35, 53 ...; the phase
 * carries each one's fraction (adr_handover_phase()). The same rounding as
 * the hand-over's estimate, `ceil(k * period - 1e-9)`. The first guess is
 * within one of k, so each loop runs at most once or twice. */
static uint64_t
adr_period_start_at (double period, uint64_t at)
{
  uint64_t k = (uint64_t)floor ((double)at / period);
  while (k > 0 && (uint64_t)ceil ((double)(k - 1) * period - 1e-9) >= at)
    k--;
  while ((uint64_t)ceil ((double)k * period - 1e-9) < at)
    k++;
  return (uint64_t)ceil ((double)k * period - 1e-9);
}

/* Keep a refine call's input in the hand-over's ring: its last
 * refine_hist_cap samples at most, so one bounded copy a call. */
static void
adr_refine_hist_push (dp_async_dsss_receiver_state_t *s,
                      const float _Complex *x, size_t x_len)
{
  const size_t cap = s->refine_hist_cap;
  if (x_len >= cap)
    {
      memcpy (s->refine_hist, x + x_len - cap, cap * sizeof *x);
      s->refine_hist_head = 0;
      s->refine_hist_len  = cap;
      return;
    }
  const size_t first
      = cap - s->refine_hist_head < x_len ? cap - s->refine_hist_head : x_len;
  memcpy (s->refine_hist + s->refine_hist_head, x, first * sizeof *x);
  memcpy (s->refine_hist, x + first, (x_len - first) * sizeof *x);
  s->refine_hist_head = (s->refine_hist_head + x_len) % cap;
  s->refine_hist_len
      = s->refine_hist_len + x_len < cap ? s->refine_hist_len + x_len : cap;
}

/* The ring's newest @p n samples, oldest first, into @p dst (n <= len). */
static void
adr_refine_hist_tail (const dp_async_dsss_receiver_state_t *s,
                      float _Complex *dst, size_t n)
{
  const size_t cap   = s->refine_hist_cap;
  const size_t start = (s->refine_hist_head + cap - n) % cap;
  const size_t first = cap - start < n ? cap - start : n;
  memcpy (dst, s->refine_hist + start, first * sizeof *dst);
  memcpy (dst + first, s->refine_hist, (n - first) * sizeof *dst);
}

/* Refine-stage entry point: buffer raw samples into whole `tsamps`-sample
 * periods (the shared car_carry_buf -- refine and track never run
 * concurrently) and run adr_refine_period() on each complete one, up to
 * the period the refine decides in. The rest of the call is the tracking
 * chain's: steps() feeds it the tail, so folding it here too would pay for
 * those samples twice (#2395). Returns how many of x the refine took: all
 * of them until it decides, up to the end of the period it decides in on
 * that call, and none after. */
static size_t
adr_process_refine (dp_async_dsss_receiver_state_t *s, const float _Complex *x,
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
        return pos;
      adr_refine_period (s, s->car_carry_buf);
      s->refine_periods++;
      s->car_carry_len = 0;
    }

  while (pos + s->tsamps <= x_len && !adr_refine_decided (s))
    {
      adr_refine_period (s, x + pos);
      s->refine_periods++;
      pos += s->tsamps;
    }

  /* Decided: the rest of x is the tracking chain's tail, which steps()
     feeds it, and the track rebuild clears this buffer. Buffering it here
     would also overrun car_carry_buf, which holds at most one period: only
     a loop that consumed every whole period leaves less than one. */
  if (adr_refine_decided (s))
    {
      s->car_carry_len = 0;
      return pos;
    }
  size_t leftover = x_len - pos;
  if (leftover > 0)
    memcpy (s->car_carry_buf, x + pos, leftover * sizeof (*x));
  s->car_carry_len = leftover;
  return x_len;
}

/* One carrier-wiped code period through the Dll -> at most ONE
 * carrier_nda_update() a period, on the period's code-period window (the
 * coherent sum the look-back picked, dp_dll_get_window_sum()): the M = 2
 * error removes the data with no sign decision, and the window does not
 * span a data transition. It runs PRE-despread, so despreading stays coherent:
 * the "unfreeze and track loop 1" step of coarse -> freeze -> refine -> track.
 * The Dll's emitted partials are the despread symbol stream this object
 * hands to RateConverter/MpskReceiver; they go to dll_out (capacity
 * max_out), and the count is returned. */
static size_t
adr_track_period (dp_async_dsss_receiver_state_t *s,
                  const float _Complex *period, float _Complex *dll_out,
                  size_t max_out)
{
  /* The coast (#1271). Once locked in this stint, BOTH flags down means
     the emitter is gone or faded, and both loops hold what they settled
     on with both flags up, their lock detectors still looking; one flag
     down is a degrade and the loops run. Left running on noise, a
     departed receiver's Dll free-runs at whatever its filter holds
     (measured: up to 90 chips per second), sweeps through every live
     emitter's code phase and can capture one that crosses slowly enough,
     its code flag then flickering on a neighbour and restarting the
     release clock for as long as it follows it. A genuine return lands
     on the held replica, lights a flag, and both loops run again on it;
     a neighbour that lights the code flag as it passes steers the loops
     for the blip and no more -- the hold point is marked with both flags
     up (dp_dll_hold_here, car_held), so what a blip steers is dropped on
     re-entry (measured: a neighbour 2 kHz off crossing at 4 chips per
     second, 8-10 blips, no follow). Holding on ONE flag down was tried
     and measured wrong: two live emitters 475 Hz apart crossing at a chip
     per second degrade both receivers' symbol flags, and a code loop held
     through that cannot re-centre on its own emitter afterwards -- both
     flags then stay down for the rest of the watch, a false loss. Run,
     it rides the crossing out (code lock held in 5 of 6 trials). A
     neighbour within about a kilohertz crossing at a chip or two a second
     can lock the carrier, and to a receiver on its own that IS a return --
     the parallel controller, which knows that emitter is held, is where it
     is told apart (#1275). Before the first lock the loops must run: that is
     the pull-in. */
  const int code_up = dp_dll_get_locked (s->dll),
            sym_up  = s->sym_lockdet.locked;
  if (code_up && sym_up)
    {
      s->had_lock = 1;
      s->car_held = s->car;
      dp_dll_hold_here (s->dll);
    }
  const int car_coast = s->had_lock && !code_up && !sym_up;
  if (car_coast && !s->car_coasting)
    {
      /* The hold point, run on its integrator: car_held's command still
         carries its last update's proportional step, which the NCO would
         otherwise repeat every update interval of the coast. */
      s->car = s->car_held;
      dp_carrier_nda_hold (&s->car);
    }
  s->car_coasting = car_coast;
  dp_dll_set_coast (s->dll, s->had_lock && !code_up && !sym_up);
  if (s->tlm && s->tlm_coast >= 0)
    {
      dp_tlm_emit (s->tlm, s->tlm_had_lock, (double)s->had_lock);
      dp_tlm_emit (s->tlm, s->tlm_coast, (double)car_coast);
      dp_tlm_emit (s->tlm, s->tlm_hold, (double)(code_up && sym_up));
    }
  for (size_t i = 0; i < s->tsamps; i++)
    s->car_wiped_buf[i] = carrier_nda_wipeoff (&s->car, period[i]);
  size_t n_out
      = dp_dll_steps (s->dll, s->car_wiped_buf, s->tsamps, dll_out, max_out);
  /* The Dll emits nothing until an epoch closes, then that epoch's dumps
     (or, with one segment, one output), so n_out > 0 is exactly "a new
     window sum exists" (dll_core.c's epoch close). A block that closes two
     epochs updates once, on the later sum. */
  const int update = n_out > 0 && !car_coast;
  if (update)
    {
      /* The carrier loop's update: the one coherent sum over the
         code-period window the Dll's look-back picked this period
         (dp_dll_get_window_sum()), a transition-free window whenever a
         symbol is at least a code period, which create() requires. Its
         M = 2 error Im(w^2) removes the BPSK data (d^2 = 1) with no sign
         decision, and is divided by the window's signal power: the
         running average of |w|^2 less the Dll's noise on the window
         sum's scale (dp_dll_get_window_noise()). The lock's ratio of sums
         is formed on the same w. */
      carrier_nda_update (&s->car, dp_dll_get_window_sum (s->dll),
                          dp_dll_get_window_noise (s->dll));
      /* carrier_nda_update() emits nothing: the composing caller flushes
         the loop's probes once per update. */
      if (s->car.tlm.ctx)
        dp_carrier_nda_tlm_flush (&s->car);
      /* Continuous carrier->code aiding: the pre-despread loop tracks the
         FULL carrier offset (including the 500 Hz/s ramp), so refresh the
         code NCO's rate bias from it every period. This keeps the initial
         build-time seed (dp_dll_set_rate_aid in adr_build_track_chain) fresh
         as the ramp drifts, and applies at the next period boundary
         (dll_set_rate_ aid only stores the field, never clobbers phase_inc).
         Off when carrier_freq_hz == 0. */
      if (s->carrier_freq_hz > 0.0)
        dp_dll_set_rate_aid (s->dll, dp_carrier_nda_get_norm_freq (&s->car)
                                         * (s->chip_rate * (double)s->spc)
                                         / s->carrier_freq_hz);
    }
  else
    /* No update this period (a coast, or no epoch closed): the next
       period's wipe runs on the integrator, not on a repeat of the last
       update's proportional step. */
    dp_carrier_nda_hold (&s->car);
  return n_out;
}

/* One carrier-wiped code period through the live chain: the Dll's
 * partials, resampled to MpskReceiver's rate, demodulated into `out`. Every
 * stage streams (state carries across calls), so running the chain a period
 * at a time is the same computation as a block at a time -- and it is what
 * lets the scratch be sized once per build instead of per push (#1192).
 * Returns the symbols written. */
static size_t
adr_track_period_chain (dp_async_dsss_receiver_state_t *s,
                        const float _Complex *period, float _Complex *out,
                        size_t max_out)
{
  size_t n_dll = adr_track_period (s, period, s->track_dll_out_buf,
                                   s->track_dll_out_cap);
  size_t n_rc
      = dp_RateConverter_execute (s->rc, s->track_dll_out_buf, n_dll,
                                  s->track_rc_out_buf, s->track_rc_out_cap);
  return dp_mpsk_receiver_steps (s->rx, s->track_rc_out_buf, n_rc, out,
                                 max_out);
}

static size_t
adr_track_chain (dp_async_dsss_receiver_state_t *s, const float _Complex *x,
                 size_t x_len, float _Complex *out, size_t max_out)
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
      emitted += adr_track_period_chain (s, s->car_carry_buf, out + emitted,
                                         max_out - emitted);
      s->car_carry_len = 0;
    }

  while (pos + s->tsamps <= x_len)
    {
      emitted += adr_track_period_chain (s, x + pos, out + emitted,
                                         max_out - emitted);
      pos += s->tsamps;
    }

  size_t leftover = x_len - pos;
  if (leftover > 0)
    memcpy (s->car_carry_buf, x + pos, leftover * sizeof (*x));
  s->car_carry_len = leftover;

  /* Symbol-lock detector: per emitted symbol, integrate the BPSK phase-lock
   * signal (I^2-Q^2)/(I^2+Q^2) = cos(2*phi) as a POWER-WEIGHTED (i.e. SNR-
   * weighted) running mean -- separate EMAs of the numerator and the total
   * power, so a high-|symbol| (high-SNR) symbol dominates and a
   * noise/transient one contributes little -- then step the hysteretic lockdet
   * on the ratio. dwell = 1/lock_alpha >= LOCK_DWELL. See the
   * ASYNC_DSSS_RX_LOCK_* defines. */
  for (size_t i = 0; i < emitted; i++)
    {
      double re = (double)crealf (out[i]);
      double im = (double)cimagf (out[i]);
      s->lock_num
          = dp_ema_step (s->lock_num, re * re - im * im, s->lock_alpha);
      s->lock_den
          = dp_ema_step (s->lock_den, re * re + im * im, s->lock_alpha);
      s->lock_metric = (s->lock_den > 0.0) ? s->lock_num / s->lock_den : 0.0;
      (void)dp_lockdet_step (&s->sym_lockdet, s->lock_metric);
      if (s->tlm && s->tlm_sym_lock >= 0)
        {
          dp_tlm_emit (s->tlm, s->tlm_sym_lock, s->lock_metric);
          dp_tlm_emit (s->tlm, s->tlm_sym_locked,
                       (double)s->sym_lockdet.locked);
          dp_tlm_emit (s->tlm, s->tlm_sym_num, s->lock_num);
          dp_tlm_emit (s->tlm, s->tlm_sym_den, s->lock_den);
        }
    }
  return emitted;
}

/* Restart the search: drain the engine, and owe its next dwell a start on
 * the block grid (adr_align_skip()). Every path back to a fresh search
 * comes through here, so no dwell starts off the grid. */
static void
adr_restart_search (dp_async_dsss_receiver_state_t *s, uint64_t at)
{
  dp_correlator_reset (s->acq);
  s->align_pending = true;
  /* The search's resampler restarts with it, at input index @p at (the
     next sample the search will see), so its timebase maps back from
     there. */
  if (s->srch_rs)
    dp_resamp_reset (s->srch_rs);
  s->srch_origin   = at;
  s->srch_n        = 0;
  s->srch_hist_len = 0;
}

/* The last srch_hist_cap input samples, kept across calls while searching
   (the hand-off reaches back into them, adr_steps()). */
static void
adr_hist_push (dp_async_dsss_receiver_state_t *s, const float _Complex *x,
               size_t x_len)
{
  const size_t cap = s->srch_hist_cap;
  if (cap == 0)
    return;
  if (x_len >= cap)
    {
      memcpy (s->srch_hist, x + x_len - cap, cap * sizeof *x);
      s->srch_hist_len = cap;
      return;
    }
  const size_t keep
      = s->srch_hist_len + x_len > cap ? cap - x_len : s->srch_hist_len;
  memmove (s->srch_hist, s->srch_hist + s->srch_hist_len - keep,
           keep * sizeof *x);
  memcpy (s->srch_hist + keep, x, x_len * sizeof *x);
  s->srch_hist_len = keep + x_len;
}

/* The search timebase's index of search sample `n` past the resampler's
   origin: the origin in search samples plus n. The block grid the dwells
   align to is counted in it, so every receiver fed the same stream from
   create shares it. At the search rate already it is the input index. */
static uint64_t
adr_search_index (const dp_async_dsss_receiver_state_t *s, uint64_t n)
{
  return (uint64_t)llround ((double)s->srch_origin * s->srch_ratio) + n;
}

/* The input index (fractional) search sample `n` past the origin carries:
   resamp's own contract, `origin + n / ratio - delay`. */
static double
adr_search_to_input (const dp_async_dsss_receiver_state_t *s, uint64_t n)
{
  if (!s->srch_rs)
    return (double)(s->srch_origin + n);
  return (double)s->srch_origin + (double)n / s->srch_ratio
         - dp_resamp_get_execute_delay (s->srch_rs);
}

/* Of an @p x_len-sample tail ending at samples_in, how many to discard so
 * the engine's next sample is a multiple of one block, `coherent_bins *
 * code_bins` search samples, counted in the search timebase
 * (adr_search_index()). All of them while the boundary is
 * past the tail; the debt is paid when it is reached. */
static size_t
adr_align_skip (dp_async_dsss_receiver_state_t *s, size_t x_len)
{
  const uint64_t block
      = (uint64_t)s->acq->search->coherent_bins * s->acq->search->code_bins;
  const uint64_t pos = adr_search_index (s, s->srch_n);
  /* The first boundary past the resampler's start-up: the search samples
     before srch_warm carry the filter's transient (input before the
     restart is zeros to it), so a dwell starting in them would see a
     weaker signal than every later one. 0 at the search rate, where this
     is the next boundary at or after pos, as it always was. */
  const uint64_t ready  = adr_search_index (s, s->srch_warm);
  const uint64_t from   = ready > pos ? ready : pos;
  const uint64_t target = (from + block - 1) / block * block;
  const uint64_t owed   = target - pos;
  if (owed > (uint64_t)x_len)
    return x_len;
  s->align_pending = false;
  return (size_t)owed;
}

/* One signal's code phase: the one key, dp__adsss_same_code_phase()
 * (async_dsss_receiver_private.h), which the pool's duplicate test calls
 * too. */
static int
adr_same_code_phase (double a, double b, size_t code_len)
{
  return dp__adsss_same_code_phase (a, b, (double)code_len);
}

/* A dwell's peak list, strongest first, to the one this receiver takes.
 * Three rules, in order, then the survivors are counted to `rank`:
 *
 * 1. In the zone of a stronger peak (a neighbouring cell, or the same
 *    emitter in a neighbouring tile): that emitter, not a second one.
 * 2. Within reach of the stronger emitters' sidelobes: one emitter's list
 *    also holds its code's Doppler-offset sidelobes, up to dsl below it and
 *    hundreds of chips from its lag, where rule 1 cannot reach (design
 *    acq-surface.md §8). A peak is a second emitter only if its statistic
 *    exceeds what the stronger emitters' sidelobes can read AT ITS CELL by
 *    more than the detector's own gate. Noise riding on a sidelobe lifts
 *    it by no more than the noise's own magnitude, so a sidelobe passes
 *    only when that noise alone would have cleared the gate: the
 *    false-alarm rate the search already runs at. The bound
 *    (adr_sidelobe_bound()) is per cell: each stronger emitter's
 *    statistic on its own lag (dp_correlator_hit_t::test_stat_peak: the
 * straddle between two lag columns put back, since its sidelobes keep their
 *    level) times the code's cross-ambiguity from that emitter to this
 *    cell (dp_correlator_sidelobe_ratio()), which is dsl only at the worst
 * cells. Bounded by dsl everywhere, an emitter at the design floor was never
 *    taken beside any stronger one its sidelobes could reach.
 * 3. In an excluded zone: another receiver's signal. It is still an
 *    emitter, so it still counts in rule 2's bound.
 *
 * 1 and the hand-off in @p pick, or 0 when the list holds no peak at this
 * rank. */
/* Rule 2's bound at the cell of @p cand: what the stronger emitters'
 * sidelobes can read there, as a statistic (doppler#2554), each one its
 * on-lag statistic times its cross-ambiguity to this cell.
 *
 * Combining them: an emitter's Doppler-offset sidelobes land on its own
 * Doppler row in every other tile, f + k f_E: the slow-time transform resolves
 * the same phase advance per epoch in each tile (design acq-surface.md §8). On
 * a deep block (D > 1) over a code-only window those cells are fixed, so two
 * emitters on one row (modulo f_E) can stack their sidelobes on one cell
 * and the bound sums them. "One row" is the candidate's row and both its
 * neighbours: two emitters a few Hz apart quantize to ADJACENT rows, and
 * a cell their sidelobes stack on sits on one parent's row and a row from
 * the other (doppler#2556, review 5630995725), so half a row each side
 * dropped that parent. Summed over every emitter regardless of row, the
 * bound narrowed the near-far range with each emitter on the air. Off the row,
 * an emitter's sidelobe is down by the slow-time leakage as well, so the
 * strongest one bounds it.
 *
 * On a one-epoch search (D = 1) a native row is a whole tile, so rows
 * cannot be told apart, and the sum would read every emitter as stacked.
 * Measured there, two equal emitters on one carrier did not stack past the
 * gate under the strongest alone, while the sum left a true emitter 10 dB
 * below the strongest of six untaken (both on doppler#2554), so the
 * strongest alone bounds it. */
static double
adr_sidelobe_bound (const dp_async_dsss_receiver_state_t *s,
                    const dp_correlator_hit_t            *cand,
                    const dp_correlator_hit_t *const *em, const double *stat,
                    size_t n)
{
  const double fe    = s->acq->search->chip_rate / (double)s->code_len;
  const double reach = 1.5 * s->acq->search->doppler_res_hz;
  const double f_c   = dp_correlator_bin_doppler_hz (s->acq, cand->row);
  /* On a deep block the per-cell ambiguity is not yet the whole story: with
     data inside the block, a strong emitter's own data splatter adds to a
     cell, and a cell was measured at 0.134 of the emitter's peak where its
     ambiguity at that lag tops out at 0.116 over every Doppler offset. The
     code's worst sidelobe (dsl, 0.142 there) covers it, so a deep block
     keeps it as a floor until the splatter is modelled (doppler#2730). */
  const double floor_ratio
      = s->acq->search->coherent_bins > 1
            ? pow (10.0, dp_correlator_dsl_db (s->acq) / 20.0)
            : 0.0;
  double strongest = 0.0, on_row = 0.0;
  for (size_t j = 0; j < n; j++)
    {
      /* what emitter j's sidelobe can read at this cell */
      const double v
          = stat[j]
            * fmax (floor_ratio,
                    dp_correlator_sidelobe_ratio (s->acq, em[j], cand));
      strongest = fmax (strongest, v);
      double d
          = fmod (f_c - dp_correlator_bin_doppler_hz (s->acq, em[j]->row), fe);
      d = d < 0.0 ? d + fe : d;
      if (fmin (d, fe - d) <= reach)
        on_row += v;
    }
  return s->acq->search->coherent_bins > 1 ? fmax (strongest, on_row)
                                           : strongest;
}

static int
adr_pick (const dp_async_dsss_receiver_state_t *s,
          const dp_correlator_hit_t *hits, size_t n_hits, double shift,
          dp_correlator_handoff_t *pick)
{
  const double gate = dp_correlator_gate_stat (s->acq);
  double       seen[ASYNC_DSSS_RX_MAX_PEAKS];
  /* The emitters so far, strongest first: statistic and Doppler. */
  double                     em_stat[ASYNC_DSSS_RX_MAX_PEAKS];
  const dp_correlator_hit_t *em_hit[ASYNC_DSSS_RX_MAX_PEAKS];
  size_t                     n_seen = 0, n_kept = 0, n_em = 0;
  for (size_t i = 0; i < n_hits; i++)
    {
      dp_correlator_handoff_t ho;
      dp_correlator_build_handoff (s->acq, &hits[i], s->code_len, &ho);
      /* Carried to the hand-off sample (adr_steps()): every phase this
         compares -- the others in the dwell, the exclusions a controller
         read off receivers fed the same stream -- is a phase at that
         sample, so the hit's is too. 0 at the search rate. */
      if (shift != 0.0)
        {
          double g = shift;
          if (s->carrier_freq_hz > 0.0)
            g *= 1.0 + ho.doppler_hz_est / s->carrier_freq_hz;
          ho.chip_phase = dp_fmod_pos (ho.chip_phase + g, (double)s->code_len);
        }
      int drop = 0;
      for (size_t j = 0; j < n_seen && !drop; j++)
        drop = adr_same_code_phase (ho.chip_phase, seen[j], s->code_len);
      seen[n_seen++] = ho.chip_phase;
      if (drop
          || (double)hits[i].test_stat
                     - adr_sidelobe_bound (s, &hits[i], em_hit, em_stat, n_em)
                 <= gate)
        continue;
      /* An emitter's sidelobes keep their level when its own peak falls
         between two lag columns, so the bound they set is its statistic
         on its own lag (dp_correlator_hit_t::test_stat_peak), not as read:
         else a strong emitter's straddle lets its sidelobe pass for a second
         emitter, by more the stronger it is. */
      em_stat[n_em]  = (double)hits[i].test_stat_peak;
      em_hit[n_em++] = &hits[i];
      for (size_t j = 0; j < s->n_excl && !drop; j++)
        drop = adr_same_code_phase (ho.chip_phase, s->excl[j], s->code_len);
      if (drop)
        continue;
      if (n_kept++ == s->rank)
        {
          *pick = ho;
          return 1;
        }
    }
  return 0;
}

dp_async_dsss_receiver_state_t *
dp_async_dsss_receiver_create (
    const uint8_t *code, size_t code_len, double chip_rate, double symbol_rate,
    double sample_rate, int m, double cn0_dbhz, double pfa, double pd,
    double doppler_uncertainty, size_t segments, size_t sps, int differential,
    double refine_max_error_db, size_t refine_samples_per_symbol,
    double refine_design_margin_db, size_t refine_n_fft,
    size_t refine_zero_pad, bool refine_sequential, size_t refine_max_n_blocks,
    double carrier_freq_hz, double lost_confirm_s, bool reacquire,
    double code_only_s, double doppler_rate, bool refine)
{
  /* symbol_rate becomes the carrier estimator's sample rate, which PSD
     refuses unless finite: refused here, at create, because that estimator
     is built under dp_xnn mid-stream (adr_new_carrier_acq), where a NULL
     aborts.  The negated form also refuses a NaN. */
  /* A carrier the code is coupled to is 0 (none) or above half the sample
     rate: the searcher's hand-off and both chains divide a Doppler by it,
     and a tiny one sent that to inf and a seed phase to NaN, which aborted
     under dp_xnn (doppler#2103). */
  /* Samples a chip at the caller's rate: any value of at least one. The
     search runs at its own, smooth rate (below). */
  if (sample_rate == 0.0 && chip_rate > 0.0 && isfinite (chip_rate))
    sample_rate = 2.0 * chip_rate; /* the default: two samples a chip */
  if (!(sample_rate > 0.0 && isfinite (sample_rate)) || !(chip_rate > 0.0))
    return NULL;
  const double spc = sample_rate / chip_rate;
  if (!code || code_len < 1 || !(chip_rate > 0.0 && isfinite (chip_rate))
      || !(symbol_rate > 0.0 && isfinite (symbol_rate)) || !(spc >= 1.0)
      || (m != 2 && m != 4 && m != 8) || refine_samples_per_symbol < 1
      || refine_n_fft < 1 || refine_zero_pad < 1
      || !dp_correlator_carrier_freq_ok (carrier_freq_hz,
                                         chip_rate * (double)spc)
      || !(lost_confirm_s >= 0.0))
    return NULL;
  /* Reacquire's release is its only way back to searching, so a release
     that never fires would leave it tracking a signal long gone: refused
     rather than accepted as a silent no-op. Checked in the clock's own
     unit, input samples, because a confirm time that rounds to zero of
     them (1e-9 s) IS the disabled clock (adr_release_clock returns at
     lost_confirm_samples == 0). */
  /* The release clock in input samples: the rule is a time, and the input
   * rate is the one clock every state of this object is fed at. Computed
   * once, here, refused on, and stored below. */
  const uint64_t lost_confirm_samples
      = (uint64_t)llround (lost_confirm_s * chip_rate * (double)spc);
  if (reacquire && lost_confirm_samples == 0)
    return NULL;
  /* The carrier estimator built mid-stream under dp_xnn
     (adr_new_carrier_acq) refuses a pfa or pd outside (0, 1), so they are
     refused here, at create (doppler#2103). */
  if (!(pfa > 0.0 && pfa < 1.0) || !(pd > 0.0 && pd < 1.0))
    return NULL;
  /* The carrier error's code-period window holds only while a symbol is at
     least a code period: then some window of one period is free of
     transitions, which the look-back finds (dp_dll_get_window_sum()). */
  if (!(symbol_rate * (double)code_len <= chip_rate))
    return NULL;
  /* The code-only window in whole epochs, design async-dsss-receiver.md §2.3:
     floor(W * chip_rate / sf) - 1 epochs fit at ANY chip phase, and under
     two (no window, or less than two epochs of one) is the epoch-by-epoch
     search. The engine refuses a bad doppler_rate itself; a window too
     long to count in epochs is refused here, before the cast. */
  if (!(code_only_s >= 0.0 && isfinite (code_only_s)))
    return NULL;
  const double window_epochs
      = floor (code_only_s * chip_rate / (double)code_len) - 1.0;
  if (!(window_epochs < 0x1p52))
    return NULL;
  const size_t code_only_epochs
      = window_epochs > 1.0 ? (size_t)window_epochs : 1;
  /* Each Dll this receiver builds -- the refine collector and the tracker,
     both created mid-stream under dp_xnn -- meets the Dll's own domain now,
     so a configuration the Dll refuses (a 1-chip code, whose early and late
     taps would coincide) is refused here rather than aborting the process
     mid-stream, and so is a symbol period its aid would refuse
     (adr_track_chain_ok(), doppler#2103). */
  {
    const size_t refine_segments = dp_dll_lookback_segments (
        (size_t)floor ((double)code_len * spc + 1e-9), refine_max_error_db);
    if (!dp_dll_params_ok (code_len, spc, 0.0, ASYNC_DSSS_RX_DLL_BN,
                           ASYNC_DSSS_RX_DLL_ZETA,
                           ASYNC_DSSS_RX_REFINE_DLL_SPACING, refine_segments)
        || !adr_track_chain_ok (code_len, chip_rate, symbol_rate, segments,
                                sps, spc))
      return NULL;
  }

  /* The carrier loop, from the design C/N0 alone, decided before anything
     is built: its bandwidth by the loop-SNR rule
     (dp_async_dsss_receiver_carrier_bn()), its update SNR and its block
     lock's non-coherent count
     (dp_async_dsss_receiver_carrier_lock_n_noncoh()). A design C/N0 whose
     update SNR is not finite and positive (an absurd finite cn0_dbhz overflows
     or underflows it), whose rule bandwidth the loop filter refuses, or at
     which no block up to CARRIER_NDA_BLOCK_MAX_LEN reaches
     ASYNC_DSSS_RX_CAR_LOCK_PD against ASYNC_DSSS_RX_CAR_LOCK_PFA, builds no
     receiver: the receiver chooses the lock's rates, so it does not accept
     an unsized flag (carrier_nda's own EMA lock stays that object's
     default). Refused here, before the search is sized, so a refused
     design is refused at once. */
  const double car_bn
      = dp_async_dsss_receiver_carrier_bn (cn0_dbhz, chip_rate, code_len);
  const double   car_gamma = adr_carrier_gamma (cn0_dbhz, chip_rate, code_len);
  const uint32_t car_blk   = dp_async_dsss_receiver_carrier_lock_n_noncoh (
      cn0_dbhz, chip_rate, code_len);
  if (!(isfinite (car_gamma) && car_gamma > 0.0)
      || !dp_loop_filter_params_ok (car_bn, 0.707, 1.0) || car_blk == 0u)
    return NULL;

  dp_async_dsss_receiver_state_t *obj = dp_xcalloc (1, sizeof (*obj));
  obj->tlm_sym_lock = obj->tlm_sym_locked = -1; /* detached */
  obj->tlm_sym_num = obj->tlm_sym_den = -1;
  obj->tlm_state = obj->tlm_handover = obj->tlm_refused = -1;
  obj->tlm_fine_hz = obj->tlm_fine_snr = obj->tlm_fine_n = -1;
  obj->tlm_ho_src = obj->tlm_cn0 = obj->tlm_phase_car = obj->tlm_phase_code
      = -1;
  obj->tlm_had_lock = obj->tlm_coast = obj->tlm_hold = obj->tlm_release = -1;

  obj->code = dp_xmalloc (code_len);
  memcpy (obj->code, code, code_len);
  obj->code_len = code_len;

  /* The search runs at the caller's rate when its period is a whole number
     of samples (an awkward length such as 2 * 1,023 = 2 * 3 * 11 * 31 is
     read on a padded lag grid), else where one code period is a power of
     two samples (dp_correlator_search_period()), resampled from the caller's
     rate: 2,048 for 1,023 chips at 11 MSa/s over 5 Mcps. The searcher checks
     its own domain (pfa, pd, C/N0 ...), so its refusal is this create's, not
     an abort under dp_xnn (doppler#2103). */
  const size_t n_period
      = dp_correlator_search_period (code_len, sample_rate, chip_rate);
  const double search_rate = chip_rate * (double)n_period / (double)code_len;
  obj->srch_ratio          = search_rate / sample_rate;
  obj->acq                 = n_period
                                 ? dp_correlator_create_continuous (
                                       obj->code, code_len, search_rate, chip_rate,
                                       symbol_rate, cn0_dbhz, doppler_uncertainty, pfa, pd,
                                       0 /* noise_mode=mean */, code_only_epochs, doppler_rate)
                                 : NULL;
  if (obj->acq && fabs (obj->srch_ratio - 1.0) > 1e-12)
    {
      obj->srch_rs = dp_resamp_create (obj->srch_ratio);
      if (!obj->srch_rs)
        {
          dp_correlator_destroy (obj->acq);
          obj->acq = NULL;
        }
      else
        {
          obj->srch_hist_cap
              = (size_t)ceil (dp_resamp_get_execute_delay (obj->srch_rs)) + 3;
          /* The filter's span, twice its delay, in search samples. */
          obj->srch_warm
              = (size_t)ceil (2.0 * dp_resamp_get_execute_delay (obj->srch_rs)
                              * obj->srch_ratio)
                + 1;
          obj->srch_hist
              = dp_xmallocn (obj->srch_hist_cap, sizeof *obj->srch_hist);
        }
    }
  if (!obj->acq)
    {
      free (obj->code);
      free (obj);
      return NULL;
    }
  /* A physically-coupled carrier moves the code too: the searcher's
     hand-off advances its hit's phase by the drift over half its dwell
     (#1254), and a coherent block aligns its epochs (#1256). The same
     carrier the tracking Dll's aid uses. */
  /* The carrier rebuilds the searcher's map, so it can fail: a receiver
     without the carrier it was asked for is not built. */
  if (carrier_freq_hz > 0.0
      && dp_correlator_set_carrier_freq_hz (obj->acq, carrier_freq_hz)
             != DP_OK)
    {
      /* The search resampler and its history may be built by now; destroy
         frees whatever the zeroed object holds. */
      dp_async_dsss_receiver_destroy (obj);
      return NULL;
    }
  /* A dwell's whole peak list, for adr_pick() to rank. Its first entry is
     the one-peak detector's pick, so rank 0 with no exclusions takes what
     max_peaks 1 took. */
  /* The carrier loop, as decided above: the refine rule below and every
     chain this receiver builds read it. */
  obj->bn_carrier = car_bn;
  obj->car_gamma  = car_gamma;
  obj->car_blk    = car_blk;
  /* With no refine the search's Doppler is the hand-over, and the carrier
     loop pulls in from it only while half the search's Doppler row is
     inside the loop's acquisition bound (async-dsss-receiver.md §4.4).
     Both are fixed here: the row is chip_rate / (code_len * D) and the
     bound bn * chip_rate / (2 * code_len) (ASYNC_DSSS_RX_CARRIER_PULLIN_HZ),
     so the rule is D >= 1 / bn at any chip rate, compared in whole bins
     because at D = 1 / bn the two are equal. Outside it a seed would hand
     the loop a residual it cannot pull in, so the receiver is not built. */
  if (!refine
      && obj->acq->search->coherent_bins
             < ASYNC_DSSS_RX_NO_REFINE_MIN_BINS (obj->bn_carrier))
    {
      dp_async_dsss_receiver_destroy (obj);
      return NULL;
    }
  (void)dp_correlator_set_max_peaks (obj->acq, ASYNC_DSSS_RX_MAX_PEAKS, NULL);
  /* One emitter, one listed peak: a zone over every Doppler bin of every
     roll, so a strong emitter's own lag -- its slow-time sidelobes, its
     roll aliases, its data's spread inside a coherent dwell -- cannot take
     the list's slots from a weaker one elsewhere (doppler#3067), and the
     zone removes only what the pick drops anyway (doppler#3060). Half the
     pick's key wide, not the whole key: the zone is a lag, and with a
     carrier set the key carries each hit's code phase on by its own
     Doppler's dilation, off the lag grid, so a zone as wide as the key
     masks pairs two lags apart that the key keeps -- for several dwells
     when their Dopplers are close (#3071, measured at +/- 2 kHz). Half the
     key keeps the zone inside it; an emitter off the grid by half a lag
     then shows its shoulder once a dwell, which the bench does not see. */
  (void)dp_correlator_set_peak_zone (
      obj->acq, 1, DP_CORRELATOR_SAME_PHASE_CHIPS / 2.0, NULL);
  /* A deep block bounds rule 2 with the code's worst sidelobe as a floor
     (adr_sidelobe_bound()), which the searcher computes on the first read.
     Read it here, so that sweep is this create's cost and never a stall
     inside push(); a one-epoch search never reads it. */
  if (obj->acq->search->coherent_bins > 1)
    (void)dp_correlator_dsl_db (obj->acq);
  obj->spc          = spc;
  obj->m            = m;
  obj->differential = differential;
  obj->chip_rate    = chip_rate;
  obj->symbol_rate  = symbol_rate;
  obj->cn0_dbhz     = cn0_dbhz;
  obj->pfa          = pfa;
  obj->pd           = pd;
  adr_enter (obj, ASYNC_DSSS_RX_SEARCHING);

  obj->lost_confirm_s       = lost_confirm_s;
  obj->reacquire            = reacquire;
  obj->refine               = refine;
  obj->lost_confirm_samples = lost_confirm_samples;
  obj->code_only_s          = code_only_s;
  obj->doppler_rate         = doppler_rate;
  /* Sample 0 is on every block boundary: at the search rate the first
     dwell owes nothing. A resampled search owes its filter's start-up. */
  obj->samples_in    = 0;
  obj->align_pending = obj->srch_warm > 0;

  obj->refine_max_error_db       = refine_max_error_db;
  obj->refine_samples_per_symbol = refine_samples_per_symbol;
  obj->refine_design_margin_db   = refine_design_margin_db;
  obj->refine_n_fft              = refine_n_fft;
  obj->refine_zero_pad           = refine_zero_pad;
  obj->refine_sequential         = refine_sequential;
  obj->refine_max_n_blocks       = refine_max_n_blocks;
  obj->refine_min_blocks         = ASYNC_DSSS_RX_REFINE_MIN_BLOCKS;
  obj->carrier_freq_hz           = carrier_freq_hz;

  /* tsamps (one code period, samples) is fixed for this object's entire
   * lifetime -- refine_segments is likewise fixed (depends only on
   * tsamps/refine_max_error_db, both construction-time invariants), so
   * both the shared carry scratch and refine_segments are computed ONCE
   * here, never per-rebuild. */
  /* The refine and track stages run in blocks of whole samples; a period
     that is not a whole number of them uses its floor (the partials close
     on the code NCO's phase, so a block need not be exactly a period), and
     the hand-off carries the exact fraction (adr_handover_phase()). */
  obj->tsamps = (size_t)floor ((double)code_len * spc + 1e-9);
  obj->refine_segments
      = dp_dll_lookback_segments (obj->tsamps, refine_max_error_db);
  obj->car_wiped_buf = dp_xmallocn (obj->tsamps, sizeof (*obj->car_wiped_buf));
  obj->car_carry_buf = dp_xmallocn (obj->tsamps, sizeof (*obj->car_carry_buf));
  obj->car_carry_len = 0;

  /* The fine hand-over, designed at the design point: the code's share of
     sign-changing boundaries (the replica's loss), the seed's error bound
     (below: the right emitter's seed error, capped by the span), the
     code error the search's grid leaves (half a search sample), a refine
     block's length, the receiver's pfa and ASYNC_DSSS_RX_FINE_P_HO. Off
     where the design refuses (M other than 2, a bound no divisor of
     refine_segments covers, or no dwell under the cap). */
  {
    size_t flips = 0;
    for (size_t i = 0; i < code_len; i++)
      flips += (code[i] & 1u) != (code[(i + 1) % code_len] & 1u);
    const double block_s = (double)refine_n_fft
                           / ((double)refine_samples_per_symbol * symbol_rate);
    /* The seed's error bound, for a hit on the right emitter: the data
       spectrum's main lobe (a dwell partly in data smears the peak to
       anywhere inside +-symbol_rate, its first nulls) plus the search's
       row, capped by the span's worst pair (the seed and the truth each
       inside +-doppler_uncertainty), which bounds any peak. Not the span
       alone: a span bound asks a wide span for a K no divisor of
       refine_segments covers, and turns the fine hand-over off where the
       coarse seed is worst (docs/design/async-dsss-receiver.md, "The
       seed's error bound"). A seed further off than this aliases by
       K / (M T); the tone check passes it, and the carrier loop never
       locks (doppler#3262). */
    double row = 0.0;
    (void)dp_correlator_get_doppler_res_hz (obj->acq, &row);
    const double e_seed = fmin (2.0 * doppler_uncertainty, symbol_rate + row);
    obj->fine_on
        = dp_async_dsss_receiver_fine_design (
              cn0_dbhz, chip_rate, code_len, (double)flips / (double)code_len,
              symbol_rate, m, obj->refine_segments, e_seed, doppler_rate,
              0.5 / (spc * (obj->srch_rs ? obj->srch_ratio : 1.0)), block_s,
              pfa, ASYNC_DSSS_RX_FINE_P_HO, refine_max_n_blocks, &obj->fine)
          == DP_OK;
    obj->fine_hz = obj->fine_snr_db = NAN;
    if (obj->fine_on)
      {
        const double T = (double)code_len / chip_rate;
        /* The dwell's sums and a period's spare. A refine that runs
           longer (CarrierAcquisition's dwell the longer) slides the
           buffer, so the estimate is the latest dwell's, never older. */
        obj->fine_cap
            = (size_t)ceil ((double)obj->fine.blocks * block_s / T + 1.0)
              * (size_t)obj->fine.k;
        obj->fine_buf = dp_xmallocn (obj->fine_cap, sizeof *obj->fine_buf);
        obj->fine_ppe
            = dp_xnn (dp_ppe_create (obj->fine_cap, obj->fine.max_rate));
      }
  }
  /* Placeholder chains (phase 0, no Doppler) -- always allocated, seeded
   * for real the moment a hit fires (fixed shape, same rationale
   * dsss_receiver_core.c's own state struct doc comment gives). */
  adr_build_refine_chain (obj, 0.0, 0.0, &obj->car_frozen, &obj->refine_dll,
                          &obj->refine_rc, &obj->ca, &obj->refine_dll_out_buf,
                          &obj->refine_dll_out_cap, &obj->refine_rc_out_buf,
                          &obj->refine_rc_out_cap);
  obj->refine_samples_fed = 0;

  adr_build_track_chain (obj, 0.0, 0.0, segments, sps, adr_m_out (sps),
                         &obj->car, &obj->dll, &obj->rc, &obj->rx);
  /* The hold's copy starts as the loop itself, as every rebuild leaves it
     (adr_rebuild_track_chain): a zeroed carrier_nda is no loop its own
     state check accepts, so a blob taken before the first seed would be
     refused on restore. */
  obj->car_held      = obj->car;
  obj->segments      = segments;
  obj->sps           = sps;
  obj->n             = adr_m_out (sps);
  obj->car_carry_len = 0; /* both placeholder builds share this buffer */
  adr_size_track_scratch (obj);

  obj->seed_chip_phase     = 0.0;
  obj->seed_doppler_hz_est = 0.0;
  obj->doppler_hz_est      = 0.0;
  obj->cn0_dbhz_est        = 0.0;

  /* Symbol-lock detector config (running state reset per track-chain build,
   * adr_reset_lock()). */
  obj->lock_alpha = 1.0 / (double)ASYNC_DSSS_RX_LOCK_DWELL;
  dp_lockdet_init (&obj->sym_lockdet, ASYNC_DSSS_RX_LOCK_UP,
                   ASYNC_DSSS_RX_LOCK_DOWN, ASYNC_DSSS_RX_LOCK_N_UP,
                   ASYNC_DSSS_RX_LOCK_N_DOWN);
  adr_reset_lock (obj);
  /* How far past its estimate the refine can decide, from the
     configuration (the field's comment has the terms). The refine's
     RateConverter is the same at every rebuild: its rate is fixed here.
     Allocated last, so everything the search and tracking use is where it
     was before the ring. */
  {
    const double period  = (double)code_len * spc;
    const double partial = period / (double)obj->refine_segments;
    obj->refine_hist_cap = (size_t)ceil (
        dp_RateConverter_get_count_lag (obj->refine_rc) * partial + partial
        + period + (double)obj->tsamps);
    obj->refine_hist
        = dp_xmallocn (obj->refine_hist_cap, sizeof *obj->refine_hist);
    obj->refine_hist_len  = 0;
    obj->refine_hist_head = 0;
  }
  return obj;
}

void
dp_async_dsss_receiver_destroy (dp_async_dsss_receiver_state_t *state)
{
  if (!state)
    return;
  adr_free_track_chain (state);
  adr_free_refine_chain (state);
  free (state->track_dll_out_buf);
  free (state->track_rc_out_buf);
  free (state->car_wiped_buf);
  free (state->car_carry_buf);
  dp_correlator_destroy (state->acq);
  dp_resamp_destroy (state->srch_rs);
  free (state->srch_buf);
  free (state->srch_hist);
  free (state->refine_hist);
  free (state->fine_buf);
  dp_ppe_destroy (state->fine_ppe);
  free (state->code);
  free (state);
}

static int adr_unheld (const dp_async_dsss_receiver_state_t *s);

void
dp_async_dsss_receiver_reset (dp_async_dsss_receiver_state_t *state)
{
  adr_restart_search (state, state->samples_in);
  /* Best-effort: on OOM, leave the current chains in place rather than
   * signal a failure this void-returning lifecycle function can't report
   * (matches dp_dsss_receiver_reset()'s own contract). */
  adr_rebuild_refine_chain (state, 0.0, 0.0);
  adr_rebuild_track_chain (state, 0.0, 0.0, state->segments, state->sps,
                           state->n);
  adr_enter (state, adr_unheld (state));
  state->seed_chip_phase     = 0.0;
  state->seed_doppler_hz_est = 0.0;
  state->doppler_hz_est      = 0.0;
  state->cn0_dbhz_est        = 0.0;
  state->refused_hits        = 0;
}

int
dp_async_dsss_receiver_set_telemetry (dp_async_dsss_receiver_state_t *state,
                                      dp_tlm_t *tlm, const char *prefix,
                                      uint32_t decim)
{
  if (!tlm)
    {
      adr_tlm_detach (state);
      state->tlm = NULL;
      return DP_OK;
    }
  const char  *p   = prefix ? prefix : "rx";
  const size_t len = strlen (p);
  if (decim == 0 || len >= DP_TLM_NAME_MAX)
    return DP_ERR_INVALID;
  /* A refusal leaves the receiver as it was, as a leaf's does: the dry run
     below moves every loop off whatever context it was on, so keep the
     request it replaces and put it back on any refusal. */
  dp_tlm_t *const old_tlm   = state->tlm;
  const uint32_t  old_decim = state->tlm_decim;
  char            old_prefix[DP_TLM_NAME_MAX];
  memcpy (old_prefix, state->tlm_prefix, sizeof old_prefix);
  memcpy (state->tlm_prefix, p, len + 1);
  state->tlm_decim = decim;
  /* Every name must fit before ANY registers on the caller's context. The
     loops join their own names, so the one test that cannot drift from what
     they will register is to let them: attach to a scratch context first,
     and to the caller's only if that took every probe. Without it a prefix
     that fits the carrier's names and not the demodulator's leaves the
     carrier's in the caller's table, unused (#1897). The scratch then
     knows every name the attach needs, so it also answers whether the
     caller's table has room for the ones it lacks. */
  /* A refusal says why. Out of memory -- the scratch itself, or a child
     whose own attach could not allocate (acq builds a scratch too) -- is
     DP_ERR_MEMORY; everything else is the argument's (#2846). */
  int       rc      = DP_ERR_INVALID;
  dp_tlm_t *scratch = dp_tlm_create (1);
  int       fits    = 0;
  if (scratch)
    {
      state->tlm  = scratch;
      const int a = adr_tlm_attach (state);
      fits        = a == DP_OK && dp_tlm_has_room_for (tlm, scratch);
      if (a == DP_ERR_MEMORY)
        rc = DP_ERR_MEMORY;
      adr_tlm_detach (state);
      dp_tlm_destroy (scratch);
    }
  else
    rc = DP_ERR_MEMORY;
  if (fits)
    {
      state->tlm  = tlm;
      const int a = adr_tlm_attach (state);
      if (a == DP_OK)
        return DP_OK;
      if (a == DP_ERR_MEMORY)
        rc = DP_ERR_MEMORY;
      /* Whole or nothing: a half-attached receiver would report some loops
         and not others with no way to tell from the context. */
      adr_tlm_detach (state);
    }
  /* Refused: restore the previous request. Its names are already in its
     context, so re-registering them takes no slot and cannot fail. */
  memcpy (state->tlm_prefix, old_prefix, sizeof old_prefix);
  state->tlm_decim = old_decim;
  state->tlm       = old_tlm;
  if (old_tlm)
    (void)adr_tlm_attach (state);
  return rc;
}

size_t
dp_async_dsss_receiver_steps_max_out (dp_async_dsss_receiver_state_t *state)
{
  (void)state;
  return 0; /* caller falls back to allocating x_len -- always sufficient,
               symbols emitted <= raw samples in. */
}

int
dp_async_dsss_receiver_seed_ok (const dp_async_dsss_receiver_state_t *state,
                                double chip_phase, double doppler_hz_est)
{
  /* A Doppler is reportable only below half the sample rate, which is what
     dp_correlator_carrier_freq_ok() relies on to keep doppler / carrier
     finite; a seed near 1e308 sent the hand-off's phase to NaN (doppler#2103).
     The comparisons are written so a NaN fails them. */
  return isfinite (chip_phase) && chip_phase >= 0.0
         && chip_phase < (double)state->code_len
         && fabs (doppler_hz_est)
                < 0.5 * state->chip_rate * (double)state->spc;
}

int
dp_async_dsss_receiver_seed (dp_async_dsss_receiver_state_t *state,
                             double chip_phase, double doppler_hz_est,
                             double cn0_dbhz_est)
{
  /* Only a receiver with nothing to lose takes a seed: a searching one.
   * Lost is deliberately refused too -- reset() releases it, so a stale
   * hold is never silently overwritten. */
  if (state->state != ASYNC_DSSS_RX_SEARCHING
      && state->state != ASYNC_DSSS_RX_IDLE)
    return DP_ERR_INVALID;
  if (!dp_async_dsss_receiver_seed_ok (state, chip_phase, doppler_hz_est))
    return DP_ERR_INVALID;

  /* With no refine the seed is the hand-over: the live chain is built at
     the seed's own sample, the next one this receiver is fed, from the
     seed's phase and Doppler -- the hand-over at zero samples of refine
     (adr_handover_phase()) -- and the refine chain is neither rebuilt nor
     run. */
  if (!state->refine)
    {
      adr_rebuild_track_chain (state, chip_phase, doppler_hz_est,
                               state->segments, state->sps, state->n);
      adr_enter (state, ASYNC_DSSS_RX_TRACKING);
      state->ho_src = ASYNC_DSSS_RX_HO_SEED; /* the seed is the hand-over */
    }
  else
    {
      adr_rebuild_refine_chain (state, chip_phase, doppler_hz_est);
      adr_enter (state, ASYNC_DSSS_RX_REFINING);
    }
  state->seed_chip_phase     = chip_phase;
  state->seed_doppler_hz_est = doppler_hz_est;
  state->doppler_hz_est      = doppler_hz_est;
  state->cn0_dbhz_est        = cn0_dbhz_est;
  return DP_OK;
}

/* The release rule of async-dsss-receiver.md section 10, run once per
 * tracking call: both lock flags down, without a break, for longer than
 * the confirm interval means the emitter is gone. One flag down is a
 * degrade and leaves the clock alone; either flag up restarts it.
 *
 * With reacquire the rule is the pool's (async-dsss-receiver.md section
 * 10): symbol lock alone, and the release resets the receiver to
 * searching instead of parking it in lost, so a receiver that is always
 * online finds the next signal without anyone calling reset(). */
static void
adr_release_clock (dp_async_dsss_receiver_state_t *s, size_t x_len)
{
  if (s->lost_confirm_samples == 0)
    return;
  const bool up = s->reacquire
                      ? s->sym_lockdet.locked
                      : (dp_dll_get_locked (s->dll) || s->sym_lockdet.locked);
  if (up)
    {
      s->both_down_samples = 0;
      return;
    }
  s->both_down_samples += x_len;
  if (s->both_down_samples <= s->lost_confirm_samples)
    return;
  if (s->reacquire)
    dp_async_dsss_receiver_reset (s);
  else
    adr_enter (s, ASYNC_DSSS_RX_LOST);
}

/* The state a receiver that holds nothing is in: its own search, or, pooled,
 * idle until the controller seeds it (docs/design/async-dsss-receiver.md
 * §6). */
static int
adr_unheld (const dp_async_dsss_receiver_state_t *s)
{
  return s->pooled ? ASYNC_DSSS_RX_IDLE : ASYNC_DSSS_RX_SEARCHING;
}

/* Pick k's phase carried from its sample (pick_at) to `to`, as adr_pick()
   carries a hit: dilated with a carrier, and exactly the stored phase when
   the two samples are the same. */
static double
adr_pick_phase_at (const dp_async_dsss_receiver_state_t *s, size_t k,
                   uint64_t to)
{
  const double shift = (double)(to - s->pick_at[k]) / s->spc;
  if (shift == 0.0)
    return s->pick_phase[k];
  double g = shift;
  if (s->carrier_freq_hz > 0.0)
    g *= 1.0 + s->pick_doppler[k] / s->carrier_freq_hz;
  return dp_fmod_pos (s->pick_phase[k] + g, (double)s->code_len);
}

/* A search-only receiver's dwell (set_search_only()): the pick for ranks 0
 * … pick_want - 1, each survivor kept with its phase at the sample after
 * this call. The hits' phases are carried there from the dwell's end, t,
 * by adr_pick()'s own shift; the exclusions, from the sample they were
 * set at (excl_at), are carried there too, nominally -- a holder's own
 * Doppler dilation over one call is a hundredth of a chip at 20 ppm, far
 * inside the one-chip key. */
static void
adr_collect_picks (dp_async_dsss_receiver_state_t *s,
                   const dp_correlator_hit_t *hits, size_t n_hits, double t)
{
  const double L   = (double)s->code_len;
  const double end = (double)s->samples_in; /* the sample after the call */
  /* The exclusions name the sample they were set at (excl_at), which a
     controller setting them before every call makes this call's first;
     one set calls earlier is carried over those calls too. */
  const double ex_sh = (end - (double)s->excl_at) / s->spc;
  double       saved[ASYNC_DSSS_RX_MAX_EXCLUSIONS];
  memcpy (saved, s->excl, sizeof saved);
  for (size_t k = 0; k < s->n_excl; k++)
    s->excl[k] = dp_fmod_pos (s->excl[k] + ex_sh, L);
  const size_t rank = s->rank;
  for (size_t r = 0; r < s->pick_want && s->n_picks < ASYNC_DSSS_RX_MAX_PEAKS;
       r++)
    {
      dp_correlator_handoff_t ho;
      s->rank = r;
      if (!adr_pick (s, hits, n_hits, (end - t) / s->spc, &ho))
        break;
      if (!dp_async_dsss_receiver_seed_ok (s, ho.chip_phase,
                                           ho.doppler_hz_est))
        {
          s->refused_hits++;
          continue;
        }
      /* A dwell before this one, not yet taken, may have picked the same
         signal: its slot takes this dwell's fresher estimate, so a list
         taken after several dwells holds each signal once. */
      size_t slot = s->n_picks;
      for (size_t q = 0; q < s->n_picks && slot == s->n_picks; q++)
        if (adr_same_code_phase (adr_pick_phase_at (s, q, s->samples_in),
                                 ho.chip_phase, s->code_len))
          slot = q;
      s->pick_phase[slot]   = ho.chip_phase;
      s->pick_doppler[slot] = ho.doppler_hz_est;
      s->pick_cn0[slot]     = ho.cn0_dbhz_est;
      s->pick_at[slot]      = s->samples_in; /* `end`'s sample */
      if (slot == s->n_picks)
        s->n_picks++;
    }
  s->rank = rank;
  memcpy (s->excl, saved, sizeof saved);
}

/* steps()'s body. A hit, a refused hand-over or a decided refine hands the
 * rest of the call to itself here, so a call may pass through several
 * states; the public steps() below reports once, after all of them. */
static size_t
adr_steps (dp_async_dsss_receiver_state_t *state, const float _Complex *x,
           size_t x_len, float _Complex *out, size_t max_out)
{
  if (x_len == 0)
    return 0;
  state->state_samples += x_len;

  if (state->state == ASYNC_DSSS_RX_LOST)
    state->both_down_samples += x_len; /* since the flags dropped */
  if (state->state == ASYNC_DSSS_RX_LOST)
    return 0; /* consumed and discarded: the feeding loop has no case */
  if (state->state == ASYNC_DSSS_RX_IDLE)
    return 0; /* a pooled receiver waits for the controller's seed */

  if (state->state == ASYNC_DSSS_RX_SEARCHING)
    {
      /* The search's samples: x itself when the caller feeds the search
       * rate, else x resampled to it (srch_rs). Every search sample below
       * is counted in srch_n, the search timebase, and a hit's position is
       * mapped back to the input with adr_search_to_input(). At the search
       * rate already the two timebases are one and this loop is exactly
       * the search it replaces: no sample moves, no phase is added. */
      const float _Complex *y  = x;
      size_t                ny = x_len;
      if (state->srch_rs)
        {
          const size_t need
              = (size_t)ceil ((double)x_len * state->srch_ratio) + 4;
          if (need > state->srch_cap)
            {
              free (state->srch_buf);
              state->srch_buf = dp_xmallocn (need, sizeof *state->srch_buf);
              state->srch_cap = need;
            }
          ny = dp_resamp_execute (state->srch_rs, x, x_len, state->srch_buf,
                                  state->srch_cap);
          y  = state->srch_buf;
        }
      const uint64_t in0 = state->samples_in - x_len; /* x[0]'s index */
      size_t         yp  = 0;
      while (yp < ny)
        {
          /* A restarted search first runs out to the block grid. */
          if (state->align_pending)
            {
              const size_t skip = adr_align_skip (state, ny - yp);
              yp += skip;
              state->srch_n += skip;
              if (yp == ny)
                break;
            }
          /* The engine's own position and the hit's, one timebase, and the
           * same fail-closed bound -- the rule and why are at
           * dsss_receiver_core.c's steps(), which hands off the same way. */
          const uint64_t      before = dp_correlator_position (state->acq);
          dp_correlator_hit_t hits[ASYNC_DSSS_RX_MAX_PEAKS];
          size_t n_hits = dp_correlator_push (state->acq, y + yp, ny - yp,
                                              hits, ASYNC_DSSS_RX_MAX_PEAKS);
          /* With room for a whole dwell's list the push reports ONE dwell,
             every peak sharing its samples_consumed, and may frame on past
             it to the next dwell boundary. The hand-off resumes at the
             dwell's end; a search that goes on resumes where the ENGINE
             stopped, dp_correlator_consumed(), so no sample is pushed twice.
           */
          const uint64_t at = n_hits ? hits[0].samples_consumed - before : 0;
          if (n_hits == 0 || at == 0 || at > (uint64_t)(ny - yp))
            {
              state->srch_n += ny - yp;
              break;
            }
          const size_t taken = dp_correlator_consumed (state->acq);

          /* A hit is a seed the object made for itself -- the same path an
           * outside detection takes. The list is filtered first
           * (adr_pick(): exclusions, duplicates, rank); a dwell with no
           * peak at this receiver's rank seeds nothing and the search goes
           * on. dp_correlator_build_handoff() folds the phase into [0,
           * code_len), but not the Doppler: a hit at or past fs/2 is outside
           * the seed domain. Such a hit is refused as a seed and counted in
           * refused_hits, and the search goes on, as it would after no
           * hit. */
          /* The dwell ends at search sample srch_n + at, which carries input
           * index t (adr_search_to_input()). The live chain starts at
           * ceil(t), reaching back into the history when that is behind
           * this call (it holds the resampler's whole lag, so the clamp is
           * a guard, not a path), and every phase is carried from t to it:
           * (i0 - t) / spc chips, which adr_pick() applies, dilated, to
           * each hit before it compares any. The gap is the resampler's
           * lag, a few samples, and 0 at the search rate. */
          const double t    = adr_search_to_input (state, state->srch_n + at);
          double       i0   = ceil (t - 1e-9);
          const double back = (double)in0 - (double)state->srch_hist_len;
          if (i0 < back)
            i0 = back;
          if (i0 > (double)state->samples_in)
            i0 = (double)state->samples_in;
          if (state->pick_want)
            {
              /* The controller's search: report, never seed (§5). */
              adr_collect_picks (state, hits, n_hits, t);
              state->srch_n += taken;
              yp += taken;
              continue;
            }
          dp_correlator_handoff_t ho;
          const int               picked
              = adr_pick (state, hits, n_hits, (i0 - t) / state->spc, &ho);
          if (!picked
              || !dp_async_dsss_receiver_seed_ok (state, ho.chip_phase,
                                                  ho.doppler_hz_est))
            {
              if (picked)
                state->refused_hits++;
              state->srch_n += taken;
              yp += taken;
              continue;
            }
          const double phase = ho.chip_phase;
          state->srch_n += ny - yp;
          /* Accepted: the domain was just checked, and the receiver is
             searching, so the seed cannot refuse for either reason. */
          (void)dp_async_dsss_receiver_seed (state, phase, ho.doppler_hz_est,
                                             ho.cn0_dbhz_est);
          if (i0 >= (double)in0)
            {
              const size_t off = (size_t)((uint64_t)i0 - in0);
              return adr_steps (state, x + off, x_len - off, out, max_out);
            }
          const size_t need = (size_t)(in0 - (uint64_t)i0);
          /* Trusted, like every allocation on this path: a sum that does
             not fit aborts, and so does the byte product (dp_xmallocn). */
          size_t join_len;
          if (!dp_size_add (need, x_len, &join_len))
            abort ();
          float _Complex *join = dp_xmallocn (join_len, sizeof *join);
          memcpy (join, state->srch_hist + state->srch_hist_len - need,
                  need * sizeof *join);
          memcpy (join + need, x, x_len * sizeof *join);
          const size_t n_out = adr_steps (state, join, join_len, out, max_out);
          free (join);
          return n_out;
        }
      adr_hist_push (state, x, x_len);
      return 0;
    }

  if (state->state == ASYNC_DSSS_RX_REFINING)
    {
      const uint64_t before = state->refine_samples_fed; /* x[0]'s index */
      state->refine_samples_fed += x_len;
      const size_t taken = adr_process_refine (state, x, x_len);

      dp_carrier_acq_state_t *ca = state->ca;
      if (!adr_refine_decided (state))
        {
          adr_refine_hist_push (state, x, x_len);
          return 0; /* still refining, no output yet */
        }

      double refined_doppler_hz_est
          = state->seed_doppler_hz_est
            + (ca->ready ? ca->residual_hz : 0.0); /* give-up: unrefined */
      int ho_src = ca->ready ? ASYNC_DSSS_RX_HO_DATA : ASYNC_DSSS_RX_HO_SEED;
      /* The fine estimate, when its check passes: the squared sums' tone
         is at M times the residual against the frozen carrier (the seed),
         located by PolynomialPhaseEstimator, in cycles a sum, at fine.k
         sums a code period. The check is the peak's peak-to-mean against
         the largest of n noise bins at the receiver's pfa. A refusal
         falls back to CarrierAcquisition's estimate, or the seed. */
      state->fine_hz = state->fine_snr_db = NAN;
      if (state->fine_on && state->fine_n >= 4)
        {
          const ppe_result_t r = dp_ppe_estimate (
              state->fine_ppe, state->fine_buf, state->fine_n);
          const double T = (double)state->code_len / state->chip_rate;
          /* The check's cells are the sums times the rate rows; the
             frequency handed over is the tone's at the buffer's end (the
             estimator's rate is the chirp's, cycles a sum squared, from
             the first sum). */
          const double thr = adr_fine_thr_db (
              state->fine_n * state->fine_ppe->n_rate, state->pfa);
          const double f_end
              = r.freq_norm + r.rate_norm * (double)(state->fine_n - 1);
          state->fine_hz
              = f_end * (double)state->fine.k / T / (double)state->m;
          state->fine_snr_db = r.snr_db;
          if (state->tlm)
            {
              dp_tlm_emit (state->tlm, state->tlm_fine_hz, state->fine_hz);
              dp_tlm_emit (state->tlm, state->tlm_fine_snr, r.snr_db);
              dp_tlm_emit (state->tlm, state->tlm_fine_n,
                           (double)state->fine_n);
            }
          if (r.snr_db >= thr)
            {
              refined_doppler_hz_est
                  = state->seed_doppler_hz_est + state->fine_hz;
              ho_src = ASYNC_DSSS_RX_HO_FINE;
            }
        }
      /* samples_consumed_refine: freq_refine.refine_seed_carrier_acq()'s
       * own elapsed-time formula, ported verbatim -- a resampled chain
       * has no exact raw-sample-to-block correspondence to track through
       * RateConverter's own filter delay; this approximation is already
       * validated to ~tens-of-Hz accuracy at the specification's own real rate
       * (see the plan doc). Uses `refine_n_fft` (the RAW per-block sample
       * count `dp_carrier_acq_create()` consumes per `n_blocks` increment)
       * -- NOT `ca->nfft` (the zero-padded PSD *transform* length,
       * `next_pow_two(refine_n_fft*refine_zero_pad)`, a different and much
       * larger number). */
      double target_rate
          = (double)state->refine_samples_per_symbol * state->symbol_rate;
      double elapsed_s
          = (double)ca->n_blocks * (double)state->refine_n_fft / target_rate;
      double   front_end_rate = state->chip_rate * (double)state->spc;
      uint64_t samples_consumed_refine
          = (uint64_t)llround (elapsed_s * front_end_rate);

      /* Round UP to a whole number of code periods (tsamps) -- critical,
       * not cosmetic: the live tracking chain below is seeded from
       * `seed_chip_phase`, the code phase at the ORIGINAL handoff (advanced
       * by the clock dilation over the refine, below), not wherever the
       * refine-stage Dll's own tracking drifted to. That reuse is only
       * valid if the live chain's first sample is an EXACT whole number of
       * code periods after the handoff -- one whole period is zero net
       * code-phase advance on the nominal clock, so the phase at any such
       * boundary is the handoff's plus the dilation alone. Python's own
       * e2e_acq_to_despreader.py relies on exactly
       * this (`n_epochs_used`'s own ceiling-to-whole-epoch rounding
       * before slicing `track_rx`); omitting it here was a real bug --
       * confirmed directly: without this rounding, the live chain is
       * seeded with a code phase valid for a DIFFERENT (non-epoch-
       * aligned) stream position than the one it's actually fed,
       * misaligning Dll's correlation from the very first sample
       * (total decode failure, reproduced even at Es/N0=30dB where
       * noise cannot be the explanation). */
      /* A period of code_len * spc samples, which need not be whole: the
         round-up lands on the first whole sample at or past that many
         whole periods, and adr_handover_phase() carries what the period's
         fraction leaves (nothing when it is whole, exactly as before). */
      const double period = (double)state->code_len * state->spc;
      const double n_periods_consumed
          = ceil ((double)samples_consumed_refine / period - 1e-9);
      samples_consumed_refine
          = (uint64_t)ceil (n_periods_consumed * period - 1e-9);

      /* The live chain starts there whatever the chunking (#2974). The
       * estimate ignores the refine chain's lag (the Dll's framing, the
       * RateConverter's), so the refine decides at D, up to that lag past
       * it -- 7 samples at 2.5 samples a chip, 231 at 3 samples a chip and
       * 3000 sym/s -- and the start can lie in input an earlier call
       * passed. The ring holds the last refine_hist_cap samples before
       * this call, the most the refine can decide past its estimate (the
       * field's comment derives it), so the start is reached at every
       * chunking. A start the input has not reached yet is waited for, the
       * refine decided and the receiver still refining.
       *
       * The fallback, for a refine whose lag the cap does not cover (a
       * refine Dll drifting on noise; no signal geometry measured): a
       * start before D - cap, the oldest sample held at every chunking,
       * moves on to the first ceil(k * period) at or past it, k whole
       * periods, the fraction carried in the phase. Where the cap covers the
       * lag that point is at or before the estimate, so the estimate stands. A
       * call that took nothing (the refine decided before it) holds a start at
       * or past its first sample, so it needs nothing older. */
      const uint64_t fed_total = state->refine_samples_fed;
      uint64_t       held      = before;
      if (taken)
        {
          const uint64_t d   = before + (uint64_t)taken;
          const uint64_t cap = (uint64_t)state->refine_hist_cap;
          /* The ring holds min(before, cap) samples behind x (set_state
             takes no other count), so a start at or past this is held. */
          held = d > cap ? d - cap : 0;
        }
      const uint64_t held_start = adr_period_start_at (period, held);
      if (samples_consumed_refine < held_start)
        samples_consumed_refine = held_start;
      if (samples_consumed_refine > fed_total)
        {
          adr_refine_hist_push (state, x, x_len);
          return 0; /* the live chain's first sample is still to come */
        }
      const size_t tail_len = (size_t)(fed_total - samples_consumed_refine);
      const float _Complex *tail;
      float _Complex       *join = NULL;
      if (tail_len > x_len)
        {
          /* Behind this call: the ring's newest samples, which end at
             x[0]. */
          const size_t need = tail_len - x_len;
          join              = dp_xmallocn (tail_len, sizeof *join);
          adr_refine_hist_tail (state, join, need);
          memcpy (join + need, x, x_len * sizeof *join);
          tail = join;
        }
      else
        tail = x + (x_len - tail_len);
      /* This call is the refine's too, so the ring keeps every refine
         sample, the hand-over's included: it always holds
         min(refine_samples_fed, cap), the one count set_state accepts. */
      adr_refine_hist_push (state, x, x_len);

      /* The code phase the live chain starts from: the seed's, ADVANCED by
       * the clock dilation over the refine. A whole number of periods is
       * zero net advance only on an undilated clock; at 20 ppm the code
       * runs 100 chips/s ahead, 1.2 chips over a 12 ms refine and 5 over
       * 53 ms, and a live Dll seeded 1.5 chips off is outside its pull-in
       * -- nothing downstream locked (doppler#1249). The refine-stage
       * Dll's own tracked phase is NOT the answer: at the floor it wanders
       * by 13 chips over the same 53 ms (measured), where the dilation
       * model from the refined Doppler is within a tenth of a chip. Off
       * when carrier_freq_hz == 0: no carrier, no dilation to model. */
      /* The live chain's Dll is built under dp_xnn, which aborts on a
         non-finite init phase or one outside [0, code_len): a refined value
         failing that takes the failed-refine path, the seed's own Doppler
         and phase (doppler#2103). What survives is then held to the seed
         domain, the same test a hit is (#2461 #122): a refined Doppler at or
         past fs/2 is no Doppler this receiver can report, so the hand-over
         is refused like a hit -- counted in refused_hits, the search reset,
         and the rest of this call searched. */
      double handover_chip_phase = adr_handover_phase (
          state, refined_doppler_hz_est, samples_consumed_refine);
      if (!isfinite (refined_doppler_hz_est) || !isfinite (handover_chip_phase)
          || handover_chip_phase < 0.0
          || handover_chip_phase >= (double)state->code_len)
        {
          refined_doppler_hz_est = state->seed_doppler_hz_est;
          ho_src                 = ASYNC_DSSS_RX_HO_SEED;
          handover_chip_phase    = adr_handover_phase (
              state, refined_doppler_hz_est, samples_consumed_refine);
        }
      if (!dp_async_dsss_receiver_seed_ok (state, handover_chip_phase,
                                           refined_doppler_hz_est))
        {
          /* Refused, as a hit outside the domain is. Not reset(): that
             would zero the refusal count this increments. The tail is the
             search's; entering searching restarts state_samples, so the
             recursion counts it once. */
          state->refused_hits++;
          /* The refused Doppler is not left readable: searching reads the
             estimates as reset() leaves them. */
          state->seed_doppler_hz_est = 0.0;
          state->doppler_hz_est      = 0.0;
          adr_restart_search (state, state->samples_in - tail_len);
          adr_enter (state, adr_unheld (state));
        }
      else
        {
          adr_rebuild_track_chain (state, handover_chip_phase,
                                   refined_doppler_hz_est, state->segments,
                                   state->sps, state->n);
          adr_enter (state, ASYNC_DSSS_RX_TRACKING);
          state->doppler_hz_est = refined_doppler_hz_est;
          state->ho_src         = ho_src;
        }
      const size_t n_out = adr_steps (state, tail, tail_len, out, max_out);
      free (join);
      return n_out;
    }

  size_t n_out = adr_track_chain (state, x, x_len, out, max_out);
  adr_release_clock (state, x_len);
  return n_out;
}

size_t
dp_async_dsss_receiver_steps (dp_async_dsss_receiver_state_t *state,
                              const float _Complex *x, size_t x_len,
                              float _Complex *out, size_t max_out)
{
  /* Counted up front: every tail adr_steps() recurses on ends where this
     call's input ends, so a tail of t samples starts at samples_in - t. */
  state->samples_in += x_len;
  /* The loops' NCO phases at this call's first sample, before it runs:
     the chain still holds car_carry_len samples it has not wiped, so the
     first new sample is that many past the NCOs' next one. */
  double ph_car = 0.0, ph_code = 0.0;
  if (state->tlm && x_len)
    {
      ph_car  = dp_carrier_nda_get_nco_phase_ahead (&state->car,
                                                    state->car_carry_len);
      ph_code = dp_dll_get_code_phase_ahead (state->dll, state->car_carry_len);
    }
  size_t n_out = adr_steps (state, x, x_len, out, max_out);
  /* The lifecycle, once a call and after it, on the block's stamp: where
     the receiver ended up, the Doppler its chain was built from, and the
     hits refused so far. An empty call changed nothing and reports nothing,
     which also keeps every probe within one record per input sample. */
  if (state->tlm && x_len)
    {
      dp_tlm_emit (state->tlm, state->tlm_state, (double)state->state);
      dp_tlm_emit (state->tlm, state->tlm_handover, state->doppler_hz_est);
      dp_tlm_emit (state->tlm, state->tlm_refused,
                   (double)state->refused_hits);
      dp_tlm_emit (state->tlm, state->tlm_ho_src, (double)state->ho_src);
      dp_tlm_emit (state->tlm, state->tlm_cn0, state->cn0_dbhz_est);
      dp_tlm_emit (state->tlm, state->tlm_phase_car, ph_car);
      dp_tlm_emit (state->tlm, state->tlm_phase_code, ph_code);
      dp_tlm_emit (state->tlm, state->tlm_release,
                   state->lost_confirm_samples
                       ? (double)state->both_down_samples
                             / (double)state->lost_confirm_samples
                       : 0.0);
    }
  return n_out;
}

int
dp_async_dsss_receiver_configure_search_raw (
    dp_async_dsss_receiver_state_t *state, size_t doppler_bins,
    size_t n_noncoh)
{
  const int rc = dp_correlator_configure_search_raw (state->acq, doppler_bins,
                                                     n_noncoh, NULL);
  /* An accepted search config drains the engine (dp_correlator_reset()), so
     its next dwell starts on the block grid like any restarted search's. */
  if (rc == 0)
    state->align_pending = true;
  return rc;
}

int
dp_async_dsss_receiver_set_threads (dp_async_dsss_receiver_state_t *state,
                                    int                             n)
{
  return dp_correlator_set_threads (state->acq, n);
}

int
dp_async_dsss_receiver_set_pooled (dp_async_dsss_receiver_state_t *state,
                                   bool                            on)
{
  state->pooled = on;
  if (on && state->state == ASYNC_DSSS_RX_SEARCHING)
    adr_enter (state, ASYNC_DSSS_RX_IDLE);
  else if (!on && state->state == ASYNC_DSSS_RX_IDLE)
    {
      adr_restart_search (state, state->samples_in);
      adr_enter (state, ASYNC_DSSS_RX_SEARCHING);
    }
  return DP_OK;
}

int
dp_async_dsss_receiver_set_search_only (dp_async_dsss_receiver_state_t *state,
                                        size_t n_picks)
{
  if (n_picks > ASYNC_DSSS_RX_MAX_PEAKS)
    return DP_ERR_INVALID;
  state->pick_want = n_picks;
  if (n_picks == 0)
    state->n_picks = 0;
  return DP_OK;
}

size_t
dp_async_dsss_receiver_take_picks (dp_async_dsss_receiver_state_t *state,
                                   double *chip_phase, double *doppler_hz,
                                   double *cn0_dbhz, size_t max)
{
  const size_t n = state->n_picks < max ? state->n_picks : max;
  for (size_t k = 0; k < n; k++)
    {
      /* Carried from its call's end to the sample after the last fed:
         nothing when it is taken right after that call. */
      chip_phase[k] = adr_pick_phase_at (state, k, state->samples_in);
      doppler_hz[k] = state->pick_doppler[k];
      cn0_dbhz[k]   = state->pick_cn0[k];
    }
  state->n_picks = 0;
  memset (state->pick_phase, 0, sizeof state->pick_phase);
  memset (state->pick_doppler, 0, sizeof state->pick_doppler);
  memset (state->pick_cn0, 0, sizeof state->pick_cn0);
  memset (state->pick_at, 0, sizeof state->pick_at);
  return n;
}

int
dp_async_dsss_receiver_set_rank (dp_async_dsss_receiver_state_t *state,
                                 size_t                          rank)
{
  if (rank >= ASYNC_DSSS_RX_MAX_PEAKS)
    return DP_ERR_INVALID;
  state->rank = rank;
  return DP_OK;
}

size_t
dp_async_dsss_receiver_get_rank (const dp_async_dsss_receiver_state_t *state)
{
  return state->rank;
}

int
dp_async_dsss_receiver_set_exclusions (dp_async_dsss_receiver_state_t *state,
                                       const double *phases, size_t phases_len)
{
  if (phases_len > ASYNC_DSSS_RX_MAX_EXCLUSIONS || (phases_len && !phases))
    return DP_ERR_INVALID;
  for (size_t i = 0; i < phases_len; i++)
    if (!isfinite (phases[i]))
      return DP_ERR_INVALID;
  /* Checked whole, then committed: a refusal leaves the list as it was. */
  const double cl = (double)state->code_len;
  for (size_t i = 0; i < phases_len; i++)
    state->excl[i] = dp_fmod_pos (phases[i], cl);
  for (size_t i = phases_len; i < ASYNC_DSSS_RX_MAX_EXCLUSIONS; i++)
    state->excl[i] = 0.0;
  state->n_excl  = phases_len;
  state->excl_at = state->samples_in; /* the sample they name */
  return DP_OK;
}

size_t
dp_async_dsss_receiver_get_n_exclusions (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->n_excl;
}

int
dp_async_dsss_receiver_get_threads (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->acq->search->threads;
}

size_t
dp_async_dsss_receiver_get_carrier_lock_n_noncoh (
    const dp_async_dsss_receiver_state_t *state)
{
  return (size_t)state->car_blk;
}

size_t
dp_async_dsss_receiver_get_coherent_bins (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->acq->search->coherent_bins;
}

int
dp_async_dsss_receiver_set_refine_min_blocks (
    dp_async_dsss_receiver_state_t *state, size_t n_blocks)
{
  state->refine_min_blocks = n_blocks;
  return DP_OK;
}

void
dp_async_dsss_receiver_configure_lock_raw (
    dp_async_dsss_receiver_state_t *state, double up_thresh,
    double down_thresh, size_t n_looks, double alpha, uint32_t n_up,
    uint32_t n_down)
{
  dp_dll_configure_lock_raw (state->dll, up_thresh, down_thresh, n_looks,
                             alpha, n_up, n_down);
}

/* The whole carrier the live chain holds, Hz: loop 1's (the pre-despread
 * carrier loop, in cycles per front-end sample) plus what loop 2 -- the
 * post-despread MpskReceiver, at sps * symbol_rate -- has taken up beyond
 * it, its loop-filter integrator. status() reports it and
 * configure_chain_raw() re-seeds a rebuilt chain from it (doppler#1261,
 * #2414): one sum, so the two cannot disagree. */
static double
adr_tracked_doppler_hz (const dp_async_dsss_receiver_state_t *s)
{
  return dp_carrier_nda_get_norm_freq (&s->car) * s->chip_rate * (double)s->spc
         + dp_mpsk_receiver_get_norm_freq (s->rx)
               * ((double)s->sps * s->symbol_rate);
}

int
dp_async_dsss_receiver_configure_chain_raw (
    dp_async_dsss_receiver_state_t *state, size_t segments, size_t sps, int n)
{
  /* The new grid's chain is checked as create() checked the first, before
     anything is rebuilt: a period past what the Dll's aid takes, or an `n`
     MpskReceiver refuses, would abort the rebuild under dp_xnn (doppler#2103,
     #2421). A negative `n` is refused before the cast. */
  if (n < 0 || !mpsk_rx_m_out_ok ((double)sps, (size_t)n, 0)
      || !adr_track_chain_ok (state->code_len, state->chip_rate,
                              state->symbol_rate, segments, sps, state->spc))
    return -1;

  /* Read before the rebuild, while sps is still the old grid's: the sum
     is loop 1 PLUS loop 2. Loop 2's residual alone (what this read before
     #2414) dropped loop 1's carrier, so a receiver tracking +2.4 kHz was
     rebuilt near 0 Hz and lost the signal. The phase is the stream's next
     sample's, not the Dll's: the rebuild drops the samples the old chain
     carried, so the new chain's first sample is the stream's next (#3063). */
  double chip_phase     = adr_live_chip_phase (state);
  double doppler_hz_now = adr_tracked_doppler_hz (state);

  adr_rebuild_track_chain (state, chip_phase, doppler_hz_now, segments, sps,
                           n);
  return 0;
}

int
dp_async_dsss_receiver_get_tracking (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->state == ASYNC_DSSS_RX_TRACKING;
}
int
dp_async_dsss_receiver_get_refining (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->state == ASYNC_DSSS_RX_REFINING;
}
async_dsss_receiver_status_t
dp_async_dsss_receiver_status (const dp_async_dsss_receiver_state_t *s)
{
  /* Where the emitter is NOW. Once tracking (and held where it was in
   * lost) the estimate is the WHOLE carrier: loop 1's, in cycles per
   * front-end sample, plus what loop 2 -- the post-despread MpskReceiver
   * loop, at sps * symbol_rate -- has taken up beyond it, its loop-filter
   * integrator, the same sum configure_chain_raw() re-seeds a rebuilt
   * chain from (doppler#1261). While refining the frozen carrier IS the
   * seed; searching has no emitter. */
  double fs = s->chip_rate * (double)s->spc;
  double doppler_hz;
  switch (s->state)
    {
    case ASYNC_DSSS_RX_TRACKING:
    case ASYNC_DSSS_RX_LOST:
      doppler_hz = adr_tracked_doppler_hz (s);
      break;
    case ASYNC_DSSS_RX_REFINING:
      doppler_hz = dp_costas_get_norm_freq (&s->car_frozen) * fs;
      break;
    default:
      doppler_hz = 0.0;
    }
  async_dsss_receiver_status_t r = {
    .state                 = s->state,
    .doppler_hz            = doppler_hz,
    .chip_phase            = dp_async_dsss_receiver_get_chip_phase (s),
    .code_rate             = dp_async_dsss_receiver_get_code_rate (s),
    .cn0_dbhz_est          = s->cn0_dbhz_est,
    .code_locked           = dp_async_dsss_receiver_get_code_locked (s),
    .locked                = dp_async_dsss_receiver_get_locked (s),
    .lock_metric           = s->lock_metric,
    .lock_threshold        = dp_async_dsss_receiver_get_lock_threshold (s),
    .car_last_error        = dp_async_dsss_receiver_get_car_last_error (s),
    .mpsk_last_error       = dp_async_dsss_receiver_get_mpsk_last_error (s),
    .state_samples         = s->state_samples,
    .both_down_samples     = s->both_down_samples,
    .refused_hits          = s->refused_hits,
    .carrier_lock_n_noncoh = s->car_blk,
  };
  return r;
}

int
dp_async_dsss_receiver_get_lost (const dp_async_dsss_receiver_state_t *state)
{
  return state->state == ASYNC_DSSS_RX_LOST;
}
double
dp_async_dsss_receiver_get_doppler_hz (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->doppler_hz_est;
}
double
dp_async_dsss_receiver_get_cn0_dbhz_est (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->cn0_dbhz_est;
}
size_t
dp_async_dsss_receiver_get_segments (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->segments;
}
size_t
dp_async_dsss_receiver_get_sps (const dp_async_dsss_receiver_state_t *state)
{
  return state->sps;
}
int
dp_async_dsss_receiver_get_n (const dp_async_dsss_receiver_state_t *state)
{
  return state->n;
}
double
dp_async_dsss_receiver_get_chip_phase (
    const dp_async_dsss_receiver_state_t *state)
{
  /* Refining has no live Dll yet: the signal it holds is the seed's, at the
     seed's phase advanced by the clock dilation since -- the one formula
     the hand-over starts the live chain from. A controller excludes this
     phase, so it must name the signal, not read 0 (#2525). */
  if (state->state == ASYNC_DSSS_RX_REFINING)
    return adr_handover_phase (state, state->seed_doppler_hz_est,
                               state->refine_samples_fed);
  return adr_live_chip_phase (state);
}
double
dp_async_dsss_receiver_get_code_rate (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_dll_get_code_rate (state->dll);
}
double
dp_async_dsss_receiver_get_lock (const dp_async_dsss_receiver_state_t *state)
{
  return dp_mpsk_receiver_get_lock (state->rx);
}
double
dp_async_dsss_receiver_get_norm_freq (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_mpsk_receiver_get_norm_freq (state->rx);
}

double
dp_async_dsss_receiver_get_nco_freq (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_mpsk_receiver_get_nco_freq (state->rx);
}

int
dp_async_dsss_receiver_get_locked (const dp_async_dsss_receiver_state_t *state)
{
  return state->sym_lockdet.locked;
}

double
dp_async_dsss_receiver_get_lock_metric (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->lock_metric;
}

double
dp_async_dsss_receiver_get_lock_threshold (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->sym_lockdet.up_thresh;
}

dp_lockdet_rule_t
dp_async_dsss_receiver_get_sym_lock_rule (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_lockdet_rule (&state->sym_lockdet);
}

dp_lockdet_rule_t
dp_async_dsss_receiver_get_car_lock_rule (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_carrier_nda_get_lock_rule (&state->car);
}

dp_lockdet_rule_t
dp_async_dsss_receiver_get_code_lock_rule (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_dll_get_lock_rule (state->dll);
}

dp_lockdet_rule_t
dp_async_dsss_receiver_get_demod_lock_rule (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_mpsk_receiver_get_lock_rule (state->rx);
}

dp_lockdet_rule_t
dp_async_dsss_receiver_get_demod_sync_lock_rule (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_mpsk_receiver_get_sync_lock_rule (state->rx);
}

int
dp_async_dsss_receiver_get_handover_source (
    const dp_async_dsss_receiver_state_t *state)
{
  return state->ho_src;
}

int
dp_async_dsss_receiver_get_code_locked (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_dll_get_locked (state->dll);
}

double
dp_async_dsss_receiver_get_car_last_error (
    const dp_async_dsss_receiver_state_t *state)
{
  return carrier_nda_loop_error (dp_carrier_nda_get_last_error (&state->car));
}

double
dp_async_dsss_receiver_get_car_nco_freq (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_carrier_nda_get_nco_freq (&state->car);
}

double
dp_async_dsss_receiver_get_mpsk_last_error (
    const dp_async_dsss_receiver_state_t *state)
{
  return dp_mpsk_receiver_get_last_error (state->rx);
}

size_t
dp_async_dsss_receiver_state_bytes (const dp_async_dsss_receiver_state_t *s)
{
  return sizeof (dp_state_hdr_t) + sizeof (async_dsss_receiver_extra_t)
         + dp_correlator_state_bytes (s->acq)
         + dp_costas_state_bytes (&s->car_frozen)
         + dp_dll_state_bytes (s->refine_dll)
         + dp_RateConverter_state_bytes (s->refine_rc)
         + dp_carrier_acq_state_bytes (s->ca)
         + dp_carrier_nda_state_bytes (&s->car)
         + dp_carrier_nda_state_bytes (&s->car_held)
         + dp_dll_state_bytes (s->dll) + dp_RateConverter_state_bytes (s->rc)
         + dp_mpsk_receiver_state_bytes (s->rx)
         + (s->srch_rs ? dp_resamp_state_bytes (s->srch_rs) : 0)
         + s->srch_hist_cap * sizeof (float _Complex)
         + s->tsamps * sizeof (float _Complex)
         + s->refine_hist_cap * sizeof (float _Complex)
         + s->fine_cap * sizeof (float _Complex);
}

void
dp_async_dsss_receiver_get_state (const dp_async_dsss_receiver_state_t *s,
                                  void                                 *blob)
{
  DP_GET_OPEN (ASYNC_DSSS_RECEIVER_STATE_MAGIC,
               ASYNC_DSSS_RECEIVER_STATE_VERSION,
               dp_async_dsss_receiver_state_bytes (s));
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
    .state_samples       = s->state_samples,
    .both_down_samples   = s->both_down_samples,
    .had_lock            = (uint8_t)(s->had_lock != 0),
    .car_coasting        = (uint8_t)(s->car_coasting != 0),
    .lock_num            = s->lock_num,
    .lock_den            = s->lock_den,
    .lock_metric         = s->lock_metric,
    .sym_lockdet         = s->sym_lockdet,
    .refused_hits        = s->refused_hits,
    .refine_min_blocks   = (uint64_t)s->refine_min_blocks,
    .samples_in          = s->samples_in,
    .align_pending       = (uint64_t)(s->align_pending != 0),
    .rank                = (uint64_t)s->rank,
    .n_excl              = (uint64_t)s->n_excl,
    .srch_origin         = s->srch_origin,
    .srch_n              = s->srch_n,
    .srch_hist_len       = (uint64_t)s->srch_hist_len,
    .pooled              = (uint64_t)(s->pooled != 0),
    .pick_want           = (uint64_t)s->pick_want,
    .n_picks             = (uint64_t)s->n_picks,
    .refine_hist_cap     = (uint64_t)s->refine_hist_cap,
    .refine_hist_len     = (uint64_t)s->refine_hist_len,
    .excl_at             = s->excl_at,
    .ho_src              = (uint64_t)s->ho_src,
    .fine_n              = (uint64_t)s->fine_n,
  };
  memcpy (extra.pick_at, s->pick_at, sizeof extra.pick_at);
  memcpy (extra.excl, s->excl, sizeof extra.excl); /* tail kept zeroed */
  memcpy (extra.pick_phase, s->pick_phase, sizeof extra.pick_phase);
  memcpy (extra.pick_doppler, s->pick_doppler, sizeof extra.pick_doppler);
  memcpy (extra.pick_cn0, s->pick_cn0, sizeof extra.pick_cn0);
  dp_w_bytes (&_w, &extra, sizeof extra);
  DP_W_CHILD (&_w, dp_correlator, s->acq);
  DP_W_CHILD (&_w, dp_costas, &s->car_frozen);
  DP_W_CHILD (&_w, dp_dll, s->refine_dll);
  DP_W_CHILD (&_w, dp_RateConverter, s->refine_rc);
  DP_W_CHILD (&_w, dp_carrier_acq, s->ca);
  DP_W_CHILD (&_w, dp_carrier_nda, &s->car);
  DP_W_CHILD (&_w, dp_carrier_nda, &s->car_held);
  DP_W_CHILD (&_w, dp_dll, s->dll);
  DP_W_CHILD (&_w, dp_RateConverter, s->rc);
  DP_W_CHILD (&_w, dp_mpsk_receiver, s->rx);
  /* The search resampler, when there is one: create fixed whether there
     is (the rates), so the blob's layout is this instance's. */
  if (s->srch_rs)
    DP_W_CHILD (&_w, dp_resamp, s->srch_rs);
  /* The hand-off history's live samples, then zeros. */
  dp_w_cf32 (&_w, s->srch_hist, s->srch_hist_len);
  dp_w_zeros (&_w,
              (s->srch_hist_cap - s->srch_hist_len) * sizeof (float _Complex));
  /* The carry's live samples, then zeros: the rest of the buffer is an
     earlier period's or dp_xmalloc's, not state (#2076's sibling). */
  dp_w_cf32 (&_w, s->car_carry_buf, s->car_carry_len);
  dp_w_zeros (&_w, (s->tsamps - s->car_carry_len) * sizeof (float _Complex));
  /* The hand-over's ring, oldest first, then zeros. */
  {
    const size_t cap   = s->refine_hist_cap;
    const size_t len   = s->refine_hist_len;
    const size_t start = (s->refine_hist_head + cap - len) % cap;
    const size_t first = cap - start < len ? cap - start : len;
    dp_w_cf32 (&_w, s->refine_hist + start, first);
    dp_w_cf32 (&_w, s->refine_hist, len - first);
    dp_w_zeros (&_w, (cap - len) * sizeof (float _Complex));
  }
  /* The fine hand-over's sums, then zeros. */
  dp_w_cf32 (&_w, s->fine_buf, s->fine_n);
  dp_w_zeros (&_w, (s->fine_cap - s->fine_n) * sizeof (float _Complex));
}

/* Is c the carrier loop create() builds for s (adr_build_track_chain)? The
   blob's reject keys for the carrier children. bn is compared to the rule's
   value: a caller that retuned the live loop's bandwidth through
   dp_carrier_nda_set_bn() (the pull-in harness's --bn-carrier) has a loop
   this receiver did not build, and its blob is refused. */
static int
adr_carrier_is_built (const dp_async_dsss_receiver_state_t *s,
                      const dp_carrier_nda_state_t         *c)
{
  return c->m == 2 && c->t_upd == (double)s->tsamps
         && c->den == CARRIER_NDA_DEN_UNIT
         && c->den_alpha == ASYNC_DSSS_RX_CAR_DEN_ALPHA
         && c->den_gamma == s->car_gamma && c->blk_len == s->car_blk
         && (s->car_blk == 0 || c->blk_pfa == ASYNC_DSSS_RX_CAR_LOCK_PFA)
         && c->bn == s->bn_carrier && c->zeta == 0.707;
}

/* Skip one child sub-blob on a peek reader, checking its envelope (magic,
   version, endian, size) as the child's own set_state would open: a
   clobbered header is refused here, before any child commits. The child's
   own set_state is the authority on its envelope; this restates its
   MAGIC and VERSION as a pre-check (no child exposes a validate-only
   entry), and must be kept in step with it. A payload the child refuses
   after its envelope passes is the composite case, doppler#2104. */
static int
adr_peek_child (dp_reader_t *pk, size_t bytes, uint32_t magic,
                uint16_t version)
{
  const void *c = dp_r_reserve (pk, bytes);
  return c && dp_state_validate (c, bytes, magic, version) == DP_OK;
}

int
dp_async_dsss_receiver_set_state (dp_async_dsss_receiver_state_t *s,
                                  const void                     *blob)
{
  DP_SET_OPEN (ASYNC_DSSS_RECEIVER_STATE_MAGIC,
               ASYNC_DSSS_RECEIVER_STATE_VERSION,
               dp_async_dsss_receiver_state_bytes (s));
  async_dsss_receiver_extra_t extra;
  dp_r_bytes (&_r, &extra, sizeof extra);
  if (extra.segments != (uint64_t)s->segments || extra.sps != (uint64_t)s->sps
      || extra.n != (uint64_t)s->n
      || extra.refine_segments != (uint64_t)s->refine_segments
      || extra.car_carry_len >= (uint64_t)s->tsamps
      || extra.state > ASYNC_DSSS_RX_LOST || extra.pooled > 1
      || extra.ho_src > ASYNC_DSSS_RX_HO_FINE || extra.fine_n > s->fine_cap
      || (extra.state == ASYNC_DSSS_RX_IDLE && !extra.pooled)
      || (extra.state == ASYNC_DSSS_RX_SEARCHING && extra.pooled)
      || extra.pick_want > ASYNC_DSSS_RX_MAX_PEAKS
      || extra.n_picks > ASYNC_DSSS_RX_MAX_PEAKS || extra.align_pending > 1
      || extra.rank >= ASYNC_DSSS_RX_MAX_PEAKS
      || extra.n_excl > ASYNC_DSSS_RX_MAX_EXCLUSIONS
      || extra.srch_origin > extra.samples_in
      || extra.srch_hist_len > (uint64_t)s->srch_hist_cap
      || extra.refine_hist_cap != (uint64_t)s->refine_hist_cap
      || extra.refine_hist_len
             != (extra.refine_samples_fed < extra.refine_hist_cap
                     ? extra.refine_samples_fed
                     : extra.refine_hist_cap))
    return DP_ERR_INVALID;
  /* The samples the exclusions and picks name are ones already fed, and an
     untaken slot's is zero, as take_picks() leaves it. */
  if (extra.excl_at > extra.samples_in)
    return DP_ERR_INVALID;
  for (size_t i = 0; i < ASYNC_DSSS_RX_MAX_PEAKS; i++)
    if (i < extra.n_picks ? extra.pick_at[i] > extra.samples_in
                          : extra.pick_at[i] != 0)
      return DP_ERR_INVALID;
  /* Only what set_exclusions() can leave: folded phases, a zeroed tail. */
  for (size_t i = 0; i < ASYNC_DSSS_RX_MAX_EXCLUSIONS; i++)
    if (i < extra.n_excl
            ? !(extra.excl[i] >= 0.0 && extra.excl[i] < (double)s->code_len)
            : extra.excl[i] != 0.0)
      return DP_ERR_INVALID;
  /* Only what the picks can hold: folded phases, the seed domain's
     Dopplers, a zeroed tail. */
  for (size_t i = 0; i < ASYNC_DSSS_RX_MAX_PEAKS; i++)
    if (i < extra.n_picks
            ? !dp_async_dsss_receiver_seed_ok (s, extra.pick_phase[i],
                                               extra.pick_doppler[i])
                  || !isfinite (extra.pick_cn0[i])
            : (extra.pick_phase[i] != 0.0 || extra.pick_doppler[i] != 0.0
               || extra.pick_cn0[i] != 0.0))
      return DP_ERR_INVALID;
  /* A reacquire receiver never enters lost (its release resets it to
     searching), so lost is no state it can be in, and nothing would take
     it out: refused, as a state the object cannot reach. */
  if (s->reacquire && extra.state == ASYNC_DSSS_RX_LOST)
    return DP_ERR_INVALID;
  /* Likewise refining, for a receiver built without the refine. */
  if (!s->refine && extra.state == ASYNC_DSSS_RX_REFINING)
    return DP_ERR_INVALID;
  /* The seed fields are only ever what seed() accepted (or the zeros reset
     leaves), so a blob outside the seed domain is forged. Refused here,
     before any child is decoded: a NaN phase reached dp_xnn in the refine
     chain's Dll create (doppler#2103, through set_state). */
  if (!dp_async_dsss_receiver_seed_ok (s, extra.seed_chip_phase,
                                       extra.seed_doppler_hz_est))
    return DP_ERR_INVALID;
  /* The two carrier children decode into copies and commit only when each
     is the loop create() builds: M = 2, an update a period, the unit
     denominator at the design point's gamma, the block lock create()
     sized, and the rule's bn and the build's zeta. Their configuration is
     not this blob's to change (car_gamma, car_blk and bn_carrier are
     create()'s, restored by it): a blob from a receiver built at another
     C/N0 has the same size and would restore a different loop, and one
     with an update a sample would run the control tsamps times too large.
     They are checked BEFORE any child commits, on a peek reader that also
     checks every other child's envelope on its way past, so a carrier the
     receiver did not build, or any child sub-blob with a clobbered header,
     is refused with the receiver as it was. A child whose envelope passes
     and whose own payload check refuses still commits the children ahead
     of it: the composite case, doppler#2104. */
  dp_carrier_nda_state_t car = s->car, held = s->car_held;
  {
    dp_reader_t pk = _r;
    if (!adr_peek_child (&pk, dp_correlator_state_bytes (s->acq),
                         CORRELATOR_STATE_MAGIC, CORRELATOR_STATE_VERSION)
        || !adr_peek_child (&pk, dp_costas_state_bytes (&s->car_frozen),
                            COSTAS_STATE_MAGIC, COSTAS_STATE_VERSION)
        || !adr_peek_child (&pk, dp_dll_state_bytes (s->refine_dll),
                            DLL_STATE_MAGIC, DLL_STATE_VERSION)
        || !adr_peek_child (&pk, dp_RateConverter_state_bytes (s->refine_rc),
                            RC_STATE_MAGIC, RC_STATE_VERSION)
        || !adr_peek_child (&pk, dp_carrier_acq_state_bytes (s->ca),
                            CARRIER_ACQ_STATE_MAGIC,
                            CARRIER_ACQ_STATE_VERSION))
      return DP_ERR_INVALID;
    DP_R_CHILD (&pk, dp_carrier_nda, &car);
    DP_R_CHILD (&pk, dp_carrier_nda, &held);
    if (!adr_carrier_is_built (s, &car) || !adr_carrier_is_built (s, &held))
      return DP_ERR_INVALID;
    if (!adr_peek_child (&pk, dp_dll_state_bytes (s->dll), DLL_STATE_MAGIC,
                         DLL_STATE_VERSION)
        || !adr_peek_child (&pk, dp_RateConverter_state_bytes (s->rc),
                            RC_STATE_MAGIC, RC_STATE_VERSION)
        || !adr_peek_child (&pk, dp_mpsk_receiver_state_bytes (s->rx),
                            MPSK_RECEIVER_STATE_MAGIC,
                            MPSK_RECEIVER_STATE_VERSION)
        || (s->srch_rs
            && !adr_peek_child (&pk, dp_resamp_state_bytes (s->srch_rs),
                                RESAMP_STATE_MAGIC, RESAMP_STATE_VERSION)))
      return DP_ERR_INVALID;
  }
  DP_R_CHILD (&_r, dp_correlator, s->acq);
  DP_R_CHILD (&_r, dp_costas, &s->car_frozen);
  DP_R_CHILD (&_r, dp_dll, s->refine_dll);
  DP_R_CHILD (&_r, dp_RateConverter, s->refine_rc);
  DP_R_CHILD (&_r, dp_carrier_acq, s->ca);
  /* The carrier pair, already decoded and checked: skipped here. */
  if (!dp_r_reserve (&_r, dp_carrier_nda_state_bytes (&s->car))
      || !dp_r_reserve (&_r, dp_carrier_nda_state_bytes (&s->car_held)))
    return DP_ERR_INVALID;
  s->car      = car;
  s->car_held = held;
  DP_R_CHILD (&_r, dp_dll, s->dll);
  DP_R_CHILD (&_r, dp_RateConverter, s->rc);
  DP_R_CHILD (&_r, dp_mpsk_receiver, s->rx);
  if (s->srch_rs)
    DP_R_CHILD (&_r, dp_resamp, s->srch_rs);
  if (s->srch_hist_cap)
    dp_r_cf32 (&_r, s->srch_hist, s->srch_hist_cap);
  dp_r_cf32 (&_r, s->car_carry_buf, s->tsamps);
  dp_r_cf32 (&_r, s->refine_hist, s->refine_hist_cap);
  if (s->fine_cap)
    dp_r_cf32 (&_r, s->fine_buf, s->fine_cap);
  s->fine_n              = (size_t)extra.fine_n;
  s->refine_hist_len     = (size_t)extra.refine_hist_len;
  s->refine_hist_head    = s->refine_hist_len % s->refine_hist_cap;
  s->state               = extra.state;
  s->seed_chip_phase     = extra.seed_chip_phase;
  s->seed_doppler_hz_est = extra.seed_doppler_hz_est;
  s->doppler_hz_est      = extra.doppler_hz_est;
  s->cn0_dbhz_est        = extra.cn0_dbhz_est;
  s->refine_samples_fed  = extra.refine_samples_fed;
  s->refine_periods      = 0; /* a diagnostic, not in the blob: restarts */
  s->car_carry_len       = (size_t)extra.car_carry_len;
  s->state_samples       = extra.state_samples;
  s->both_down_samples   = extra.both_down_samples;
  s->had_lock            = extra.had_lock;
  s->car_coasting        = extra.car_coasting;
  s->lock_num            = extra.lock_num;
  s->lock_den            = extra.lock_den;
  s->lock_metric         = extra.lock_metric;
  s->sym_lockdet         = extra.sym_lockdet;
  s->refused_hits        = extra.refused_hits;
  s->refine_min_blocks   = (size_t)extra.refine_min_blocks;
  s->samples_in          = extra.samples_in;
  s->rank                = (size_t)extra.rank;
  s->n_excl              = (size_t)extra.n_excl;
  memcpy (s->excl, extra.excl, sizeof s->excl);
  s->align_pending = extra.align_pending != 0;
  s->srch_origin   = extra.srch_origin;
  s->srch_n        = extra.srch_n;
  s->srch_hist_len = (size_t)extra.srch_hist_len;
  s->pooled        = extra.pooled != 0;
  s->pick_want     = (size_t)extra.pick_want;
  s->n_picks       = (size_t)extra.n_picks;
  memcpy (s->pick_phase, extra.pick_phase, sizeof s->pick_phase);
  memcpy (s->pick_doppler, extra.pick_doppler, sizeof s->pick_doppler);
  memcpy (s->pick_cn0, extra.pick_cn0, sizeof s->pick_cn0);
  memcpy (s->pick_at, extra.pick_at, sizeof s->pick_at);
  s->excl_at = extra.excl_at;
  s->ho_src  = (int)extra.ho_src;
  return DP_OK;
}
